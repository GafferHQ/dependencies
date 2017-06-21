// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_bytes_element_reader.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class UploadBytesElementReaderTest : public PlatformTest {
 protected:
  void SetUp() override {
    const char kData[] = "123abc";
    bytes_.assign(kData, kData + arraysize(kData));
    reader_.reset(new UploadBytesElementReader(&bytes_[0], bytes_.size()));
    ASSERT_EQ(OK, reader_->Init(CompletionCallback()));
    EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
    EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());
    EXPECT_TRUE(reader_->IsInMemory());
  }

  std::vector<char> bytes_;
  scoped_ptr<UploadElementReader> reader_;
};

TEST_F(UploadBytesElementReaderTest, ReadPartially) {
  const size_t kHalfSize = bytes_.size() / 2;
  std::vector<char> buf(kHalfSize);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(bytes_.size() - buf.size(), reader_->BytesRemaining());
  bytes_.resize(kHalfSize);  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadBytesElementReaderTest, ReadAll) {
  std::vector<char> buf(bytes_.size());
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
  // Try to read again.
  EXPECT_EQ(
      0, reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
}

TEST_F(UploadBytesElementReaderTest, ReadTooMuch) {
  const size_t kTooLargeSize = bytes_.size() * 2;
  std::vector<char> buf(kTooLargeSize);
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);
  EXPECT_EQ(
      static_cast<int>(bytes_.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  buf.resize(bytes_.size());  // Resize to compare.
  EXPECT_EQ(bytes_, buf);
}

TEST_F(UploadBytesElementReaderTest, MultipleInit) {
  std::vector<char> buf(bytes_.size());
  scoped_refptr<IOBuffer> wrapped_buffer = new WrappedIOBuffer(&buf[0]);

  // Read all.
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);

  // Call Init() again to reset the state.
  ASSERT_EQ(OK, reader_->Init(CompletionCallback()));
  EXPECT_EQ(bytes_.size(), reader_->GetContentLength());
  EXPECT_EQ(bytes_.size(), reader_->BytesRemaining());

  // Read again.
  EXPECT_EQ(
      static_cast<int>(buf.size()),
      reader_->Read(wrapped_buffer.get(), buf.size(), CompletionCallback()));
  EXPECT_EQ(0U, reader_->BytesRemaining());
  EXPECT_EQ(bytes_, buf);
}

}  // namespace net
