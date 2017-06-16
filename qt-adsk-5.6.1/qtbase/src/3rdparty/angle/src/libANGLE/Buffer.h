//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer.h: Defines the gl::Buffer class, representing storage of vertex and/or
// index data. Implements GL buffer objects and related functionality.
// [OpenGL ES 2.0.24] section 2.9 page 21.

#ifndef LIBANGLE_BUFFER_H_
#define LIBANGLE_BUFFER_H_

#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/renderer/IndexRangeCache.h"

#include "common/angleutils.h"

namespace rx
{
class BufferImpl;
};

namespace gl
{

class Buffer : public RefCountObject
{
  public:
    Buffer(rx::BufferImpl *impl, GLuint id);

    virtual ~Buffer();

    Error bufferData(const void *data, GLsizeiptr size, GLenum usage);
    Error bufferSubData(const void *data, GLsizeiptr size, GLintptr offset);
    Error copyBufferSubData(Buffer* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size);
    Error mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access);
    Error unmap();

    GLenum getUsage() const { return mUsage; }
    GLint getAccessFlags() const {  return mAccessFlags; }
    GLboolean isMapped() const { return mMapped; }
    GLvoid *getMapPointer() const { return mMapPointer; }
    GLint64 getMapOffset() const { return mMapOffset; }
    GLint64 getMapLength() const { return mMapLength; }
    GLint64 getSize() const { return mSize; }

    rx::BufferImpl *getImplementation() const { return mBuffer; }

    rx::IndexRangeCache *getIndexRangeCache() { return &mIndexRangeCache; }
    const rx::IndexRangeCache *getIndexRangeCache() const { return &mIndexRangeCache; }

  private:
    rx::BufferImpl *mBuffer;

    GLenum mUsage;
    GLint64 mSize;
    GLint mAccessFlags;
    GLboolean mMapped;
    GLvoid *mMapPointer;
    GLint64 mMapOffset;
    GLint64 mMapLength;

    rx::IndexRangeCache mIndexRangeCache;
};

}

#endif   // LIBANGLE_BUFFER_H_
