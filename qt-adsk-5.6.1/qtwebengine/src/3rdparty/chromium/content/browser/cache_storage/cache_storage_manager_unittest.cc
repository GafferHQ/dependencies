// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/quota/mock_quota_manager_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class CacheStorageManagerTest : public testing::Test {
 public:
  CacheStorageManagerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        callback_bool_(false),
        callback_error_(CACHE_STORAGE_OK),
        origin1_("http://example1.com"),
        origin2_("http://example2.com") {}

  void SetUp() override {
    ChromeBlobStorageContext* blob_storage_context(
        ChromeBlobStorageContext::GetFor(&browser_context_));
    // Wait for ChromeBlobStorageContext to finish initializing.
    base::RunLoop().RunUntilIdle();

    quota_manager_proxy_ = new MockQuotaManagerProxy(
        nullptr, base::ThreadTaskRunnerHandle::Get().get());

    if (MemoryOnly()) {
      cache_manager_ = CacheStorageManager::Create(
          base::FilePath(), base::ThreadTaskRunnerHandle::Get(),
          quota_manager_proxy_);
    } else {
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      cache_manager_ = CacheStorageManager::Create(
          temp_dir_.path(), base::ThreadTaskRunnerHandle::Get(),
          quota_manager_proxy_);
    }

    cache_manager_->SetBlobParametersForCache(
        browser_context_.GetRequestContext(),
        blob_storage_context->context()->AsWeakPtr());
  }

  void TearDown() override {
    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  virtual bool MemoryOnly() { return false; }

  void BoolAndErrorCallback(base::RunLoop* run_loop,
                            bool value,
                            CacheStorageError error) {
    callback_bool_ = value;
    callback_error_ = error;
    run_loop->Quit();
  }

  void CacheAndErrorCallback(base::RunLoop* run_loop,
                             const scoped_refptr<CacheStorageCache>& cache,
                             CacheStorageError error) {
    callback_cache_ = cache;
    callback_error_ = error;
    run_loop->Quit();
  }

  void StringsAndErrorCallback(base::RunLoop* run_loop,
                               const std::vector<std::string>& strings,
                               CacheStorageError error) {
    callback_strings_ = strings;
    callback_error_ = error;
    run_loop->Quit();
  }

  void CachePutCallback(base::RunLoop* run_loop, CacheStorageError error) {
    callback_error_ = error;
    run_loop->Quit();
  }

  void CacheMatchCallback(
      base::RunLoop* run_loop,
      CacheStorageError error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
    callback_error_ = error;
    callback_cache_response_ = response.Pass();
    // Deliberately drop the data handle as only the url is being tested.
    run_loop->Quit();
  }

  bool Open(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->OpenCache(
        origin, cache_name,
        base::Bind(&CacheStorageManagerTest::CacheAndErrorCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    bool error = callback_error_ != CACHE_STORAGE_OK;
    if (error)
      EXPECT_TRUE(!callback_cache_.get());
    else
      EXPECT_TRUE(callback_cache_.get());
    return !error;
  }

  bool Has(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->HasCache(
        origin, cache_name,
        base::Bind(&CacheStorageManagerTest::BoolAndErrorCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_bool_;
  }

  bool Delete(const GURL& origin, const std::string& cache_name) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->DeleteCache(
        origin, cache_name,
        base::Bind(&CacheStorageManagerTest::BoolAndErrorCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_bool_;
  }

  bool Keys(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->EnumerateCaches(
        origin,
        base::Bind(&CacheStorageManagerTest::StringsAndErrorCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool StorageMatch(const GURL& origin,
                    const std::string& cache_name,
                    const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->MatchCache(
        origin, cache_name, request.Pass(),
        base::Bind(&CacheStorageManagerTest::CacheMatchCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool StorageMatchAll(const GURL& origin, const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->MatchAllCaches(
        origin, request.Pass(),
        base::Bind(&CacheStorageManagerTest::CacheMatchCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool CachePut(const scoped_refptr<CacheStorageCache>& cache,
                const GURL& url) {
    ServiceWorkerFetchRequest request;
    ServiceWorkerResponse response;
    request.url = url;
    response.url = url;

    CacheStorageBatchOperation operation;
    operation.operation_type = CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
    operation.request = request;
    operation.response = response;

    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache->BatchOperation(
        std::vector<CacheStorageBatchOperation>(1, operation),
        base::Bind(&CacheStorageManagerTest::CachePutCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  bool CacheMatch(const scoped_refptr<CacheStorageCache>& cache,
                  const GURL& url) {
    scoped_ptr<ServiceWorkerFetchRequest> request(
        new ServiceWorkerFetchRequest());
    request->url = url;
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache->Match(
        request.Pass(),
        base::Bind(&CacheStorageManagerTest::CacheMatchCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();

    return callback_error_ == CACHE_STORAGE_OK;
  }

  CacheStorage* CacheStorageForOrigin(const GURL& origin) {
    return cache_manager_->FindOrCreateCacheStorage(origin);
  }

 protected:
  TestBrowserContext browser_context_;
  TestBrowserThreadBundle browser_thread_bundle_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_ptr<CacheStorageManager> cache_manager_;

  scoped_refptr<CacheStorageCache> callback_cache_;
  int callback_bool_;
  CacheStorageError callback_error_;
  scoped_ptr<ServiceWorkerResponse> callback_cache_response_;
  std::vector<std::string> callback_strings_;

  const GURL origin1_;
  const GURL origin2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageManagerTest);
};

class CacheStorageManagerMemoryOnlyTest : public CacheStorageManagerTest {
  bool MemoryOnly() override { return true; }
};

class CacheStorageManagerTestP : public CacheStorageManagerTest,
                                 public testing::WithParamInterface<bool> {
  bool MemoryOnly() override { return !GetParam(); }
};

TEST_F(CacheStorageManagerTest, TestsRunOnIOThread) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

TEST_P(CacheStorageManagerTestP, OpenCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, OpenTwoCaches) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
}

TEST_P(CacheStorageManagerTestP, CachePointersDiffer) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_NE(callback_cache_.get(), cache.get());
}

TEST_P(CacheStorageManagerTestP, Open2CachesSameNameDiffOrigins) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin2_, "foo"));
  EXPECT_NE(cache.get(), callback_cache_.get());
}

TEST_P(CacheStorageManagerTestP, OpenExistingCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_EQ(callback_cache_.get(), cache.get());
}

TEST_P(CacheStorageManagerTestP, HasCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Has(origin1_, "foo"));
  EXPECT_TRUE(callback_bool_);
}

TEST_P(CacheStorageManagerTestP, HasNonExistent) {
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Has(origin1_, "foo"));
}

TEST_P(CacheStorageManagerTestP, DeleteTwice) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(Delete(origin1_, "foo"));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, EmptyKeys) {
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_TRUE(callback_strings_.empty());
}

TEST_P(CacheStorageManagerTestP, SomeKeys) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("bar");
  EXPECT_EQ(expected_keys, callback_strings_);
  EXPECT_TRUE(Keys(origin2_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("baz", callback_strings_[0].c_str());
}

TEST_P(CacheStorageManagerTestP, DeletedKeysGone) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

TEST_P(CacheStorageManagerTestP, StorageMatchEntryExists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(StorageMatch(origin1_, "foo", GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoEntry) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatch(origin1_, "foo", GURL("http://example.com/bar")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchNoCache) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatch(origin1_, "bar", GURL("http://example.com/foo")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoEntry) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/bar")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllNoCaches) {
  EXPECT_FALSE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
  EXPECT_EQ(CACHE_STORAGE_ERROR_NOT_FOUND, callback_error_);
}

TEST_P(CacheStorageManagerTestP, StorageMatchAllEntryExistsTwice) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));

  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, StorageMatchInOneOfMany) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "baz"));

  EXPECT_TRUE(StorageMatchAll(origin1_, GURL("http://example.com/foo")));
}

TEST_P(CacheStorageManagerTestP, Chinese) {
  EXPECT_TRUE(Open(origin1_, "你好"));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "你好"));
  EXPECT_EQ(callback_cache_.get(), cache.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("你好", callback_strings_[0].c_str());
}

TEST_F(CacheStorageManagerTest, EmptyKey) {
  EXPECT_TRUE(Open(origin1_, ""));
  scoped_refptr<CacheStorageCache> cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, ""));
  EXPECT_EQ(cache.get(), callback_cache_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("", callback_strings_[0].c_str());
  EXPECT_TRUE(Has(origin1_, ""));
  EXPECT_TRUE(Delete(origin1_, ""));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_F(CacheStorageManagerTest, DataPersists) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin1_, "baz"));
  EXPECT_TRUE(Open(origin2_, "raz"));
  EXPECT_TRUE(Delete(origin1_, "bar"));
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(2u, callback_strings_.size());
  std::vector<std::string> expected_keys;
  expected_keys.push_back("foo");
  expected_keys.push_back("baz");
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_F(CacheStorageManagerMemoryOnlyTest, DataLostWhenMemoryOnly) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin2_, "baz"));
  quota_manager_proxy_->SimulateQuotaManagerDestroyed();
  cache_manager_ = CacheStorageManager::Create(cache_manager_.get());
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(0u, callback_strings_.size());
}

TEST_F(CacheStorageManagerTest, BadCacheName) {
  // Since the implementation writes cache names to disk, ensure that we don't
  // escape the directory.
  const std::string bad_name = "../../../../../../../../../../../../../../foo";
  EXPECT_TRUE(Open(origin1_, bad_name));
  EXPECT_TRUE(Keys(origin1_));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ(bad_name.c_str(), callback_strings_[0].c_str());
}

TEST_F(CacheStorageManagerTest, BadOriginName) {
  // Since the implementation writes origin names to disk, ensure that we don't
  // escape the directory.
  GURL bad_origin("http://../../../../../../../../../../../../../../foo");
  EXPECT_TRUE(Open(bad_origin, "foo"));
  EXPECT_TRUE(Keys(bad_origin));
  EXPECT_EQ(1u, callback_strings_.size());
  EXPECT_STREQ("foo", callback_strings_[0].c_str());
}

// With a persistent cache if the client drops its reference to a
// CacheStorageCache
// it should be deleted.
TEST_F(CacheStorageManagerTest, DropReference) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache = callback_cache_->AsWeakPtr();
  callback_cache_ = NULL;
  EXPECT_TRUE(!cache);
}

// With a memory cache the cache can't be freed from memory until the client
// calls delete.
TEST_F(CacheStorageManagerMemoryOnlyTest, MemoryLosesReferenceOnlyAfterDelete) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  base::WeakPtr<CacheStorageCache> cache = callback_cache_->AsWeakPtr();
  callback_cache_ = NULL;
  EXPECT_TRUE(cache);
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_FALSE(cache);
}

TEST_P(CacheStorageManagerTestP, DeleteBeforeRelease) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Delete(origin1_, "foo"));
  EXPECT_TRUE(callback_cache_->AsWeakPtr());
}

TEST_P(CacheStorageManagerTestP, OpenRunsSerially) {
  EXPECT_FALSE(Delete(origin1_, "tmp"));  // Init storage.
  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  cache_storage->StartAsyncOperationForTesting();

  scoped_ptr<base::RunLoop> open_loop(new base::RunLoop());
  cache_manager_->OpenCache(
      origin1_, "foo",
      base::Bind(&CacheStorageManagerTest::CacheAndErrorCallback,
                 base::Unretained(this), base::Unretained(open_loop.get())));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback_cache_);

  cache_storage->CompleteAsyncOperationForTesting();
  open_loop->Run();
  EXPECT_TRUE(callback_cache_);
}

TEST_F(CacheStorageManagerMemoryOnlyTest, MemoryBackedSize) {
  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());

  EXPECT_TRUE(Open(origin1_, "foo"));
  scoped_refptr<CacheStorageCache> foo_cache = callback_cache_;
  EXPECT_TRUE(Open(origin1_, "bar"));
  scoped_refptr<CacheStorageCache> bar_cache = callback_cache_;
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());

  EXPECT_TRUE(CachePut(foo_cache, GURL("http://example.com/foo")));
  EXPECT_LT(0, cache_storage->MemoryBackedSize());
  int64 foo_size = cache_storage->MemoryBackedSize();

  EXPECT_TRUE(CachePut(bar_cache, GURL("http://example.com/foo")));
  EXPECT_EQ(foo_size * 2, cache_storage->MemoryBackedSize());
}

TEST_F(CacheStorageManagerTest, MemoryBackedSizePersistent) {
  CacheStorage* cache_storage = CacheStorageForOrigin(origin1_);
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_EQ(0, cache_storage->MemoryBackedSize());
}

class CacheStorageMigrationTest : public CacheStorageManagerTest {
 protected:
  CacheStorageMigrationTest() : cache1_("foo"), cache2_("bar") {}

  void SetUp() override {
    CacheStorageManagerTest::SetUp();

    // Populate a cache, then move it to the "legacy" location
    // so that tests can verify the results of migration.
    legacy_path_ = CacheStorageManager::ConstructLegacyOriginPath(
        cache_manager_->root_path(), origin1_);
    new_path_ = CacheStorageManager::ConstructOriginPath(
        cache_manager_->root_path(), origin1_);

    ASSERT_FALSE(base::DirectoryExists(legacy_path_));
    ASSERT_FALSE(base::DirectoryExists(new_path_));
    ASSERT_TRUE(Open(origin1_, cache1_));
    ASSERT_TRUE(Open(origin1_, cache2_));
    callback_cache_ = nullptr;
    ASSERT_FALSE(base::DirectoryExists(legacy_path_));
    ASSERT_TRUE(base::DirectoryExists(new_path_));

    quota_manager_proxy_->SimulateQuotaManagerDestroyed();
    cache_manager_ = CacheStorageManager::Create(cache_manager_.get());

    ASSERT_TRUE(base::Move(new_path_, legacy_path_));
    ASSERT_TRUE(base::DirectoryExists(legacy_path_));
    ASSERT_FALSE(base::DirectoryExists(new_path_));
  }

  int64 GetOriginUsage(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    cache_manager_->GetOriginUsage(
        origin,
        base::Bind(&CacheStorageMigrationTest::UsageCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();
    return callback_usage_;
  }

  void UsageCallback(base::RunLoop* run_loop, int64 usage) {
    callback_usage_ = usage;
    run_loop->Quit();
  }

  base::FilePath legacy_path_;
  base::FilePath new_path_;

  const std::string cache1_;
  const std::string cache2_;

  int64 callback_usage_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageMigrationTest);
};

TEST_F(CacheStorageMigrationTest, OpenCache) {
  EXPECT_TRUE(Open(origin1_, cache1_));
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));

  EXPECT_TRUE(Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back(cache1_);
  expected_keys.push_back(cache2_);
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_F(CacheStorageMigrationTest, DeleteCache) {
  EXPECT_TRUE(Delete(origin1_, cache1_));
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));

  EXPECT_TRUE(Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back(cache2_);
  EXPECT_EQ(expected_keys, callback_strings_);
}

TEST_F(CacheStorageMigrationTest, GetOriginUsage) {
  EXPECT_GT(GetOriginUsage(origin1_), 0);
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));
}

TEST_F(CacheStorageMigrationTest, MoveFailure) {
  // Revert the migration.
  ASSERT_TRUE(base::Move(legacy_path_, new_path_));
  ASSERT_FALSE(base::DirectoryExists(legacy_path_));
  ASSERT_TRUE(base::DirectoryExists(new_path_));

  // Make a dummy legacy directory.
  ASSERT_TRUE(base::CreateDirectory(legacy_path_));

  // Ensure that migration doesn't stomp existing new directory,
  // but does clean up old directory.
  EXPECT_TRUE(Open(origin1_, cache1_));
  EXPECT_FALSE(base::DirectoryExists(legacy_path_));
  EXPECT_TRUE(base::DirectoryExists(new_path_));

  EXPECT_TRUE(Keys(origin1_));
  std::vector<std::string> expected_keys;
  expected_keys.push_back(cache1_);
  expected_keys.push_back(cache2_);
  EXPECT_EQ(expected_keys, callback_strings_);
}

class CacheStorageQuotaClientTest : public CacheStorageManagerTest {
 protected:
  CacheStorageQuotaClientTest() {}

  void SetUp() override {
    CacheStorageManagerTest::SetUp();
    quota_client_.reset(
        new CacheStorageQuotaClient(cache_manager_->AsWeakPtr()));
  }

  void UsageCallback(base::RunLoop* run_loop, int64 usage) {
    callback_usage_ = usage;
    run_loop->Quit();
  }

  void OriginsCallback(base::RunLoop* run_loop, const std::set<GURL>& origins) {
    callback_origins_ = origins;
    run_loop->Quit();
  }

  void DeleteOriginCallback(base::RunLoop* run_loop,
                            storage::QuotaStatusCode status) {
    callback_status_ = status;
    run_loop->Quit();
  }

  int64 QuotaGetOriginUsage(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->GetOriginUsage(
        origin, storage::kStorageTypeTemporary,
        base::Bind(&CacheStorageQuotaClientTest::UsageCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();
    return callback_usage_;
  }

  size_t QuotaGetOriginsForType() {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->GetOriginsForType(
        storage::kStorageTypeTemporary,
        base::Bind(&CacheStorageQuotaClientTest::OriginsCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();
    return callback_origins_.size();
  }

  size_t QuotaGetOriginsForHost(const std::string& host) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->GetOriginsForHost(
        storage::kStorageTypeTemporary, host,
        base::Bind(&CacheStorageQuotaClientTest::OriginsCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();
    return callback_origins_.size();
  }

  bool QuotaDeleteOriginData(const GURL& origin) {
    scoped_ptr<base::RunLoop> loop(new base::RunLoop());
    quota_client_->DeleteOriginData(
        origin, storage::kStorageTypeTemporary,
        base::Bind(&CacheStorageQuotaClientTest::DeleteOriginCallback,
                   base::Unretained(this), base::Unretained(loop.get())));
    loop->Run();
    return callback_status_ == storage::kQuotaStatusOk;
  }

  bool QuotaDoesSupport(storage::StorageType type) {
    return quota_client_->DoesSupport(type);
  }

  scoped_ptr<CacheStorageQuotaClient> quota_client_;

  storage::QuotaStatusCode callback_status_;
  int64 callback_usage_;
  std::set<GURL> callback_origins_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClientTest);
};

class CacheStorageQuotaClientTestP : public CacheStorageQuotaClientTest,
                                     public testing::WithParamInterface<bool> {
  bool MemoryOnly() override { return !GetParam(); }
};

TEST_P(CacheStorageQuotaClientTestP, QuotaID) {
  EXPECT_EQ(storage::QuotaClient::kServiceWorkerCache, quota_client_->id());
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginUsage) {
  EXPECT_EQ(0, QuotaGetOriginUsage(origin1_));
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_LT(0, QuotaGetOriginUsage(origin1_));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginsForType) {
  EXPECT_EQ(0u, QuotaGetOriginsForType());
  EXPECT_TRUE(Open(origin1_, "foo"));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "foo"));
  EXPECT_EQ(2u, QuotaGetOriginsForType());
}

TEST_P(CacheStorageQuotaClientTestP, QuotaGetOriginsForHost) {
  EXPECT_EQ(0u, QuotaGetOriginsForHost("example.com"));
  EXPECT_TRUE(Open(GURL("http://example.com:8080"), "foo"));
  EXPECT_TRUE(Open(GURL("http://example.com:9000"), "foo"));
  EXPECT_TRUE(Open(GURL("ftp://example.com"), "foo"));
  EXPECT_TRUE(Open(GURL("http://example2.com"), "foo"));
  EXPECT_EQ(3u, QuotaGetOriginsForHost("example.com"));
  EXPECT_EQ(1u, QuotaGetOriginsForHost("example2.com"));
  EXPECT_TRUE(callback_origins_.find(GURL("http://example2.com")) !=
              callback_origins_.end());
  EXPECT_EQ(0u, QuotaGetOriginsForHost("unknown.com"));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteOriginData) {
  EXPECT_TRUE(Open(origin1_, "foo"));
  // Call put to test that initialized caches are properly deleted too.
  EXPECT_TRUE(CachePut(callback_cache_, GURL("http://example.com/foo")));
  EXPECT_TRUE(Open(origin1_, "bar"));
  EXPECT_TRUE(Open(origin2_, "baz"));

  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));

  EXPECT_FALSE(Has(origin1_, "foo"));
  EXPECT_FALSE(Has(origin1_, "bar"));
  EXPECT_TRUE(Has(origin2_, "baz"));
  EXPECT_TRUE(Open(origin1_, "foo"));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDeleteEmptyOrigin) {
  EXPECT_TRUE(QuotaDeleteOriginData(origin1_));
}

TEST_P(CacheStorageQuotaClientTestP, QuotaDoesSupport) {
  EXPECT_TRUE(QuotaDoesSupport(storage::kStorageTypeTemporary));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypePersistent));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeSyncable));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeQuotaNotManaged));
  EXPECT_FALSE(QuotaDoesSupport(storage::kStorageTypeUnknown));
}

INSTANTIATE_TEST_CASE_P(CacheStorageManagerTests,
                        CacheStorageManagerTestP,
                        ::testing::Values(false, true));

INSTANTIATE_TEST_CASE_P(CacheStorageQuotaClientTests,
                        CacheStorageQuotaClientTestP,
                        ::testing::Values(false, true));

}  // namespace content
