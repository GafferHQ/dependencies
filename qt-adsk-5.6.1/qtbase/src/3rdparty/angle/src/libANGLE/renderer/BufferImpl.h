//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferImpl.h: Defines the abstract rx::BufferImpl class.

#ifndef LIBANGLE_RENDERER_BUFFERIMPL_H_
#define LIBANGLE_RENDERER_BUFFERIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/Buffer.h"

#include <stdint.h>

namespace rx
{

class BufferImpl : angle::NonCopyable
{
  public:
    virtual ~BufferImpl() { }

    virtual gl::Error setData(const void* data, size_t size, GLenum usage) = 0;
    virtual gl::Error setSubData(const void* data, size_t size, size_t offset) = 0;
    virtual gl::Error copySubData(BufferImpl* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size) = 0;
    virtual gl::Error map(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr) = 0;
    virtual gl::Error unmap() = 0;

    // This method may not have a corresponding GL-backed function. It is necessary
    // for validation, for certain indexed draw calls.
    virtual gl::Error getData(const uint8_t **outData) = 0;
};

}

#endif // LIBANGLE_RENDERER_BUFFERIMPL_H_
