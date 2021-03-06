﻿// Copyright 2012-2013 Alalf <alalf.iQLc_at_gmail.com>
//
// This file is part of SCFF-DirectShow-Filter(SCFF DSF).
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

/// @file scff_imaging/complex_layout.cc
/// scff_imaging::ComplexLayoutの定義

#include "scff_imaging/complex_layout.h"

#include "scff_imaging/debug.h"
#include "scff_imaging/utilities.h"
#include "scff_imaging/screen_capture.h"
#include "scff_imaging/scale.h"
#include "scff_imaging/padding.h"

namespace scff_imaging {

//=====================================================================
// scff_imaging::ComplexLayout
//=====================================================================

ComplexLayout::ComplexLayout(
    int element_count,
    const LayoutParameter (&parameters)[kMaxProcessorSize])
    : Layout(),
      element_count_(element_count),
      screen_capture_(nullptr) {
  DbgLog((kLogMemory, kTrace,
          TEXT("ComplexLayout: NEW(%d)"),
          element_count));
  // 配列の初期化
  for (int i = 0; i < kMaxProcessorSize; i++) {
    parameters_[i] = parameters[i];
    scale_[i] = nullptr;
    element_x_[i] = -1;    // ありえない値
    element_y_[i] = -1;    // ありえない値
  }
  // 明示的に初期化していない
  // captured_image_[kMaxProcessorSize]
  // converted_image_[kMaxProcessorSize]
  // draw_context_
  // background_color_
}

ComplexLayout::~ComplexLayout() {
  DbgLog((kLogMemory, kTrace,
          TEXT("ComplexLayout: DELETE")));
  // 管理しているインスタンスをすべて破棄
  // 破棄はプロセッサ→イメージの順
  if (screen_capture_ != nullptr) {
    delete screen_capture_;
  }
  for (int i = 0; i < kMaxProcessorSize; i++) {
    if (scale_[i] != nullptr) {
      delete scale_[i];
    }
  }
}

ErrorCodes ComplexLayout::InitByIndex(int index) {
  ASSERT(0 <= index && index < element_count_);
  if (!utilities::Contains(0, 0,
                           GetOutputImage()->width(),
                           GetOutputImage()->height(),
                           parameters_[index].bound_x,
                           parameters_[index].bound_y,
                           parameters_[index].bound_width,
                           parameters_[index].bound_height)) {
    return ErrorCodes::kComplexLayoutBoundError;
  }

  // 仮想パディングサイズの計算
  int virtual_padding_top = 0;
  int virtual_padding_bottom = 0;
  int virtual_padding_left = 0;
  int virtual_padding_right = 0;
  utilities::CalculatePaddingSize(
      parameters_[index].bound_width,
      parameters_[index].bound_height,
      parameters_[index].clipping_width,
      parameters_[index].clipping_height,
      parameters_[index].stretch,
      parameters_[index].keep_aspect_ratio,
      &virtual_padding_top, &virtual_padding_bottom,
      &virtual_padding_left, &virtual_padding_right);

  // 描画する原点の座標を計算
  element_x_[index] = parameters_[index].bound_x + virtual_padding_left;
  element_y_[index] = parameters_[index].bound_y + virtual_padding_top;

  // パディング分だけサイズを小さくする
  const int element_width =
      parameters_[index].bound_width -
          (virtual_padding_left + virtual_padding_right);
  const int element_height =
      parameters_[index].bound_height -
          (virtual_padding_top + virtual_padding_bottom);

  //-------------------------------------------------------------------
  // 初期化の順番はイメージ→プロセッサの順
  //-------------------------------------------------------------------
  // Image
  //-------------------------------------------------------------------
  // ScreenCaptureから取得した変換処理前のイメージ
  const ErrorCodes error_captured_image =
      captured_image_[index].Create(ImagePixelFormats::kRGB0,
                                    parameters_[index].clipping_width,
                                    parameters_[index].clipping_height);
  if (error_captured_image != ErrorCodes::kNoError) {
    return error_captured_image;
  }

  // SWScaleで拡大縮小ピクセルフォーマット変換を行った後のイメージ
  const ErrorCodes error_converted_image =
        converted_image_[index].Create(GetOutputImage()->pixel_format(),
                                       element_width,
                                       element_height);
  if (error_converted_image != ErrorCodes::kNoError) {
    return error_converted_image;
  }
  //-------------------------------------------------------------------
  // Processor
  //-------------------------------------------------------------------
  // 拡大縮小ピクセルフォーマット変換
  Scale *scale = new Scale(parameters_[index].swscale_config);
  scale->SetInputImage(&(captured_image_[index]));
  scale->SetOutputImage(&(converted_image_[index]));
  const ErrorCodes error_scale_init = scale->Init();
  if (error_scale_init != ErrorCodes::kNoError) {
    delete scale;
    return error_scale_init;
  }
  scale_[index] = scale;
  //-------------------------------------------------------------------

  return ErrorCodes::kNoError;
}

//-------------------------------------------------------------------

ErrorCodes ComplexLayout::Init() {
  DbgLog((kLogTrace, kTraceInfo,
          TEXT("ComplexLayout: Init(%d)"),
          element_count_));

  // DrawUtilsが使えるフォーマットでなければComplexLayoutは使えない
  if (!utilities::CanUseDrawUtils(GetOutputImage()->pixel_format())) {
    return ErrorOccured(ErrorCodes::kComplexLayoutInvalidPixelFormatError);
  }

  // 要素を初期化
  for (int i = 0; i < element_count_; i++) {
    const ErrorCodes error_element = InitByIndex(i);
    if (error_element != ErrorCodes::kNoError) {
      // 一つでも失敗したらエラー扱い
      return ErrorOccured(error_element);
    }
  }

  //-------------------------------------------------------------------
  // Processor
  //-------------------------------------------------------------------
  // スクリーンキャプチャ
  ScreenCapture *screen_capture = new ScreenCapture(
      !utilities::IsTopdownPixelFormat(GetOutputImage()->pixel_format()),
      element_count_, parameters_);
  for (int i = 0; i < element_count_; i++) {
    screen_capture->SetOutputImage(&(captured_image_[i]), i);
  }
  const ErrorCodes error_screen_capture = screen_capture->Init();
  if (error_screen_capture != ErrorCodes::kNoError) {
    delete screen_capture;
    return ErrorOccured(error_screen_capture);
  }
  screen_capture_ = screen_capture;
  //-------------------------------------------------------------------

  // 描画用コンテキストの初期化
  const int error_init =
      ff_draw_init(&draw_context_,
                   GetOutputImage()->av_pixel_format(),
                   0);
  ASSERT(error_init == 0);

  // 真っ黒に設定
  uint8_t rgba_background_color[4] = {0};
  ff_draw_color(&draw_context_,
                &background_color_,
                rgba_background_color);

  return InitDone();
}

ErrorCodes ComplexLayout::Run() {
  if (GetCurrentError() != ErrorCodes::kNoError) {
    // 何かエラーが発生している場合は何もしない
    return GetCurrentError();
  }

  // まとめてスクリーンキャプチャ
  const ErrorCodes error_screen_capture = screen_capture_->Run();
  if (error_screen_capture != ErrorCodes::kNoError) {
    return ErrorOccured(error_screen_capture);
  }

  // Scaleを利用して変換
  // すこしでもCacheヒット率をあげるべく逆順に
  for (int i = element_count_ - 1; i >= 0; i--) {
    const ErrorCodes error_scale = scale_[i]->Run();
    if (error_scale != ErrorCodes::kNoError) {
      return ErrorOccured(error_scale);
    }
  }

  // 背景描画
  ff_fill_rectangle(&draw_context_, &background_color_,
                    GetOutputImage()->avpicture()->data,
                    GetOutputImage()->avpicture()->linesize,
                    0, 0,
                    GetOutputImage()->width(),
                    GetOutputImage()->height());

  // 要素を順番に描画
  for (int i = 0; i < element_count_; i++) {
    ff_copy_rectangle2(&draw_context_,
                       GetOutputImage()->avpicture()->data,
                       GetOutputImage()->avpicture()->linesize,
                       converted_image_[i].avpicture()->data,
                       converted_image_[i].avpicture()->linesize,
                       element_x_[i], element_y_[i],
                       0, 0,
                       converted_image_[i].width(),
                       converted_image_[i].height());
  }

  // エラー発生なし
  return GetCurrentError();
}
}   // namespace scff_imaging
