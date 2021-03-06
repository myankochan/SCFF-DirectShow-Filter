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

/// @file scff_imaging/avpicture_with_fill_image.cc
/// scff_imaging::AVPictureWithFillImageの定義

#include "scff_imaging/avpicture_with_fill_image.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "scff_imaging/debug.h"
#include "scff_imaging/imaging_types.h"
#include "scff_imaging/utilities.h"

namespace scff_imaging {

//=====================================================================
// scff_imaging::AVPictureWithFillImage
//=====================================================================

AVPictureWithFillImage::AVPictureWithFillImage()
    : Image(),
      raw_bitmap_(nullptr),
      avpicture_(nullptr) {
  /// @attention avpicture_そのものの構築はCreateで行う
}

AVPictureWithFillImage::~AVPictureWithFillImage() {
  if (!IsEmpty()) {
    /// @attention avpicture_fillによって
    ///            関連付けられたメモリ領域も解放してくれる
    avpicture_free(avpicture_);
  }
}

bool AVPictureWithFillImage::IsEmpty() const {
  return avpicture_ == nullptr;
}

ErrorCodes AVPictureWithFillImage::Create(ImagePixelFormats pixel_format,
                                          int width, int height) {
  // pixel_format, width, heightを設定する
  ErrorCodes error_create = Image::Create(pixel_format, width, height);
  if (error_create != ErrorCodes::kNoError) {
    return error_create;
  }

  // RawBitmapを作成
  int size = utilities::CalculateDataSize(pixel_format, width, height);
  uint8_t *raw_bitmap = static_cast<uint8_t*>(av_malloc(size));
  if (raw_bitmap == nullptr) {
    return ErrorCodes::kAVPictureWithFillImageOutOfMemoryError;
  }

  // 取り込み用AVPictureを作成
  AVPicture *avpicture = new AVPicture();
  if (avpicture == nullptr) {
    av_freep(raw_bitmap);
    return ErrorCodes::kAVPictureWithFillImageCannotCreateAVPictureError;
  }

  // 取り込みバッファとAVPictureを関連付け
  int result_fill =
      avpicture_fill(avpicture, raw_bitmap,
                     av_pixel_format(),
                     width, height);
  if (result_fill != size) {
    av_freep(raw_bitmap);
    return ErrorCodes::kAVPictureWithFillImageCannotFillError;
  }

  avpicture_ = avpicture;
  raw_bitmap_ = raw_bitmap;

  return ErrorCodes::kNoError;
}

AVPicture* AVPictureWithFillImage::avpicture() const {
  return avpicture_;
}

uint8_t* AVPictureWithFillImage::raw_bitmap() const {
  return raw_bitmap_;
}
}   // namespace scff_imaging
