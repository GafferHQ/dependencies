// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/public/test/async_file_test_helper.h"
#include "content/public/test/test_file_system_context.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::BlobDataSnapshot;
using storage::BlobDataBuilder;
using storage::BlobURLRequestJob;

namespace content {

namespace {

const int kBufferSize = 1024;
const char kTestData1[] = "Hello";
const char kTestData2[] = "Here it is data.";
const char kTestFileData1[] = "0123456789";
const char kTestFileData2[] = "This is sample file.";
const char kTestFileSystemFileData1[] = "abcdefghijklmnop";
const char kTestFileSystemFileData2[] = "File system file test data.";
const char kTestDiskCacheKey[] = "pipe";
const char kTestDiskCacheData[] = "Ceci n'est pas un pamplemousse.";
const char kTestContentType[] = "foo/bar";
const char kTestContentDisposition[] = "attachment; filename=foo.txt";

const char kFileSystemURLOrigin[] = "http://remote";
const storage::FileSystemType kFileSystemType =
    storage::kFileSystemTypeTemporary;

const int kTestDiskCacheStreamIndex = 0;

// Our disk cache tests don't need a real data handle since the tests themselves
// scope the disk cache and entries.
class EmptyDataHandle : public storage::BlobDataBuilder::DataHandle {
 private:
  ~EmptyDataHandle() override {}
};

scoped_ptr<disk_cache::Backend> CreateInMemoryDiskCache() {
  scoped_ptr<disk_cache::Backend> cache;
  net::TestCompletionCallback callback;
  int rv = disk_cache::CreateCacheBackend(net::MEMORY_CACHE,
                                          net::CACHE_BACKEND_DEFAULT,
                                          base::FilePath(), 0,
                                          false, nullptr, nullptr, &cache,
                                          callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  return cache.Pass();
}

disk_cache::ScopedEntryPtr CreateDiskCacheEntry(disk_cache::Backend* cache,
                                                const char* key,
                                                const std::string& data) {
  disk_cache::Entry* temp_entry = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->CreateEntry(key, &temp_entry, callback.callback());
  if (callback.GetResult(rv) != net::OK)
    return nullptr;
  disk_cache::ScopedEntryPtr entry(temp_entry);

  scoped_refptr<net::StringIOBuffer> iobuffer = new net::StringIOBuffer(data);
  rv = entry->WriteData(kTestDiskCacheStreamIndex, 0, iobuffer.get(),
                        iobuffer->size(), callback.callback(), false);
  EXPECT_EQ(static_cast<int>(data.size()), callback.GetResult(rv));
  return entry.Pass();
}

}  // namespace

class BlobURLRequestJobTest : public testing::Test {
 public:
  // A simple ProtocolHandler implementation to create BlobURLRequestJob.
  class MockProtocolHandler :
      public net::URLRequestJobFactory::ProtocolHandler {
   public:
    MockProtocolHandler(BlobURLRequestJobTest* test) : test_(test) {}

    // net::URLRequestJobFactory::ProtocolHandler override.
    net::URLRequestJob* MaybeCreateJob(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const override {
      return new BlobURLRequestJob(request, network_delegate,
                                   test_->GetSnapshotFromBuilder(),
                                   test_->file_system_context_.get(),
                                   base::ThreadTaskRunnerHandle::Get().get());
    }

   private:
    BlobURLRequestJobTest* test_;
  };

  BlobURLRequestJobTest()
      : blob_data_(new BlobDataBuilder("uuid")), expected_status_code_(0) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    temp_file1_ = temp_dir_.path().AppendASCII("BlobFile1.dat");
    ASSERT_EQ(static_cast<int>(arraysize(kTestFileData1) - 1),
              base::WriteFile(temp_file1_, kTestFileData1,
                                   arraysize(kTestFileData1) - 1));
    base::File::Info file_info1;
    base::GetFileInfo(temp_file1_, &file_info1);
    temp_file_modification_time1_ = file_info1.last_modified;

    temp_file2_ = temp_dir_.path().AppendASCII("BlobFile2.dat");
    ASSERT_EQ(static_cast<int>(arraysize(kTestFileData2) - 1),
              base::WriteFile(temp_file2_, kTestFileData2,
                                   arraysize(kTestFileData2) - 1));
    base::File::Info file_info2;
    base::GetFileInfo(temp_file2_, &file_info2);
    temp_file_modification_time2_ = file_info2.last_modified;

    disk_cache_backend_ = CreateInMemoryDiskCache();
    disk_cache_entry_ = CreateDiskCacheEntry(
        disk_cache_backend_.get(), kTestDiskCacheKey, kTestDiskCacheData);

    url_request_job_factory_.SetProtocolHandler("blob",
                                                new MockProtocolHandler(this));
    url_request_context_.set_job_factory(&url_request_job_factory_);
  }

  void TearDown() override {
    blob_handle_.reset();
    // Clean up for ASAN
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void SetUpFileSystem() {
    // Prepare file system.
    file_system_context_ = CreateFileSystemContextForTesting(
        NULL, temp_dir_.path());

    file_system_context_->OpenFileSystem(
        GURL(kFileSystemURLOrigin),
        kFileSystemType,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::Bind(&BlobURLRequestJobTest::OnValidateFileSystem,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(file_system_root_url_.is_valid());

    // Prepare files on file system.
    const char kFilename1[] = "FileSystemFile1.dat";
    temp_file_system_file1_ = GetFileSystemURL(kFilename1);
    WriteFileSystemFile(kFilename1, kTestFileSystemFileData1,
                        arraysize(kTestFileSystemFileData1) - 1,
                        &temp_file_system_file_modification_time1_);
    const char kFilename2[] = "FileSystemFile2.dat";
    temp_file_system_file2_ = GetFileSystemURL(kFilename2);
    WriteFileSystemFile(kFilename2, kTestFileSystemFileData2,
                        arraysize(kTestFileSystemFileData2) - 1,
                        &temp_file_system_file_modification_time2_);
  }

  GURL GetFileSystemURL(const std::string& filename) {
    return GURL(file_system_root_url_.spec() + filename);
  }

  void WriteFileSystemFile(const std::string& filename,
                           const char* buf, int buf_size,
                           base::Time* modification_time) {
    storage::FileSystemURL url =
        file_system_context_->CreateCrackedFileSystemURL(
            GURL(kFileSystemURLOrigin),
            kFileSystemType,
            base::FilePath().AppendASCII(filename));

    ASSERT_EQ(base::File::FILE_OK,
              content::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url, buf, buf_size));

    base::File::Info file_info;
    ASSERT_EQ(base::File::FILE_OK,
              content::AsyncFileTestHelper::GetMetadata(
                  file_system_context_.get(), url, &file_info));
    if (modification_time)
      *modification_time = file_info.last_modified;
  }

  void OnValidateFileSystem(const GURL& root,
                            const std::string& name,
                            base::File::Error result) {
    ASSERT_EQ(base::File::FILE_OK, result);
    ASSERT_TRUE(root.is_valid());
    file_system_root_url_ = root;
  }

  void TestSuccessNonrangeRequest(const std::string& expected_response,
                                  int64 expected_content_length) {
    expected_status_code_ = 200;
    expected_response_ = expected_response;
    TestRequest("GET", net::HttpRequestHeaders());
    EXPECT_EQ(expected_content_length,
              request_->response_headers()->GetContentLength());
  }

  void TestErrorRequest(int expected_status_code) {
    expected_status_code_ = expected_status_code;
    expected_response_ = "";
    TestRequest("GET", net::HttpRequestHeaders());
  }

  void TestRequest(const std::string& method,
                   const net::HttpRequestHeaders& extra_headers) {
    request_ = url_request_context_.CreateRequest(
        GURL("blob:blah"), net::DEFAULT_PRIORITY, &url_request_delegate_);
    request_->set_method(method);
    if (!extra_headers.IsEmpty())
      request_->SetExtraRequestHeaders(extra_headers);
    request_->Start();

    base::MessageLoop::current()->Run();

    // Verify response.
    EXPECT_TRUE(request_->status().is_success());
    EXPECT_EQ(expected_status_code_,
              request_->response_headers()->response_code());
    EXPECT_EQ(expected_response_, url_request_delegate_.response_data());
  }

  void BuildComplicatedData(std::string* expected_result) {
    blob_data_->AppendData(kTestData1 + 1, 2);
    *expected_result = std::string(kTestData1 + 1, 2);

    blob_data_->AppendFile(temp_file1_, 2, 3, temp_file_modification_time1_);
    *expected_result += std::string(kTestFileData1 + 2, 3);

    blob_data_->AppendDiskCacheEntry(new EmptyDataHandle(),
                                     disk_cache_entry_.get(),
                                     kTestDiskCacheStreamIndex);
    *expected_result += std::string(kTestDiskCacheData);

    blob_data_->AppendFileSystemFile(temp_file_system_file1_, 3, 4,
                                     temp_file_system_file_modification_time1_);
    *expected_result += std::string(kTestFileSystemFileData1 + 3, 4);

    blob_data_->AppendData(kTestData2 + 4, 5);
    *expected_result += std::string(kTestData2 + 4, 5);

    blob_data_->AppendFile(temp_file2_, 5, 6, temp_file_modification_time2_);
    *expected_result += std::string(kTestFileData2 + 5, 6);

    blob_data_->AppendFileSystemFile(temp_file_system_file2_, 6, 7,
                                     temp_file_system_file_modification_time2_);
    *expected_result += std::string(kTestFileSystemFileData2 + 6, 7);
  }

  scoped_ptr<BlobDataSnapshot> GetSnapshotFromBuilder() {
    if (!blob_handle_) {
      blob_handle_ = blob_context_.AddFinishedBlob(blob_data_.get()).Pass();
    }
    return blob_handle_->CreateSnapshot().Pass();
  }

  // This only works if all the Blob items have a definite pre-computed length.
  // Otherwise, this will fail a CHECK.
  int64 GetTotalBlobLength() {
    int64 total = 0;
    scoped_ptr<BlobDataSnapshot> data = GetSnapshotFromBuilder();
    const auto& items = data->items();
    for (const auto& item : items) {
      int64 length = base::checked_cast<int64>(item->length());
      CHECK(length <= kint64max - total);
      total += length;
    }
    return total;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath temp_file1_;
  base::FilePath temp_file2_;
  base::Time temp_file_modification_time1_;
  base::Time temp_file_modification_time2_;
  GURL file_system_root_url_;
  GURL temp_file_system_file1_;
  GURL temp_file_system_file2_;
  base::Time temp_file_system_file_modification_time1_;
  base::Time temp_file_system_file_modification_time2_;

  scoped_ptr<disk_cache::Backend> disk_cache_backend_;
  disk_cache::ScopedEntryPtr disk_cache_entry_;

  base::MessageLoopForIO message_loop_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  storage::BlobStorageContext blob_context_;
  scoped_ptr<storage::BlobDataHandle> blob_handle_;
  scoped_ptr<BlobDataBuilder> blob_data_;
  scoped_ptr<BlobDataSnapshot> blob_data_snapshot_;
  net::URLRequestJobFactoryImpl url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  scoped_ptr<net::URLRequest> request_;

  int expected_status_code_;
  std::string expected_response_;
};

TEST_F(BlobURLRequestJobTest, TestGetSimpleDataRequest) {
  blob_data_->AppendData(kTestData1);
  TestSuccessNonrangeRequest(kTestData1, arraysize(kTestData1) - 1);
}

TEST_F(BlobURLRequestJobTest, TestGetSimpleFileRequest) {
  blob_data_->AppendFile(temp_file1_, 0, kuint64max, base::Time());
  TestSuccessNonrangeRequest(kTestFileData1, arraysize(kTestFileData1) - 1);
}

TEST_F(BlobURLRequestJobTest, TestGetLargeFileRequest) {
  base::FilePath large_temp_file =
      temp_dir_.path().AppendASCII("LargeBlob.dat");
  std::string large_data;
  large_data.reserve(kBufferSize * 5);
  for (int i = 0; i < kBufferSize * 5; ++i)
    large_data.append(1, static_cast<char>(i % 256));
  ASSERT_EQ(static_cast<int>(large_data.size()),
            base::WriteFile(large_temp_file, large_data.data(),
                            large_data.size()));
  blob_data_->AppendFile(large_temp_file, 0, kuint64max, base::Time());
  TestSuccessNonrangeRequest(large_data, large_data.size());
}

TEST_F(BlobURLRequestJobTest, TestGetNonExistentFileRequest) {
  base::FilePath non_existent_file =
      temp_file1_.InsertBeforeExtension(FILE_PATH_LITERAL("-na"));
  blob_data_->AppendFile(non_existent_file, 0, kuint64max, base::Time());
  TestErrorRequest(404);
}

TEST_F(BlobURLRequestJobTest, TestGetChangedFileRequest) {
  base::Time old_time =
      temp_file_modification_time1_ - base::TimeDelta::FromSeconds(10);
  blob_data_->AppendFile(temp_file1_, 0, 3, old_time);
  TestErrorRequest(404);
}

TEST_F(BlobURLRequestJobTest, TestGetSlicedFileRequest) {
  blob_data_->AppendFile(temp_file1_, 2, 4, temp_file_modification_time1_);
  std::string result(kTestFileData1 + 2, 4);
  TestSuccessNonrangeRequest(result, 4);
}

TEST_F(BlobURLRequestJobTest, TestGetSimpleFileSystemFileRequest) {
  SetUpFileSystem();
  blob_data_->AppendFileSystemFile(temp_file_system_file1_, 0, kuint64max,
                                   base::Time());
  TestSuccessNonrangeRequest(kTestFileSystemFileData1,
                             arraysize(kTestFileSystemFileData1) - 1);
}

TEST_F(BlobURLRequestJobTest, TestGetLargeFileSystemFileRequest) {
  SetUpFileSystem();
  std::string large_data;
  large_data.reserve(kBufferSize * 5);
  for (int i = 0; i < kBufferSize * 5; ++i)
    large_data.append(1, static_cast<char>(i % 256));

  const char kFilename[] = "LargeBlob.dat";
  WriteFileSystemFile(kFilename, large_data.data(), large_data.size(), NULL);

  blob_data_->AppendFileSystemFile(GetFileSystemURL(kFilename), 0, kuint64max,
                                   base::Time());
  TestSuccessNonrangeRequest(large_data, large_data.size());
}

TEST_F(BlobURLRequestJobTest, TestGetNonExistentFileSystemFileRequest) {
  SetUpFileSystem();
  GURL non_existent_file = GetFileSystemURL("non-existent.dat");
  blob_data_->AppendFileSystemFile(non_existent_file, 0, kuint64max,
                                   base::Time());
  TestErrorRequest(404);
}

TEST_F(BlobURLRequestJobTest, TestGetInvalidFileSystemFileRequest) {
  SetUpFileSystem();
  GURL invalid_file;
  blob_data_->AppendFileSystemFile(invalid_file, 0, kuint64max, base::Time());
  TestErrorRequest(500);
}

TEST_F(BlobURLRequestJobTest, TestGetChangedFileSystemFileRequest) {
  SetUpFileSystem();
  base::Time old_time =
      temp_file_system_file_modification_time1_ -
      base::TimeDelta::FromSeconds(10);
  blob_data_->AppendFileSystemFile(temp_file_system_file1_, 0, 3, old_time);
  TestErrorRequest(404);
}

TEST_F(BlobURLRequestJobTest, TestGetSlicedFileSystemFileRequest) {
  SetUpFileSystem();
  blob_data_->AppendFileSystemFile(temp_file_system_file1_, 2, 4,
                                  temp_file_system_file_modification_time1_);
  std::string result(kTestFileSystemFileData1 + 2, 4);
  TestSuccessNonrangeRequest(result, 4);
}

TEST_F(BlobURLRequestJobTest, TestGetSimpleDiskCacheRequest) {
  blob_data_->AppendDiskCacheEntry(new EmptyDataHandle(),
                                   disk_cache_entry_.get(),
                                   kTestDiskCacheStreamIndex);
  TestSuccessNonrangeRequest(kTestDiskCacheData,
                             arraysize(kTestDiskCacheData) - 1);
}

TEST_F(BlobURLRequestJobTest, TestGetComplicatedDataFileAndDiskCacheRequest) {
  SetUpFileSystem();
  std::string result;
  BuildComplicatedData(&result);
  TestSuccessNonrangeRequest(result, GetTotalBlobLength());
}

TEST_F(BlobURLRequestJobTest, TestGetRangeRequest1) {
  SetUpFileSystem();
  std::string result;
  BuildComplicatedData(&result);
  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange,
                          net::HttpByteRange::Bounded(5, 10).GetHeaderValue());
  expected_status_code_ = 206;
  expected_response_ = result.substr(5, 10 - 5 + 1);
  TestRequest("GET", extra_headers);

  EXPECT_EQ(6, request_->response_headers()->GetContentLength());

  int64 first = 0, last = 0, length = 0;
  EXPECT_TRUE(
      request_->response_headers()->GetContentRange(&first, &last, &length));
  EXPECT_EQ(5, first);
  EXPECT_EQ(10, last);
  EXPECT_EQ(GetTotalBlobLength(), length);
}

TEST_F(BlobURLRequestJobTest, TestGetRangeRequest2) {
  SetUpFileSystem();
  std::string result;
  BuildComplicatedData(&result);
  net::HttpRequestHeaders extra_headers;
  extra_headers.SetHeader(net::HttpRequestHeaders::kRange,
                          net::HttpByteRange::Suffix(10).GetHeaderValue());
  expected_status_code_ = 206;
  expected_response_ = result.substr(result.length() - 10);
  TestRequest("GET", extra_headers);

  EXPECT_EQ(10, request_->response_headers()->GetContentLength());

  int64 total = GetTotalBlobLength();
  int64 first = 0, last = 0, length = 0;
  EXPECT_TRUE(
      request_->response_headers()->GetContentRange(&first, &last, &length));
  EXPECT_EQ(total - 10, first);
  EXPECT_EQ(total - 1, last);
  EXPECT_EQ(total, length);
}

TEST_F(BlobURLRequestJobTest, TestExtraHeaders) {
  blob_data_->set_content_type(kTestContentType);
  blob_data_->set_content_disposition(kTestContentDisposition);
  blob_data_->AppendData(kTestData1);
  expected_status_code_ = 200;
  expected_response_ = kTestData1;
  TestRequest("GET", net::HttpRequestHeaders());

  std::string content_type;
  EXPECT_TRUE(request_->response_headers()->GetMimeType(&content_type));
  EXPECT_EQ(kTestContentType, content_type);
  void* iter = NULL;
  std::string content_disposition;
  EXPECT_TRUE(request_->response_headers()->EnumerateHeader(
      &iter, "Content-Disposition", &content_disposition));
  EXPECT_EQ(kTestContentDisposition, content_disposition);
}

}  // namespace content
