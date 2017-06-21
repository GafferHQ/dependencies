// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/public/browser/indexed_db_context.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

class GURL;

namespace base {
class ListValue;
class FilePath;
class SequencedTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class IndexedDBConnection;

class CONTENT_EXPORT IndexedDBContextImpl
    : NON_EXPORTED_BASE(public IndexedDBContext) {
 public:
  // Recorded in histograms, so append only.
  enum ForceCloseReason {
    FORCE_CLOSE_DELETE_ORIGIN = 0,
    FORCE_CLOSE_BACKING_STORE_FAILURE,
    FORCE_CLOSE_INTERNALS_PAGE,
    FORCE_CLOSE_COPY_ORIGIN,
    FORCE_CLOSE_REASON_MAX
  };

  // The indexed db directory.
  static const base::FilePath::CharType kIndexedDBDirectory[];

  // If |data_path| is empty, nothing will be saved to disk.
  IndexedDBContextImpl(const base::FilePath& data_path,
                       storage::SpecialStoragePolicy* special_storage_policy,
                       storage::QuotaManagerProxy* quota_manager_proxy,
                       base::SequencedTaskRunner* task_runner);

  IndexedDBFactory* GetIDBFactory();

  // Disables the exit-time deletion of session-only data.
  void SetForceKeepSessionState() { force_keep_session_state_ = true; }

  // IndexedDBContext implementation:
  base::SequencedTaskRunner* TaskRunner() const override;
  std::vector<IndexedDBInfo> GetAllOriginsInfo() override;
  int64 GetOriginDiskUsage(const GURL& origin_url) override;
  void DeleteForOrigin(const GURL& origin_url) override;
  void CopyOriginData(const GURL& origin_url,
                      IndexedDBContext* dest_context) override;
  base::FilePath GetFilePathForTesting(
      const std::string& origin_id) const override;
  void SetTaskRunnerForTesting(base::SequencedTaskRunner* task_runner) override;

  // Methods called by IndexedDBDispatcherHost for quota support.
  void ConnectionOpened(const GURL& origin_url, IndexedDBConnection* db);
  void ConnectionClosed(const GURL& origin_url, IndexedDBConnection* db);
  void TransactionComplete(const GURL& origin_url);
  void DatabaseDeleted(const GURL& origin_url);
  bool WouldBeOverQuota(const GURL& origin_url, int64 additional_bytes);
  bool IsOverQuota(const GURL& origin_url);

  storage::QuotaManagerProxy* quota_manager_proxy();

  std::vector<GURL> GetAllOrigins();
  base::Time GetOriginLastModified(const GURL& origin_url);
  base::ListValue* GetAllOriginsDetails();

  // ForceClose takes a value rather than a reference since it may release the
  // owning object.
  void ForceClose(const GURL origin_url, ForceCloseReason reason);
  // GetStoragePaths returns all paths owned by this database, in arbitrary
  // order.
  std::vector<base::FilePath> GetStoragePaths(const GURL& origin_url) const;

  base::FilePath data_path() const { return data_path_; }
  bool IsInOriginSet(const GURL& origin_url) {
    std::set<GURL>* set = GetOriginSet();
    return set->find(origin_url) != set->end();
  }
  size_t GetConnectionCount(const GURL& origin_url);
  int GetOriginBlobFileCount(const GURL& origin_url);

  // For unit tests allow to override the |data_path_|.
  void set_data_path_for_testing(const base::FilePath& data_path) {
    data_path_ = data_path;
  }

  bool is_incognito() const { return data_path_.empty(); }

 protected:
  ~IndexedDBContextImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearLocalState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ClearSessionOnlyDatabases);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, SetForceKeepSessionState);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, ForceCloseOpenDatabasesOnDelete);
  friend class IndexedDBQuotaClientTest;

  typedef std::map<GURL, int64> OriginToSizeMap;
  class IndexedDBGetUsageAndQuotaCallback;

  base::FilePath GetBlobPath(const std::string& origin_id) const;
  base::FilePath GetLevelDBPath(const GURL& origin_url) const;
  base::FilePath GetLevelDBPath(const std::string& origin_id) const;
  int64 ReadUsageFromDisk(const GURL& origin_url) const;
  void EnsureDiskUsageCacheInitialized(const GURL& origin_url);
  void QueryDiskAndUpdateQuotaUsage(const GURL& origin_url);
  void GotUsageAndQuota(const GURL& origin_url,
                        storage::QuotaStatusCode,
                        int64 usage,
                        int64 quota);
  void GotUpdatedQuota(const GURL& origin_url, int64 usage, int64 quota);
  void QueryAvailableQuota(const GURL& origin_url);

  std::set<GURL>* GetOriginSet();
  bool AddToOriginSet(const GURL& origin_url) {
    return GetOriginSet()->insert(origin_url).second;
  }
  void RemoveFromOriginSet(const GURL& origin_url) {
    GetOriginSet()->erase(origin_url);
  }

  // Only for testing.
  void ResetCaches();

  scoped_refptr<IndexedDBFactory> factory_;
  base::FilePath data_path_;
  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<std::set<GURL> > origin_set_;
  OriginToSizeMap origin_size_map_;
  OriginToSizeMap space_available_map_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBContextImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONTEXT_IMPL_H_
