// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_url_generator.h"

#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace google_apis {
namespace {
// The URLs used for production may be different for Chromium OS and Chrome
// OS, so use testing base urls.
const char kBaseUrlForTesting[] = "https://www.example.com";
const char kBaseDownloadUrlForTesting[] = "https://download.example.com/p/";
}  // namespace

class DriveApiUrlGeneratorTest : public testing::Test {
 public:
  DriveApiUrlGeneratorTest()
      : url_generator_(GURL(kBaseUrlForTesting),
                       GURL(kBaseDownloadUrlForTesting)) {}

 protected:
  DriveApiUrlGenerator url_generator_;
};

// Make sure the hard-coded urls are returned.
TEST_F(DriveApiUrlGeneratorTest, GetAboutGetUrl) {
  EXPECT_EQ("https://www.example.com/drive/v2/about",
            url_generator_.GetAboutGetUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetAppsListUrl) {
  const bool use_internal_url = true;
  EXPECT_EQ("https://www.example.com/drive/v2internal/apps",
            url_generator_.GetAppsListUrl(use_internal_url).spec());
  EXPECT_EQ("https://www.example.com/drive/v2/apps",
            url_generator_.GetAppsListUrl(!use_internal_url).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetAppsDeleteUrl) {
  EXPECT_EQ("https://www.example.com/drive/v2internal/apps/0ADK06pfg",
            url_generator_.GetAppsDeleteUrl("0ADK06pfg").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesGetUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.example.com/drive/v2/files/0ADK06pfg",
            url_generator_.GetFilesGetUrl("0ADK06pfg", false, GURL()).spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/0Bz0bd074",
            url_generator_.GetFilesGetUrl("0Bz0bd074", false, GURL()).spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afile_id",
      url_generator_.GetFilesGetUrl("file:file_id", false, GURL()).spec());

  // If |use_internal_endpoint| is true, the generated url should point to the
  // v2internal.
  EXPECT_EQ("https://www.example.com/drive/v2internal/files/0ADK06pfg",
            url_generator_.GetFilesGetUrl("0ADK06pfg", true, GURL()).spec());

  // If |embed_origin| is not empty, it should be added as a query parameter.
  url::AddStandardScheme("chrome-extension");
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg"
      "?embedOrigin=chrome-extension%3A%2F%2Ftest",
      url_generator_.GetFilesGetUrl("0ADK06pfg", false,
                                    GURL("chrome-extension://test")).spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2internal/files/0ADK06pfg"
      "?embedOrigin=chrome-extension%3A%2F%2Ftest",
      url_generator_.GetFilesGetUrl("0ADK06pfg", true,
                                    GURL("chrome-extension://test")).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesAuthorizeUrl) {
  EXPECT_EQ(
      "https://www.example.com/drive/v2internal/files/aa/authorize?appId=bb",
      url_generator_.GetFilesAuthorizeUrl("aa", "bb").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesInsertUrl) {
  EXPECT_EQ("https://www.example.com/drive/v2/files",
            url_generator_.GetFilesInsertUrl().spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilePatchUrl) {
  struct TestPattern {
    bool set_modified_date;
    bool update_viewed_date;
    const std::string expected_query;
  };
  const TestPattern kTestPatterns[] = {
    { false, true, "" },
    { true, true, "?setModifiedDate=true" },
    { false, false, "?updateViewedDate=false" },
    { true, false, "?setModifiedDate=true&updateViewedDate=false" },
  };

  for (size_t i = 0; i < arraysize(kTestPatterns); ++i) {
    EXPECT_EQ(
        "https://www.example.com/drive/v2/files/0ADK06pfg" +
            kTestPatterns[i].expected_query,
        url_generator_.GetFilesPatchUrl(
                           "0ADK06pfg", kTestPatterns[i].set_modified_date,
                           kTestPatterns[i].update_viewed_date).spec());

    EXPECT_EQ(
        "https://www.example.com/drive/v2/files/0Bz0bd074" +
            kTestPatterns[i].expected_query,
        url_generator_.GetFilesPatchUrl(
                           "0Bz0bd074", kTestPatterns[i].set_modified_date,
                           kTestPatterns[i].update_viewed_date).spec());

    EXPECT_EQ(
        "https://www.example.com/drive/v2/files/file%3Afile_id" +
            kTestPatterns[i].expected_query,
        url_generator_.GetFilesPatchUrl(
                           "file:file_id", kTestPatterns[i].set_modified_date,
                           kTestPatterns[i].update_viewed_date).spec());
  }
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesCopyUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.example.com/drive/v2/files/0ADK06pfg/copy",
            url_generator_.GetFilesCopyUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/0Bz0bd074/copy",
            url_generator_.GetFilesCopyUrl("0Bz0bd074").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/file%3Afile_id/copy",
            url_generator_.GetFilesCopyUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesListUrl) {
  struct TestPattern {
    int max_results;
    const std::string page_token;
    const std::string q;
    const std::string expected_query;
  };
  const TestPattern kTestPatterns[] = {
    { 100, "", "", "" },
    { 150, "", "", "?maxResults=150" },
    { 10, "", "", "?maxResults=10" },
    { 100, "token", "", "?pageToken=token" },
    { 150, "token", "", "?maxResults=150&pageToken=token" },
    { 10, "token", "", "?maxResults=10&pageToken=token" },
    { 100, "", "query", "?q=query" },
    { 150, "", "query", "?maxResults=150&q=query" },
    { 10, "", "query", "?maxResults=10&q=query" },
    { 100, "token", "query", "?pageToken=token&q=query" },
    { 150, "token", "query", "?maxResults=150&pageToken=token&q=query" },
    { 10, "token", "query", "?maxResults=10&pageToken=token&q=query" },
  };

  for (size_t i = 0; i < arraysize(kTestPatterns); ++i) {
    EXPECT_EQ("https://www.example.com/drive/v2/files" +
                  kTestPatterns[i].expected_query,
              url_generator_.GetFilesListUrl(kTestPatterns[i].max_results,
                                             kTestPatterns[i].page_token,
                                             kTestPatterns[i].q).spec());
  }
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesDeleteUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.example.com/drive/v2/files/0ADK06pfg",
            url_generator_.GetFilesDeleteUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/0Bz0bd074",
            url_generator_.GetFilesDeleteUrl("0Bz0bd074").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/file%3Afile_id",
            url_generator_.GetFilesDeleteUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetFilesTrashUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.example.com/drive/v2/files/0ADK06pfg/trash",
            url_generator_.GetFilesTrashUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/0Bz0bd074/trash",
            url_generator_.GetFilesTrashUrl("0Bz0bd074").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/file%3Afile_id/trash",
            url_generator_.GetFilesTrashUrl("file:file_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChangesListUrl) {
  struct TestPattern {
    bool include_deleted;
    int max_results;
    const std::string page_token;
    int64 start_change_id;
    const std::string expected_query;
  };
  const TestPattern kTestPatterns[] = {
    { true, 100, "", 0, "" },
    { false, 100, "", 0, "?includeDeleted=false" },
    { true, 150, "", 0, "?maxResults=150" },
    { false, 150, "", 0, "?includeDeleted=false&maxResults=150" },
    { true, 10, "", 0, "?maxResults=10" },
    { false, 10, "", 0, "?includeDeleted=false&maxResults=10" },

    { true, 100, "token", 0, "?pageToken=token" },
    { false, 100, "token", 0, "?includeDeleted=false&pageToken=token" },
    { true, 150, "token", 0, "?maxResults=150&pageToken=token" },
    { false, 150, "token", 0,
      "?includeDeleted=false&maxResults=150&pageToken=token" },
    { true, 10, "token", 0, "?maxResults=10&pageToken=token" },
    { false, 10, "token", 0,
      "?includeDeleted=false&maxResults=10&pageToken=token" },

    { true, 100, "", 12345, "?startChangeId=12345" },
    { false, 100, "", 12345, "?includeDeleted=false&startChangeId=12345" },
    { true, 150, "", 12345, "?maxResults=150&startChangeId=12345" },
    { false, 150, "", 12345,
      "?includeDeleted=false&maxResults=150&startChangeId=12345" },
    { true, 10, "", 12345, "?maxResults=10&startChangeId=12345" },
    { false, 10, "", 12345,
      "?includeDeleted=false&maxResults=10&startChangeId=12345" },

    { true, 100, "token", 12345, "?pageToken=token&startChangeId=12345" },
    { false, 100, "token", 12345,
      "?includeDeleted=false&pageToken=token&startChangeId=12345" },
    { true, 150, "token", 12345,
      "?maxResults=150&pageToken=token&startChangeId=12345" },
    { false, 150, "token", 12345,
      "?includeDeleted=false&maxResults=150&pageToken=token"
      "&startChangeId=12345" },
    { true, 10, "token", 12345,
      "?maxResults=10&pageToken=token&startChangeId=12345" },
    { false, 10, "token", 12345,
      "?includeDeleted=false&maxResults=10&pageToken=token"
      "&startChangeId=12345" },
  };

  for (size_t i = 0; i < arraysize(kTestPatterns); ++i) {
    EXPECT_EQ("https://www.example.com/drive/v2/changes" +
                  kTestPatterns[i].expected_query,
              url_generator_.GetChangesListUrl(kTestPatterns[i].include_deleted,
                                               kTestPatterns[i].max_results,
                                               kTestPatterns[i].page_token,
                                               kTestPatterns[i].start_change_id)
                  .spec());
  }
}

TEST_F(DriveApiUrlGeneratorTest, GetChildrenInsertUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ("https://www.example.com/drive/v2/files/0ADK06pfg/children",
            url_generator_.GetChildrenInsertUrl("0ADK06pfg").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/0Bz0bd074/children",
            url_generator_.GetChildrenInsertUrl("0Bz0bd074").spec());
  EXPECT_EQ("https://www.example.com/drive/v2/files/file%3Afolder_id/children",
            url_generator_.GetChildrenInsertUrl("file:folder_id").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetChildrenDeleteUrl) {
  // |file_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0ADK06pfg/children/0Bz0bd074",
      url_generator_.GetChildrenDeleteUrl("0Bz0bd074", "0ADK06pfg").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/0Bz0bd074/children/0ADK06pfg",
      url_generator_.GetChildrenDeleteUrl("0ADK06pfg", "0Bz0bd074").spec());
  EXPECT_EQ(
      "https://www.example.com/drive/v2/files/file%3Afolder_id/children"
      "/file%3Achild_id",
      url_generator_.GetChildrenDeleteUrl("file:child_id", "file:folder_id")
          .spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetInitiateUploadNewFileUrl) {
  const bool kSetModifiedDate = true;

  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files?uploadType=resumable",
      url_generator_.GetInitiateUploadNewFileUrl(!kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files?uploadType=resumable&"
      "setModifiedDate=true",
      url_generator_.GetInitiateUploadNewFileUrl(kSetModifiedDate).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetInitiateUploadExistingFileUrl) {
  const bool kSetModifiedDate = true;

  // |resource_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0ADK06pfg"
      "?uploadType=resumable",
      url_generator_.GetInitiateUploadExistingFileUrl(
                         "0ADK06pfg", !kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0Bz0bd074"
      "?uploadType=resumable",
      url_generator_.GetInitiateUploadExistingFileUrl(
                         "0Bz0bd074", !kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=resumable",
      url_generator_.GetInitiateUploadExistingFileUrl(
                         "file:file_id", !kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=resumable&setModifiedDate=true",
      url_generator_.GetInitiateUploadExistingFileUrl("file:file_id",
                                                      kSetModifiedDate).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetMultipartUploadNewFileUrl) {
  const bool kSetModifiedDate = true;

  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files?uploadType=multipart",
      url_generator_.GetMultipartUploadNewFileUrl(!kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files?uploadType=multipart&"
      "setModifiedDate=true",
      url_generator_.GetMultipartUploadNewFileUrl(kSetModifiedDate).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GetMultipartUploadExistingFileUrl) {
  const bool kSetModifiedDate = true;

  // |resource_id| should be embedded into the url.
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0ADK06pfg"
      "?uploadType=multipart",
      url_generator_.GetMultipartUploadExistingFileUrl(
                         "0ADK06pfg", !kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/0Bz0bd074"
      "?uploadType=multipart",
      url_generator_.GetMultipartUploadExistingFileUrl(
                         "0Bz0bd074", !kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=multipart",
      url_generator_.GetMultipartUploadExistingFileUrl(
                         "file:file_id", !kSetModifiedDate).spec());
  EXPECT_EQ(
      "https://www.example.com/upload/drive/v2/files/file%3Afile_id"
      "?uploadType=multipart&setModifiedDate=true",
      url_generator_.GetMultipartUploadExistingFileUrl(
                         "file:file_id", kSetModifiedDate).spec());
}

TEST_F(DriveApiUrlGeneratorTest, GenerateDownloadFileUrl) {
  EXPECT_EQ("https://download.example.com/p/host/resourceId",
            url_generator_.GenerateDownloadFileUrl("resourceId").spec());
  EXPECT_EQ("https://download.example.com/p/host/file%3AresourceId",
            url_generator_.GenerateDownloadFileUrl("file:resourceId").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GeneratePermissionsInsertUrl) {
  EXPECT_EQ("https://www.example.com/drive/v2/files/0ADK06pfg/permissions",
            url_generator_.GetPermissionsInsertUrl("0ADK06pfg").spec());
}

TEST_F(DriveApiUrlGeneratorTest, GenerateThumbnailUrl) {
  EXPECT_EQ(
      "https://download.example.com/p/thumb/0ADK06pfg?width=500&height=500",
      url_generator_.GetThumbnailUrl("0ADK06pfg", 500, 500, false).spec());

  EXPECT_EQ(
      "https://download.example.com/p/thumb/"
      "0ADK06pfg?width=360&height=360&crop=true",
      url_generator_.GetThumbnailUrl("0ADK06pfg", 360, 360, true).spec());
}

TEST_F(DriveApiUrlGeneratorTest, BatchUploadUrl) {
  EXPECT_EQ("https://www.example.com/upload/drive",
            url_generator_.GetBatchUploadUrl().spec());
}

}  // namespace google_apis
