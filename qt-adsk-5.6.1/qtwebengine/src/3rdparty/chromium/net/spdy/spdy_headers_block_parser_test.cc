// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_headers_block_parser.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using base::IntToString;
using base::StringPiece;
using base::UintToString;
using std::string;

// A mock the handler class to check that we parse out the correct headers
// and call the callback methods when we should.
class MockSpdyHeadersHandler : public SpdyHeadersHandlerInterface {
 public:
  MOCK_METHOD1(OnHeaderBlock, void(uint32_t num_of_headers));
  MOCK_METHOD1(OnHeaderBlockEnd, void(size_t bytes));
  MOCK_METHOD2(OnHeader, void(StringPiece key, StringPiece value));
};

class SpdyHeadersBlockParserTest :
    public ::testing::TestWithParam<SpdyMajorVersion> {
 public:
  virtual ~SpdyHeadersBlockParserTest() {}

 protected:
  void SetUp() override {
    // Create a parser using the mock handler.
    spdy_version_ = GetParam();

    parser_.reset(new SpdyHeadersBlockParser(spdy_version_, &handler_));
    length_field_size_ =
        SpdyHeadersBlockParser::LengthFieldSizeForVersion(spdy_version_);
  }

  // Create a header block with a specified number of headers.
  string CreateHeaders(uint32_t num_headers, bool insert_nulls) {
    string headers;

    // First, write the number of headers in the header block.
    headers += EncodeLength(num_headers);

    // Second, write the key-value pairs.
    for (uint32_t i = 0; i < num_headers; i++) {
      // Build the key.
      string key;
      if (insert_nulls) {
        key = string(kBaseKey) + string("\0", 1) + UintToString(i);
      } else {
        key = string(kBaseKey) + UintToString(i);
      }
      // Encode the key as SPDY header.
      headers += EncodeLength(key.length());
      headers += key;

      // Build the value.
      string value;
      if (insert_nulls) {
        value = string(kBaseValue) + string("\0", 1) + UintToString(i);
      } else {
        value = string(kBaseValue) + UintToString(i);
      }
      // Encode the value as SPDY header.
      headers += EncodeLength(value.length());
      headers += value;
    }
    return headers;
  }

  string EncodeLength(uint32_t len) {
    char buffer[4];
    if (length_field_size_ == sizeof(uint32_t)) {
      uint32_t net_order_len = htonl(len);
      memcpy(buffer, &net_order_len, length_field_size_);
    } else if (length_field_size_ == sizeof(uint16_t)) {
      uint16_t net_order_len = htons(len);
      memcpy(buffer, &net_order_len, length_field_size_);
    } else {
      CHECK(false) << "Invalid length field size";
    }
    return string(buffer, length_field_size_);
  }

  size_t length_field_size_;
  SpdyMajorVersion spdy_version_;

  MockSpdyHeadersHandler handler_;
  scoped_ptr<SpdyHeadersBlockParser> parser_;

  static const char *const kBaseKey;
  static const char *const kBaseValue;

  // Number of headers and header blocks used in the tests.
  static const int kNumHeadersInBlock = 10;
  static const int kNumHeaderBlocks = 10;
};

const char *const SpdyHeadersBlockParserTest::kBaseKey = "test_key";
const char *const SpdyHeadersBlockParserTest::kBaseValue = "test_value";

// All tests are run with 3 different SPDY versions: SPDY/2, SPDY/3, HTTP/2.
INSTANTIATE_TEST_CASE_P(SpdyHeadersBlockParserTests,
                        SpdyHeadersBlockParserTest,
                        ::testing::Values(SPDY2, SPDY3, HTTP2));

TEST_P(SpdyHeadersBlockParserTest, BasicTest) {
  // Sanity test, verify that we parse out correctly a block with
  // a single key-value pair and that we notify when we start and finish
  // handling a headers block.
  EXPECT_EQ(spdy_version_, parser_->spdy_version());

  string headers(CreateHeaders(1, false));

  EXPECT_CALL(handler_, OnHeaderBlock(1)).Times(1);

  std::string expect_key = kBaseKey + IntToString(0);
  std::string expect_value = kBaseValue + IntToString(0);
  EXPECT_CALL(handler_, OnHeader(StringPiece(expect_key),
                                 StringPiece(expect_value))).Times(1);
  EXPECT_CALL(handler_, OnHeaderBlockEnd(headers.length())).Times(1);

  EXPECT_TRUE(parser_->
      HandleControlFrameHeadersData(1, headers.c_str(), headers.length()));
  EXPECT_EQ(SpdyHeadersBlockParser::NO_PARSER_ERROR, parser_->get_error());
}

TEST_P(SpdyHeadersBlockParserTest, NullsSupportedTest) {
  // Sanity test, verify that we parse out correctly a block with
  // a single key-value pair when the key and value contain null charecters.
  string headers(CreateHeaders(1, true));

  EXPECT_CALL(handler_, OnHeaderBlock(1)).Times(1);

  std::string expect_key = kBaseKey + string("\0", 1) + IntToString(0);
  std::string expect_value = kBaseValue + string("\0", 1) + IntToString(0);
  EXPECT_CALL(handler_, OnHeader(StringPiece(expect_key),
                                 StringPiece(expect_value))).Times(1);
  EXPECT_CALL(handler_, OnHeaderBlockEnd(headers.length())).Times(1);

  EXPECT_TRUE(parser_->
      HandleControlFrameHeadersData(1, headers.c_str(), headers.length()));
  EXPECT_EQ(SpdyHeadersBlockParser::NO_PARSER_ERROR, parser_->get_error());
}

TEST_P(SpdyHeadersBlockParserTest, MultipleBlocksAndHeadersWithPartialData) {
  testing::InSequence s;

  // CreateHeaders is deterministic; we can call it once for the whole test.
  string headers(CreateHeaders(kNumHeadersInBlock, false));

  // The mock doesn't retain storage of arguments, so keep them in scope.
  std::vector<string> retained_arguments;
  for (int i = 0; i < kNumHeadersInBlock; i++) {
    retained_arguments.push_back(kBaseKey + IntToString(i));
    retained_arguments.push_back(kBaseValue + IntToString(i));
  }
  // For each block we expect to parse out the headers in order.
  for (int i = 0; i < kNumHeaderBlocks; i++) {
    EXPECT_CALL(handler_, OnHeaderBlock(kNumHeadersInBlock)).Times(1);
    for (int j = 0; j < kNumHeadersInBlock; j++) {
      EXPECT_CALL(handler_, OnHeader(
          StringPiece(retained_arguments[2 * j]),
          StringPiece(retained_arguments[2 * j + 1]))).Times(1);
    }
    EXPECT_CALL(handler_, OnHeaderBlockEnd(headers.length())).Times(1);
  }
  // Parse the header blocks, feeding the parser one byte at a time.
  for (int i = 1; i <= kNumHeaderBlocks; i++) {
    for (string::iterator it = headers.begin(); it != headers.end(); ++it) {
      if ((it + 1) == headers.end()) {
        // Last byte completes the block.
        EXPECT_TRUE(parser_->HandleControlFrameHeadersData(i, &(*it), 1));
        EXPECT_EQ(SpdyHeadersBlockParser::NO_PARSER_ERROR,
                  parser_->get_error());
      } else {
        EXPECT_FALSE(parser_->HandleControlFrameHeadersData(i, &(*it), 1));
        EXPECT_EQ(SpdyHeadersBlockParser::NEED_MORE_DATA, parser_->get_error());
      }
    }
  }
}

TEST_P(SpdyHeadersBlockParserTest, HandlesEmptyCallsTest) {
  EXPECT_CALL(handler_, OnHeaderBlock(1)).Times(1);

  string headers(CreateHeaders(1, false));

  string expect_key = kBaseKey + IntToString(0);
  string expect_value = kBaseValue + IntToString(0);
  EXPECT_CALL(handler_, OnHeader(StringPiece(expect_key),
                                 StringPiece(expect_value))).Times(1);
  EXPECT_CALL(handler_, OnHeaderBlockEnd(headers.length())).Times(1);

  // Send a header in pieces with intermediate empty calls.
  for (string::iterator it = headers.begin(); it != headers.end(); ++it) {
    if ((it + 1) == headers.end()) {
      // Last byte completes the block.
      EXPECT_TRUE(parser_->HandleControlFrameHeadersData(1, &(*it), 1));
      EXPECT_EQ(SpdyHeadersBlockParser::NO_PARSER_ERROR, parser_->get_error());
    } else {
      EXPECT_FALSE(parser_->HandleControlFrameHeadersData(1, &(*it), 1));
      EXPECT_EQ(SpdyHeadersBlockParser::NEED_MORE_DATA, parser_->get_error());
      EXPECT_FALSE(parser_->HandleControlFrameHeadersData(1, NULL, 0));
    }
  }
}

TEST_P(SpdyHeadersBlockParserTest, LargeBlocksDiscardedTest) {
  // Header block with too many headers.
  {
    string headers = EncodeLength(
        parser_->MaxNumberOfHeadersForVersion(spdy_version_) + 1);
    EXPECT_FALSE(parser_->
        HandleControlFrameHeadersData(1, headers.c_str(), headers.length()));
    EXPECT_EQ(SpdyHeadersBlockParser::HEADER_BLOCK_TOO_LARGE,
              parser_->get_error());
  }
  parser_.reset(new SpdyHeadersBlockParser(spdy_version_, &handler_));
  // Header block with one header, which has a too-long key.
  {
    EXPECT_CALL(handler_, OnHeaderBlock(1)).Times(1);

    string headers = EncodeLength(1) + EncodeLength(
        SpdyHeadersBlockParser::kMaximumFieldLength + 1);
    EXPECT_FALSE(parser_->
        HandleControlFrameHeadersData(1, headers.c_str(), headers.length()));
    EXPECT_EQ(SpdyHeadersBlockParser::HEADER_FIELD_TOO_LARGE,
              parser_->get_error());
  }
}

TEST_P(SpdyHeadersBlockParserTest, ExtraDataTest) {
  string headers = CreateHeaders(1, false) + "foobar";

  EXPECT_CALL(handler_, OnHeaderBlock(1)).Times(1);
  EXPECT_CALL(handler_, OnHeaderBlockEnd(headers.length())).Times(1);

  string expect_key = kBaseKey + IntToString(0);
  string expect_value = kBaseValue + IntToString(0);
  EXPECT_CALL(handler_, OnHeader(StringPiece(expect_key),
                                 StringPiece(expect_value))).Times(1);

  EXPECT_FALSE(parser_->HandleControlFrameHeadersData(1, headers.c_str(),
                                                      headers.length()));
  EXPECT_EQ(SpdyHeadersBlockParser::TOO_MUCH_DATA, parser_->get_error());
}

TEST_P(SpdyHeadersBlockParserTest, WrongStreamIdTest) {
  string headers(CreateHeaders(kNumHeadersInBlock, false));
  EXPECT_FALSE(parser_->HandleControlFrameHeadersData(1, headers.data(), 1));
  EXPECT_EQ(SpdyHeadersBlockParser::NEED_MORE_DATA, parser_->get_error());
  bool result;
  EXPECT_DFATAL(
      result = parser_->HandleControlFrameHeadersData(2, headers.data() + 1, 1),
      "Unexpected stream id: 2 \\(expected 1\\)");
  EXPECT_FALSE(result);
  EXPECT_EQ(SpdyHeadersBlockParser::UNEXPECTED_STREAM_ID, parser_->get_error());
}

TEST_P(SpdyHeadersBlockParserTest, InvalidStreamIdTest) {
  string headers(CreateHeaders(kNumHeadersInBlock, false));
  bool result;
  EXPECT_DFATAL(
      result = parser_->HandleControlFrameHeadersData(0, headers.data(), 1),
      "Expected nonzero stream id, saw: 0");
  EXPECT_FALSE(result);
  EXPECT_EQ(SpdyHeadersBlockParser::UNEXPECTED_STREAM_ID, parser_->get_error());
}

}  // namespace net
