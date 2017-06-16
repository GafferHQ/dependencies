// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_VP8_PICTURE_H_
#define CONTENT_COMMON_GPU_MEDIA_VP8_PICTURE_H_

#include "base/memory/ref_counted.h"

namespace content {

class V4L2VP8Picture;
class VaapiVP8Picture;

class VP8Picture : public base::RefCounted<VP8Picture> {
 public:
  VP8Picture();

  virtual V4L2VP8Picture* AsV4L2VP8Picture();
  virtual VaapiVP8Picture* AsVaapiVP8Picture();

 protected:
  friend class base::RefCounted<VP8Picture>;
  virtual ~VP8Picture();

  DISALLOW_COPY_AND_ASSIGN(VP8Picture);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VP8_PICTURE_H_
