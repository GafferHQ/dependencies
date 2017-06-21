// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/scoped_path_override.h"
#include "chromecast/base/error_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class ErrorCodesTest : public testing::Test {
 protected:
  ErrorCodesTest() {}
  ~ErrorCodesTest() override {}

  void SetUp() override {
    // Set up a temporary directory which will be used as our fake home dir.
    ASSERT_TRUE(base::CreateNewTempDirectory("", &fake_home_dir_));
    path_override_.reset(
        new base::ScopedPathOverride(base::DIR_HOME, fake_home_dir_));
  }

  base::FilePath fake_home_dir_;
  scoped_ptr<base::ScopedPathOverride> path_override_;
};

TEST_F(ErrorCodesTest, GetInitialErrorCodeReturnsNoErrorIfMissingFile) {
  EXPECT_EQ(NO_ERROR, GetInitialErrorCode());
}

TEST_F(ErrorCodesTest, SetInitialErrorCodeSucceedsWithNoError) {
  ASSERT_TRUE(SetInitialErrorCode(NO_ERROR));

  // File should not be written.
  ASSERT_FALSE(base::PathExists(fake_home_dir_.Append("initial_error")));
  EXPECT_EQ(NO_ERROR, GetInitialErrorCode());
}

TEST_F(ErrorCodesTest, SetInitialErrorCodeSucceedsWithValidErrors) {
  // Write initial error and read it from the file.
  EXPECT_TRUE(SetInitialErrorCode(ERROR_WEB_CONTENT_RENDER_VIEW_GONE));
  EXPECT_TRUE(base::PathExists(fake_home_dir_.Append("initial_error")));
  EXPECT_EQ(ERROR_WEB_CONTENT_RENDER_VIEW_GONE, GetInitialErrorCode());

  // File should be updated with most recent error.
  EXPECT_TRUE(SetInitialErrorCode(ERROR_UNKNOWN));
  EXPECT_TRUE(base::PathExists(fake_home_dir_.Append("initial_error")));
  EXPECT_EQ(ERROR_UNKNOWN, GetInitialErrorCode());

  // File should be updated with most recent error.
  EXPECT_TRUE(SetInitialErrorCode(ERROR_WEB_CONTENT_NAME_NOT_RESOLVED));
  EXPECT_TRUE(base::PathExists(fake_home_dir_.Append("initial_error")));
  EXPECT_EQ(ERROR_WEB_CONTENT_NAME_NOT_RESOLVED, GetInitialErrorCode());

  // File should be removed after writing NO_ERROR.
  EXPECT_TRUE(SetInitialErrorCode(NO_ERROR));
  EXPECT_FALSE(base::PathExists(fake_home_dir_.Append("initial_error")));
  EXPECT_EQ(NO_ERROR, GetInitialErrorCode());
}

}  // namespace chromecast
