// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_decode_accelerator.h"

#include <GLES2/gl2.h>
#include "base/logging.h"

namespace media {

VideoDecodeAccelerator::~VideoDecodeAccelerator() {}

bool VideoDecodeAccelerator::CanDecodeOnIOThread() {
  // GPU process subclasses must override this.
  LOG(FATAL) << "This should only get called in the GPU process";
  return false;  // not reached
}

GLenum VideoDecodeAccelerator::GetSurfaceInternalFormat() const {
  return GL_RGBA;
}

VideoDecodeAccelerator::SupportedProfile::SupportedProfile()
    : profile(media::VIDEO_CODEC_PROFILE_UNKNOWN) {
}

VideoDecodeAccelerator::SupportedProfile::~SupportedProfile() {
}

} // namespace media

namespace base {

void DefaultDeleter<media::VideoDecodeAccelerator>::operator()(
    void* video_decode_accelerator) const {
  static_cast<media::VideoDecodeAccelerator*>(video_decode_accelerator)->
      Destroy();
}

}  // namespace base


