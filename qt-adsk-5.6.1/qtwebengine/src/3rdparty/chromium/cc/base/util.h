// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_UTIL_H_
#define CC_BASE_UTIL_H_

#include "base/basictypes.h"
#include "cc/resources/resource_provider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

inline GLenum GLDataType(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_gl_data_type[RESOURCE_FORMAT_MAX + 1] = {
    GL_UNSIGNED_BYTE,           // RGBA_8888
    GL_UNSIGNED_SHORT_4_4_4_4,  // RGBA_4444
    GL_UNSIGNED_BYTE,           // BGRA_8888
    GL_UNSIGNED_BYTE,           // ALPHA_8
    GL_UNSIGNED_BYTE,           // LUMINANCE_8
    GL_UNSIGNED_SHORT_5_6_5,    // RGB_565,
    GL_UNSIGNED_BYTE,           // ETC1
    GL_UNSIGNED_BYTE            // RED_8
  };
  return format_gl_data_type[format];
}

inline GLenum GLDataFormat(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_gl_data_format[RESOURCE_FORMAT_MAX + 1] = {
    GL_RGBA,           // RGBA_8888
    GL_RGBA,           // RGBA_4444
    GL_BGRA_EXT,       // BGRA_8888
    GL_ALPHA,          // ALPHA_8
    GL_LUMINANCE,      // LUMINANCE_8
    GL_RGB,            // RGB_565
    GL_ETC1_RGB8_OES,  // ETC1
    GL_RED_EXT         // RED_8
  };
  return format_gl_data_format[format];
}

inline GLenum GLInternalFormat(ResourceFormat format) {
  return GLDataFormat(format);
}

}  // namespace cc

#endif  // CC_BASE_UTIL_H_
