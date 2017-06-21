// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/elements_upload_data_stream.h"

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_element_reader.h"

namespace net {

ElementsUploadDataStream::ElementsUploadDataStream(
    ScopedVector<UploadElementReader> element_readers,
    int64_t identifier)
    : UploadDataStream(false, identifier),
      element_readers_(element_readers.Pass()),
      element_index_(0),
      read_failed_(false),
      weak_ptr_factory_(this) {
}

ElementsUploadDataStream::~ElementsUploadDataStream() {
}

scoped_ptr<UploadDataStream> ElementsUploadDataStream::CreateWithReader(
    scoped_ptr<UploadElementReader> reader,
    int64_t identifier) {
  ScopedVector<UploadElementReader> readers;
  readers.push_back(reader.Pass());
  return scoped_ptr<UploadDataStream>(
      new ElementsUploadDataStream(readers.Pass(), identifier));
}

int ElementsUploadDataStream::InitInternal() {
  return InitElements(0);
}

int ElementsUploadDataStream::ReadInternal(
    IOBuffer* buf,
    int buf_len) {
  DCHECK_GT(buf_len, 0);
  return ReadElements(new DrainableIOBuffer(buf, buf_len));
}

bool ElementsUploadDataStream::IsInMemory() const {
  for (size_t i = 0; i < element_readers_.size(); ++i) {
    if (!element_readers_[i]->IsInMemory())
      return false;
  }
  return true;
}

const ScopedVector<UploadElementReader>*
ElementsUploadDataStream::GetElementReaders() const {
  return &element_readers_;
}

void ElementsUploadDataStream::ResetInternal() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  read_failed_ = false;
  element_index_ = 0;
}

int ElementsUploadDataStream::InitElements(size_t start_index) {
  // Call Init() for all elements.
  for (size_t i = start_index; i < element_readers_.size(); ++i) {
    UploadElementReader* reader = element_readers_[i];
    // When new_result is ERR_IO_PENDING, InitInternal() will be called
    // with start_index == i + 1 when reader->Init() finishes.
    int result = reader->Init(
        base::Bind(&ElementsUploadDataStream::OnInitElementCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   i));
    DCHECK(result != ERR_IO_PENDING || !reader->IsInMemory());
    DCHECK_LE(result, OK);
    if (result != OK)
      return result;
  }

  uint64_t total_size = 0;
  for (size_t i = 0; i < element_readers_.size(); ++i) {
    total_size += element_readers_[i]->GetContentLength();
  }
  SetSize(total_size);
  return OK;
}

void ElementsUploadDataStream::OnInitElementCompleted(size_t index,
                                                      int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  // Check the last result.
  if (result == OK)
    result = InitElements(index + 1);

  if (result != ERR_IO_PENDING)
    OnInitCompleted(result);
}

int ElementsUploadDataStream::ReadElements(
    const scoped_refptr<DrainableIOBuffer>& buf) {
  while (!read_failed_ && element_index_ < element_readers_.size()) {
    UploadElementReader* reader = element_readers_[element_index_];

    if (reader->BytesRemaining() == 0) {
      ++element_index_;
      continue;
    }

    if (buf->BytesRemaining() == 0)
      break;

    int result = reader->Read(
        buf.get(),
        buf->BytesRemaining(),
        base::Bind(&ElementsUploadDataStream::OnReadElementCompleted,
                   weak_ptr_factory_.GetWeakPtr(),
                   buf));
    if (result == ERR_IO_PENDING)
      return ERR_IO_PENDING;
    ProcessReadResult(buf, result);
  }

  if (read_failed_) {
    // If an error occured during read operation, then pad with zero.
    // Otherwise the server will hang waiting for the rest of the data.
    int num_bytes_to_fill =
        static_cast<int>(std::min(static_cast<uint64_t>(buf->BytesRemaining()),
                                  size() - position() - buf->BytesConsumed()));
    DCHECK_GE(num_bytes_to_fill, 0);
    memset(buf->data(), 0, num_bytes_to_fill);
    buf->DidConsume(num_bytes_to_fill);
  }

  return buf->BytesConsumed();
}

void ElementsUploadDataStream::OnReadElementCompleted(
    const scoped_refptr<DrainableIOBuffer>& buf,
    int result) {
  ProcessReadResult(buf, result);

  result = ReadElements(buf);
  if (result != ERR_IO_PENDING)
    OnReadCompleted(result);
}

void ElementsUploadDataStream::ProcessReadResult(
    const scoped_refptr<DrainableIOBuffer>& buf,
    int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(!read_failed_);

  if (result >= 0) {
    buf->DidConsume(result);
  } else {
    read_failed_ = true;
  }
}

}  // namespace net
