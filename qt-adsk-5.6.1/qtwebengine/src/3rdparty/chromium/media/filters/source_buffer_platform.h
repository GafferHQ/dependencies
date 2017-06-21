// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_PLATFORM_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_PLATFORM_H_

#include "media/base/media_export.h"

namespace media {

// The maximum amount of data in bytes the stream will keep in memory.
MEDIA_EXPORT extern const int kSourceBufferAudioMemoryLimit;
MEDIA_EXPORT extern const int kSourceBufferVideoMemoryLimit;

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_PLATFORM_H_
