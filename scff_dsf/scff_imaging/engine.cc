﻿// Copyright 2012 Alalf <alalf.iQLc_at_gmail.com>
//
// This file is part of SCFF-DirectShow-Filter.
//
// SCFF DSF is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SCFF DSF is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SCFF DSF.  If not, see <http://www.gnu.org/licenses/>.

/// @file scff_imaging/engine.cc
/// scff_imaging::Engineの定義

#include "scff_imaging/engine.h"

#include <ctime>

#include "scff_imaging/debug.h"
#include "scff_imaging/avpicture_image.h"
#include "scff_imaging/splash_screen.h"
#include "scff_imaging/native_layout.h"
#include "scff_imaging/complex_layout.h"
#include "scff_imaging/request.h"
#include "scff_imaging/utilities.h"

namespace {

/// 指定されたAVPictureImageを黒で塗りつぶす
void Clear(scff_imaging::AVPictureImage *image) {
  if (!scff_imaging::utilities::CanUseDrawUtils(image->pixel_format())) {
    // 塗りつぶせなければなにもしない
    return;
  }

  FFDrawContext draw_context;
  FFDrawColor padding_color;

  // パディング用のコンテキストの初期化
  const int error_init =
      ff_draw_init(&draw_context,
                   image->av_pixel_format(),
                   0);
  ASSERT(error_init == 0);

  // パディング用のカラーを真っ黒に設定
  uint8_t rgba_padding_color[4] = {0};
  ff_draw_color(&draw_context,
                &padding_color,
                rgba_padding_color);

  ff_fill_rectangle(&draw_context, &padding_color,
                    image->avpicture()->data,
                    image->avpicture()->linesize,
                    0,
                    0,
                    image->width(),
                    image->height());
}
}   // namespace

namespace scff_imaging {

//=====================================================================
// scff_imaging::Engine
//=====================================================================

Engine::Engine(ImagePixelFormat output_pixel_format,
               int output_width, int output_height, double output_fps)
    : CAMThread(),
      Layout(),
      output_pixel_format_(output_pixel_format),
      output_width_(output_width),
      output_height_(output_height),
      output_fps_(output_fps),
      layout_(nullptr),
      layout_error_code_(ErrorCode::kProcessorUninitializedError),
      last_update_image_(ImageIndex::kFront) {
  MyDbgLog((LOG_MEMORY, kDbgNewDelete,
          TEXT("Engine: NEW(%d, %d, %d, %.1f)"),
          output_pixel_format, output_width, output_height, output_fps));
  // 明示的に初期化していない
  // front_image_
  // back_image_
  // splash_image_
}

Engine::~Engine() {
  MyDbgLog((LOG_MEMORY, kDbgNewDelete,
          TEXT("Engine: DELETE")));

  /// @attention enum->DWORD
  CallWorker(static_cast<DWORD>(RequestType::kStop));
  CallWorker(static_cast<DWORD>(RequestType::kResetLayout));
  CallWorker(static_cast<DWORD>(RequestType::kExit));
}

//---------------------------------------------------------------------
// Processor
//---------------------------------------------------------------------

ErrorCode Engine::Init() {
  MyDbgLog((LOG_TRACE, kDbgImportant,
          TEXT("Engine: Init")));

  //-------------------------------------------------------------------
  // 初期化の順番はイメージ→プロセッサの順
  //-------------------------------------------------------------------
  // Image
  //-------------------------------------------------------------------
  // フロントイメージ
  const ErrorCode error_front_image =
      front_image_.Create(output_pixel_format_,
                          output_width_,
                          output_height_);
  if (error_front_image != ErrorCode::kNoError) {
    return ErrorOccured(error_front_image);
  }
  // バックイメージ
  const ErrorCode error_back_image =
      back_image_.Create(output_pixel_format_,
                         output_width_,
                         output_height_);
  if (error_back_image != ErrorCode::kNoError) {
    return ErrorOccured(error_back_image);
  }
  // スプラッシュイメージ
  const ErrorCode error_splash_image =
      splash_image_.Create(output_pixel_format_,
                           output_width_,
                           output_height_);
  if (error_splash_image != ErrorCode::kNoError) {
    return ErrorOccured(error_splash_image);
  }

  // 一時的にスプラッシュスクリーンプロセッサを作ってイメージを生成しておく
  SplashScreen splash_screen;
  splash_screen.SetOutputImage(&splash_image_);
  const ErrorCode error_splash_screen = splash_screen.Init();
  if (error_splash_screen != ErrorCode::kNoError) {
    return ErrorOccured(error_splash_screen);
  }
  const ErrorCode error_splash_image_pull = splash_screen.Run();
  if (error_splash_image_pull != ErrorCode::kNoError) {
    return ErrorOccured(error_splash_image_pull);
  }

  //-------------------------------------------------------------------
  // Processor
  //-------------------------------------------------------------------
  // nop
  //-------------------------------------------------------------------

  // スレッド作成
  Create();
  CallWorker(static_cast<DWORD>(RequestType::kResetLayout));

  // 作成成功
  return InitDone();
}

ErrorCode Engine::Accept(Request *request) {
  // 何かエラーが発生している場合は何もしない
  if (GetCurrentError() != ErrorCode::kNoError) {
    return GetCurrentError();
  }

  // NULLリクエスト(何もしない)ならば、何もしない
  if (request == nullptr) {
    return GetCurrentError();
  }

  MyDbgLog((LOG_TRACE, kDbgImportant,
          TEXT("Engine: Accept")));

  // リクエストが送られてきているのならば、
  // thisを渡して処理を任せる(ダブルディスパッチ)
  // レイアウトエラーの設定はリクエストハンドラの中で行われているので
  // ここではチェックしない
  request->SendTo(this);

  /// @attention 現状、Chain of Resiposibilityはない＝
  ///            下位のプロセッサへリクエストは送らない
  return GetCurrentError();
}

//-------------------------------------------------------------------

/// @attention エラー発生中に追加の処理を行うのはEngineだけ
ErrorCode Engine::CopyFrontImage(BYTE *sample, DWORD data_size) {
  /// @attention processorのポインタがnullptrであることはエラーではない

  // Engine自体にエラーが発生していたら0クリア
  if (GetCurrentError() != ErrorCode::kNoError) {
    // Splashすら表示できない状態である可能性がある
    ZeroMemory(sample, data_size);
    return GetCurrentError();
  }

  // layout_にエラーが発生していたらスプラッシュを書く
  if (GetCurrentLayoutError() != ErrorCode::kNoError) {
    ASSERT(data_size == utilities::CalculateImageSize(splash_image_));
    avpicture_layout(splash_image_.avpicture(),
                     splash_image_.av_pixel_format(),
                     splash_image_.width(),
                     splash_image_.height(),
                     sample, data_size);
    return GetCurrentError();
  }

  // sampleにコピー
  if (last_update_image_ == ImageIndex::kFront) {
    ASSERT(data_size == utilities::CalculateImageSize(front_image_));
    avpicture_layout(front_image_.avpicture(),
                     front_image_.av_pixel_format(),
                     front_image_.width(),
                     front_image_.height(),
                     sample, data_size);
  } else if (last_update_image_ == ImageIndex::kBack) {
    ASSERT(data_size == utilities::CalculateImageSize(back_image_));
    avpicture_layout(back_image_.avpicture(),
                     back_image_.av_pixel_format(),
                     back_image_.width(),
                     back_image_.height(),
                     sample, data_size);
  }

  return GetCurrentError();
}


//-------------------------------------------------------------------
// リクエストハンドラ
//-------------------------------------------------------------------

void Engine::ResetLayout() {
  /// @attention enum->DWORD
  CallWorker(static_cast<DWORD>(RequestType::kStop));
  CallWorker(static_cast<DWORD>(RequestType::kResetLayout));
  CallWorker(static_cast<DWORD>(RequestType::kRun));
}

void Engine::SetNativeLayout() {
  /// @attention enum->DWORD
  CallWorker(static_cast<DWORD>(RequestType::kStop));
  CallWorker(static_cast<DWORD>(RequestType::kSetNativeLayout));
  CallWorker(static_cast<DWORD>(RequestType::kRun));
}

void Engine::SetComplexLayout() {
  /// @attention enum->DWORD
  CallWorker(static_cast<DWORD>(RequestType::kStop));
  CallWorker(static_cast<DWORD>(RequestType::kSetComplexLayout));
  CallWorker(static_cast<DWORD>(RequestType::kRun));
}

void Engine::SetLayoutParameters(
    int element_count,
    const LayoutParameter (&parameters)[kMaxProcessorSize]) {
  CAutoLock lock(&m_WorkerLock);
  element_count_ = element_count;
  for (int i = 0; i < kMaxProcessorSize; i++) {
    parameters_[i] = parameters[i];
    if (utilities::IsTopdownPixelFormat(output_pixel_format_)) {
      // * Topdownピクセルフォーマットの場合はbound_yの値を補正する
      // まずbound_yは左上のy座標になっているので、左下のy座標にする(y+height)
      // 左下のy座標は左上原点の座標系になっているので、左下原点の座標に直す
      parameters_[i].bound_y =
          output_height_ -
              (parameters_[i].bound_y + parameters_[i].bound_height);
    }
  }
}

//===================================================================
// キャプチャスレッド関連
//===================================================================

void Engine::DoResetLayout() {
  MyDbgLog((LOG_MEMORY, kDbgNewDelete,
          TEXT("Engine: Reset Layout")));

  // 解放+0クリア
  if (layout_ != nullptr) {
    delete layout_;
    layout_ = nullptr;
  }
  // 未初期化
  CAutoLock lock(&m_WorkerLock);
  layout_error_code_ = ErrorCode::kProcessorUninitializedError;
}

void Engine::DoSetNativeLayout() {
  // 現在のプロセッサは必要ないので削除
  DoResetLayout();

  //-------------------------------------------------------------------
  NativeLayout *native_layout = new NativeLayout(parameters_[0]);
  native_layout->SetOutputImage(&front_image_);
  const ErrorCode error_layout = native_layout->Init();
  if (error_layout != ErrorCode::kNoError) {
    // 失敗
    delete native_layout;
    LayoutErrorOccured(error_layout);
  } else {
    // 成功
    layout_ = native_layout;
    LayoutInitDone();
  }
}

void Engine::DoSetComplexLayout() {
  // 現在のプロセッサは必要ないので削除
  DoResetLayout();

  //-------------------------------------------------------------------
  ComplexLayout *complex_layout =
      new ComplexLayout(element_count_, parameters_);
  complex_layout->SetOutputImage(&front_image_);
  const ErrorCode error_layout = complex_layout->Init();
  if (error_layout != ErrorCode::kNoError) {
    // 失敗
    delete complex_layout;
    LayoutErrorOccured(error_layout);
  } else {
    // 成功
    layout_ = complex_layout;
    LayoutInitDone();
  }
}

void Engine::DoLoop() {
  DWORD request;
  const clock_t output_interval =
      static_cast<clock_t>((1 / output_fps_) * CLOCKS_PER_SEC);
  clock_t last_update = ::clock();

  do {
    while (!CheckRequest(&request)) {
      Update();
      const clock_t end_update = ::clock();
      const clock_t update_interval = end_update - last_update;
      const clock_t delta = output_interval - update_interval;
      if (delta > 0) {
        ::Sleep(static_cast<DWORD>(delta * MILLISECONDS / CLOCKS_PER_SEC));
      } else {
        MyDbgLog((LOG_TRACE, kDbgRare,
                TEXT("Engine: Drop Frame")));
      }
      last_update = ::clock();
    }

    /// @attention enum->DWORD
    if (request == static_cast<DWORD>(RequestType::kRun)) {
      Reply(NOERROR);
    }
  } while (request != static_cast<DWORD>(RequestType::kStop));
}

DWORD Engine::ThreadProc() {
  HRESULT result = ERROR;
  RequestType request = RequestType::kInvalid;

  do {
    DWORD actual_request = GetRequest();
    /// @warning DWORD->enum
    request = static_cast<RequestType>(actual_request);

    switch (request) {
      case RequestType::kResetLayout: {
        DoResetLayout();
        Reply(NOERROR);
        break;
      }
      case RequestType::kSetNativeLayout: {
        DoSetNativeLayout();
        Reply(NOERROR);
        break;
      }
      case RequestType::kSetComplexLayout: {
        DoSetComplexLayout();
        Reply(NOERROR);
        break;
      }
      case RequestType::kRun: {
        Reply(NOERROR);
        DoLoop();
        break;
      }
      case RequestType::kStop:
      case RequestType::kExit: {
        Reply(NOERROR);
        break;
      }
    }
  } while (request != RequestType::kExit);

  return 0;
}

ErrorCode Engine::Run() {
  ASSERT(layout_ != nullptr);
  const ErrorCode error = layout_->Run();
  if (error != ErrorCode::kNoError) {
    /// @attention layout_でエラーが発生してもEngine自体はエラー状態ではない
    LayoutErrorOccured(error);
  }
  return GetCurrentError();
}

void Engine::Update() {
  if (GetCurrentLayoutError() != ErrorCode::kNoError) {
    return;
  }

  if (last_update_image_ == ImageIndex::kFront) {
    layout_->SwapOutputImage(&back_image_);
    Run();
    last_update_image_ = ImageIndex::kBack;
  } else if (last_update_image_ == ImageIndex::kBack) {
    layout_->SwapOutputImage(&front_image_);
    Run();
    last_update_image_ = ImageIndex::kFront;
  }
}

ErrorCode Engine::LayoutInitDone() {
  CAutoLock lock(&m_WorkerLock);
  ASSERT(layout_error_code_ == ErrorCode::kProcessorUninitializedError);
  if (layout_error_code_ == ErrorCode::kProcessorUninitializedError) {
    layout_error_code_ = ErrorCode::kNoError;

    // ここで一回FrontImage/BackImageを黒で塗りつぶす
    Clear(&front_image_);
    Clear(&back_image_);
  }
  return layout_error_code_;
}

ErrorCode Engine::LayoutErrorOccured(ErrorCode error_code) {
  CAutoLock lock(&m_WorkerLock);
  if (error_code != ErrorCode::kNoError) {
    // 後からkNoErrorにしようとしてもできない
    // ASSERT(false);
    MyDbgLog((LOG_TRACE, kDbgImportant,
            TEXT("Engine: Layout Error Occured(%d)"),
            error_code));
    layout_error_code_ = error_code;
  }
  return layout_error_code_;
}

ErrorCode Engine::GetCurrentLayoutError() {
  CAutoLock lock(&m_WorkerLock);
  return layout_error_code_;
}

}   // namespace scff_imaging