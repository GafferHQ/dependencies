// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_ELEMENT_READER_H_
#define NET_BASE_UPLOAD_ELEMENT_READER_H_

#include "base/basictypes.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

namespace net {

class IOBuffer;
class UploadBytesElementReader;
class UploadDiskCacheEntryElementReader;
class UploadFileElementReader;

// An interface to read an upload data element.
class NET_EXPORT UploadElementReader {
 public:
  UploadElementReader() {}
  virtual ~UploadElementReader() {}

  // Returns this instance's pointer as UploadDiskCacheEntryElementReader when
  // possible, otherwise returns nullptr.
  virtual const UploadDiskCacheEntryElementReader*
  AsDiskCacheEntryReaderForTests() const;

  // Returns this instance's pointer as UploadBytesElementReader when possible,
  // otherwise returns NULL.
  virtual const UploadBytesElementReader* AsBytesReader() const;

  // Returns this instance's pointer as UploadFileElementReader when possible,
  // otherwise returns NULL.
  virtual const UploadFileElementReader* AsFileReader() const;

  // Initializes the instance synchronously when possible, otherwise does
  // initialization aynschronously, returns ERR_IO_PENDING and runs callback.
  // Calling this method again after a Init() success results in resetting the
  // state.
  virtual int Init(const CompletionCallback& callback) = 0;

  // Returns the byte-length of the element. For files that do not exist, 0
  // is returned. This is done for consistency with Mozilla.
  virtual uint64_t GetContentLength() const = 0;

  // Returns the number of bytes remaining to read.
  virtual uint64_t BytesRemaining() const = 0;

  // Returns true if the upload element is entirely in memory.
  // The default implementation returns false.
  virtual bool IsInMemory() const;

  // Reads up to |buf_length| bytes synchronously and returns the number of
  // bytes read or error code when possible, otherwise, returns ERR_IO_PENDING
  // and runs |callback| with the result. |buf_length| must be greater than 0.
  virtual int Read(IOBuffer* buf,
                   int buf_length,
                   const CompletionCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadElementReader);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_ELEMENT_READER_H_
