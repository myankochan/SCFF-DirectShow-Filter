﻿
// Copyright 2012 Alalf <alalf.iQLc_at_gmail.com>
//
// This file is part of SCFF DSF.
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

/// @file scff-imaging/native-layout.cc
/// @brief scff_imaging::NativeLayoutの定義

#include "scff-imaging/native-layout.h"

#include "scff-imaging/debug.h"
#include "scff-imaging/utilities.h"
#include "scff-imaging/screen-capture.h"
#include "scff-imaging/scale.h"
#include "scff-imaging/padding.h"

namespace scff_imaging {

//=====================================================================
// scff_imaging::NativeLayout
//=====================================================================

// コンストラクタ
NativeLayout::NativeLayout(
    const LayoutParameter &parameter)
    : Processor<void, AVPictureImage>(),
      parameter_(parameter),
      screen_capture_(0),   // NULL
      scale_(0),            // NULL
      padding_(0) {         // NULL
  MyDbgLog((LOG_MEMORY, kDbgNewDelete,
          TEXT("NativeLayout: NEW(%dx%d)"),
          parameter_.clipping_width,
          parameter_.clipping_height));
  // 明示的に初期化していない
  // captured_image_
  // converted_image_
}

// デストラクタ
NativeLayout::~NativeLayout() {
  MyDbgLog((LOG_MEMORY, kDbgNewDelete,
          TEXT("NativeLayout: DELETE")));
  // 管理しているインスタンスをすべて破棄
  // 破棄はプロセッサ→イメージの順
  if (screen_capture_ != 0) {
    delete screen_capture_;
  }
  if (scale_ != 0) {  // NULL
    delete scale_;
  }
  if (padding_ != 0) {  // NULL
    delete padding_;
  }
}

// 設定されたOutputImageはPadding可能か？
bool NativeLayout::CanUsePadding() const {
  /// @warning 2012/05/08現在drawutilsはPlaner Formatにしか対応していない
  switch (GetOutputImage()->pixel_format()) {
  case kI420:
  case kRGB0:
    return true;
  case kUYVY:
  default:
    return false;
  }
}

//-------------------------------------------------------------------

// Processor::Init
ErrorCode NativeLayout::Init() {
  MyDbgLog((LOG_TRACE, kDbgImportant,
          TEXT("NativeLayout: Init")));

  // あらかじめイメージのサイズを計算しておく
  const int captured_width = parameter_.clipping_width;
  const int captured_height = parameter_.clipping_height;
  int converted_width = GetOutputImage()->width();
  int converted_height = GetOutputImage()->height();
  int padding_top = 0;
  int padding_bottom = 0;
  int padding_left = 0;
  int padding_right = 0;

  if (CanUsePadding()) {
    // パディングサイズの計算
    const bool no_error = Utilities::CalculatePaddingSize(
        GetOutputImage()->width(),
        GetOutputImage()->height(),
        captured_width,
        captured_height,
        parameter_.stretch,
        parameter_.keep_aspect_ratio,
        &padding_top, &padding_bottom,
        &padding_left, &padding_right);
    ASSERT(no_error);

    // パディング分だけサイズを小さくする
    converted_width -= padding_left + padding_right;
    converted_height -= padding_top + padding_bottom;
  }

  //-------------------------------------------------------------------
  // 初期化の順番はイメージ→プロセッサの順
  //-------------------------------------------------------------------
  // Image
  //-------------------------------------------------------------------
  // GetDIBits用
  const ErrorCode error_captured_image =
      captured_image_.Create(kRGB0,
                             captured_width,
                             captured_height);
  if (error_captured_image != kNoError) {
    return ErrorOccured(error_captured_image);
  }

  // 変換後パディング用
  if (CanUsePadding()) {
    const ErrorCode error_converted_image =
        converted_image_.Create(GetOutputImage()->pixel_format(),
                                converted_width,
                                converted_height);
    if (error_converted_image != kNoError) {
      return ErrorOccured(error_converted_image);
    }
  }
  //-------------------------------------------------------------------
  // Processor
  //-------------------------------------------------------------------
  // スクリーンキャプチャ
  LayoutParameter parameter_array[kMaxProcessorSize];
  parameter_array[0] = parameter_;
  // NativeLayoutなのでsize=1
  ScreenCapture *screen_capture = new ScreenCapture(1, parameter_array);
  screen_capture->SetOutputImage(&captured_image_);
  const ErrorCode error_screen_capture = screen_capture->Init();
  if (error_screen_capture != kNoError) {
    delete screen_capture;
    return ErrorOccured(error_screen_capture);
  }
  screen_capture_ = screen_capture;

  // 拡大縮小ピクセルフォーマット変換
  Scale *scale = new Scale(parameter_.sws_flags);
  scale->SetInputImage(&captured_image_);
  if (CanUsePadding()) {
    // パディング可能ならバッファをはさむ
    scale->SetOutputImage(&converted_image_);
  } else {
    scale->SetOutputImage(GetOutputImage());
  }
  const ErrorCode error_scale_init = scale->Init();
  if (error_scale_init != kNoError) {
    delete scale;
    return ErrorOccured(error_scale_init);
  }
  scale_ = scale;

  // パディング
  if (CanUsePadding()) {
    Padding *padding =
        new Padding(padding_left, padding_right, padding_top, padding_bottom);
    padding->SetInputImage(&converted_image_);
    padding->SetOutputImage(GetOutputImage());
    const ErrorCode error_padding_init = padding->Init();
    if (error_padding_init != kNoError) {
      delete padding;
      return ErrorOccured(error_padding_init);
    }
    padding_ = padding;
  }
  //-------------------------------------------------------------------

  return InitDone();
}

// Processor::Run
ErrorCode NativeLayout::Run() {
  if (GetCurrentError() != kNoError) {
    // 何かエラーが発生している場合は何もしない
    return GetCurrentError();
  }

  // スクリーンキャプチャ
  const ErrorCode error_screen_capture = screen_capture_->Run();
  if (error_screen_capture != kNoError) {
    return ErrorOccured(error_screen_capture);
  }

  // Scaleを利用して変換
  const ErrorCode error_scale = scale_->Run();
  if (error_scale != kNoError) {
    return ErrorOccured(error_scale);
  }

  // Paddingを利用してパディングを行う
  if (CanUsePadding()) {
    const ErrorCode error_padding = padding_->Run();
    if (error_padding != kNoError) {
      return ErrorOccured(error_padding);
    }
  }

  // エラー発生なし
  return GetCurrentError();
}
}   // namespace scff_imaging