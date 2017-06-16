// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_response_parser.h"

#include <string>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_tree_head.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace ct {

namespace {
scoped_ptr<base::Value> ParseJson(const std::string& json) {
  base::JSONReader json_reader;
  return json_reader.Read(json).Pass();
}
}

TEST(CTLogResponseParserTest, ParsesValidJsonSTH) {
  scoped_ptr<base::Value> sample_sth_json = ParseJson(GetSampleSTHAsJson());
  SignedTreeHead tree_head;
  EXPECT_TRUE(FillSignedTreeHead(*sample_sth_json.get(), &tree_head));

  SignedTreeHead sample_sth;
  GetSampleSignedTreeHead(&sample_sth);

  ASSERT_EQ(SignedTreeHead::V1, tree_head.version);
  ASSERT_EQ(sample_sth.timestamp, tree_head.timestamp);
  ASSERT_EQ(sample_sth.tree_size, tree_head.tree_size);

  // Copy the field from the SignedTreeHead because it's not null terminated
  // there and ASSERT_STREQ expects null-terminated strings.
  char actual_hash[kSthRootHashLength + 1];
  memcpy(actual_hash, tree_head.sha256_root_hash, kSthRootHashLength);
  actual_hash[kSthRootHashLength] = '\0';
  std::string expected_sha256_root_hash = GetSampleSTHSHA256RootHash();
  ASSERT_STREQ(expected_sha256_root_hash.c_str(), actual_hash);

  const DigitallySigned& expected_signature(sample_sth.signature);

  ASSERT_EQ(tree_head.signature.hash_algorithm,
            expected_signature.hash_algorithm);
  ASSERT_EQ(tree_head.signature.signature_algorithm,
            expected_signature.signature_algorithm);
  ASSERT_EQ(tree_head.signature.signature_data,
            expected_signature.signature_data);
}

TEST(CTLogResponseParserTest, FailsToParseMissingFields) {
  scoped_ptr<base::Value> missing_signature_sth = ParseJson(
      CreateSignedTreeHeadJsonString(1 /* tree_size */, 123456u /* timestamp */,
                                     GetSampleSTHSHA256RootHash(), ""));

  SignedTreeHead tree_head;
  ASSERT_FALSE(FillSignedTreeHead(*missing_signature_sth.get(), &tree_head));

  scoped_ptr<base::Value> missing_root_hash_sth = ParseJson(
      CreateSignedTreeHeadJsonString(1 /* tree_size */, 123456u /* timestamp */,
                                     "", GetSampleSTHTreeHeadSignature()));
  ASSERT_FALSE(FillSignedTreeHead(*missing_root_hash_sth.get(), &tree_head));
}

TEST(CTLogResponseParserTest, FailsToParseIncorrectLengthRootHash) {
  SignedTreeHead tree_head;

  std::string too_long_hash;
  base::Base64Decode(
      base::StringPiece("/WHFMgXtI/umKKuACJIN0Bb73TcILm9WkeU6qszvoArK\n"),
      &too_long_hash);
  scoped_ptr<base::Value> too_long_hash_json =
      ParseJson(CreateSignedTreeHeadJsonString(
          1 /* tree_size */, 123456u /* timestamp */,
          GetSampleSTHSHA256RootHash(), too_long_hash));
  ASSERT_FALSE(FillSignedTreeHead(*too_long_hash_json.get(), &tree_head));

  std::string too_short_hash;
  base::Base64Decode(
      base::StringPiece("/WHFMgXtI/umKKuACJIN0Bb73TcILm9WkeU6qszvoA==\n"),
      &too_short_hash);
  scoped_ptr<base::Value> too_short_hash_json =
      ParseJson(CreateSignedTreeHeadJsonString(
          1 /* tree_size */, 123456u /* timestamp */,
          GetSampleSTHSHA256RootHash(), too_short_hash));
  ASSERT_FALSE(FillSignedTreeHead(*too_short_hash_json.get(), &tree_head));
}

}  // namespace ct

}  // namespace net
