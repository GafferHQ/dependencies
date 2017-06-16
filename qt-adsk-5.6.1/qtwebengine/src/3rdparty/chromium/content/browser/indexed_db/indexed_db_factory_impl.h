// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_

#include <map>
#include <set>
#include <string>

#include "content/browser/indexed_db/indexed_db_factory.h"

namespace content {

class IndexedDBContextImpl;

class CONTENT_EXPORT IndexedDBFactoryImpl : public IndexedDBFactory {
 public:
  explicit IndexedDBFactoryImpl(IndexedDBContextImpl* context);

  // content::IndexedDBFactory overrides:
  void ReleaseDatabase(const IndexedDBDatabase::Identifier& identifier,
                       bool forced_close) override;

  void GetDatabaseNames(scoped_refptr<IndexedDBCallbacks> callbacks,
                        const GURL& origin_url,
                        const base::FilePath& data_directory,
                        net::URLRequestContext* request_context) override;
  void Open(const base::string16& name,
            const IndexedDBPendingConnection& connection,
            net::URLRequestContext* request_context,
            const GURL& origin_url,
            const base::FilePath& data_directory) override;

  void DeleteDatabase(const base::string16& name,
                      net::URLRequestContext* request_context,
                      scoped_refptr<IndexedDBCallbacks> callbacks,
                      const GURL& origin_url,
                      const base::FilePath& data_directory) override;

  void HandleBackingStoreFailure(const GURL& origin_url) override;
  void HandleBackingStoreCorruption(
      const GURL& origin_url,
      const IndexedDBDatabaseError& error) override;

  OriginDBs GetOpenDatabasesForOrigin(const GURL& origin_url) const override;

  void ForceClose(const GURL& origin_url) override;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  void ContextDestroyed() override;

  // Called by the IndexedDBActiveBlobRegistry.
  void ReportOutstandingBlobs(const GURL& origin_url,
                              bool blobs_outstanding) override;

  // Called by an IndexedDBDatabase when it is actually deleted.
  void DatabaseDeleted(
      const IndexedDBDatabase::Identifier& identifier) override;

  size_t GetConnectionCount(const GURL& origin_url) const override;

 protected:
  ~IndexedDBFactoryImpl() override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const GURL& origin_url,
      const base::FilePath& data_directory,
      net::URLRequestContext* request_context,
      blink::WebIDBDataLoss* data_loss,
      std::string* data_loss_reason,
      bool* disk_full,
      leveldb::Status* s) override;

  scoped_refptr<IndexedDBBackingStore> OpenBackingStoreHelper(
      const GURL& origin_url,
      const base::FilePath& data_directory,
      net::URLRequestContext* request_context,
      blink::WebIDBDataLoss* data_loss,
      std::string* data_loss_message,
      bool* disk_full,
      bool first_time,
      leveldb::Status* s) override;

  void ReleaseBackingStore(const GURL& origin_url, bool immediate);
  void CloseBackingStore(const GURL& origin_url);
  IndexedDBContextImpl* context() const { return context_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleasedOnForcedClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           BackingStoreReleaseDelayedOnClose);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest, DatabaseFailedOpen);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           DeleteDatabaseClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           ForceCloseReleasesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBFactoryTest,
                           GetDatabaseNamesClosesBackingStore);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest,
                           ForceCloseOpenDatabasesOnCommitFailure);

  typedef std::map<IndexedDBDatabase::Identifier, IndexedDBDatabase*>
      IndexedDBDatabaseMap;
  typedef std::map<GURL, scoped_refptr<IndexedDBBackingStore> >
      IndexedDBBackingStoreMap;

  // Called internally after a database is closed, with some delay. If this
  // factory has the last reference, it will be released.
  void MaybeCloseBackingStore(const GURL& origin_url);
  bool HasLastBackingStoreReference(const GURL& origin_url) const;

  // Testing helpers, so unit tests don't need to grovel through internal state.
  bool IsDatabaseOpen(const GURL& origin_url, const base::string16& name) const;
  bool IsBackingStoreOpen(const GURL& origin_url) const;
  bool IsBackingStorePendingClose(const GURL& origin_url) const;
  void RemoveDatabaseFromMaps(const IndexedDBDatabase::Identifier& identifier);

  IndexedDBContextImpl* context_;

  IndexedDBDatabaseMap database_map_;
  OriginDBMap origin_dbs_;
  IndexedDBBackingStoreMap backing_store_map_;

  std::set<scoped_refptr<IndexedDBBackingStore> > session_only_backing_stores_;
  IndexedDBBackingStoreMap backing_stores_with_active_blobs_;
  std::set<GURL> backends_opened_since_boot_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_IMPL_H_
