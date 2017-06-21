// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_READBACK_TYPES_H_
#define CONTENT_PUBLIC_BROWSER_READBACK_TYPES_H_

#include "base/callback.h"

class SkBitmap;

namespace content {

// ReadbackResponse type used in ContentReadbackHandler.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.content_public.browser.readback_types)
// GENERATED_JAVA_PREFIX_TO_STRIP: READBACK_
enum ReadbackResponse {
  READBACK_SUCCESS,
  READBACK_FAILED,
  READBACK_SURFACE_UNAVAILABLE,
  READBACK_BITMAP_ALLOCATION_FAILURE,
};

typedef const base::Callback<void(const SkBitmap&, ReadbackResponse)>
    ReadbackRequestCallback;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_READBACK_TYPES_H_
