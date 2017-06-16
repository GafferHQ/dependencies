// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_factory_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction_coordinator.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseException.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::ASCIIToUTF16;

namespace content {

const int64 kBackingStoreGracePeriodMs = 2000;

IndexedDBFactoryImpl::IndexedDBFactoryImpl(IndexedDBContextImpl* context)
    : context_(context) {
}

IndexedDBFactoryImpl::~IndexedDBFactoryImpl() {
}

void IndexedDBFactoryImpl::RemoveDatabaseFromMaps(
    const IndexedDBDatabase::Identifier& identifier) {
  IndexedDBDatabaseMap::iterator it = database_map_.find(identifier);
  DCHECK(it != database_map_.end());
  IndexedDBDatabase* database = it->second;
  database_map_.erase(it);

  std::pair<OriginDBMap::iterator, OriginDBMap::iterator> range =
      origin_dbs_.equal_range(database->identifier().first);
  DCHECK(range.first != range.second);
  for (OriginDBMap::iterator it2 = range.first; it2 != range.second; ++it2) {
    if (it2->second == database) {
      origin_dbs_.erase(it2);
      break;
    }
  }
}

void IndexedDBFactoryImpl::ReleaseDatabase(
    const IndexedDBDatabase::Identifier& identifier,
    bool forced_close) {
  DCHECK(!database_map_.find(identifier)->second->backing_store());

  RemoveDatabaseFromMaps(identifier);

  // No grace period on a forced-close, as the initiator is
  // assuming the backing store will be released once all
  // connections are closed.
  ReleaseBackingStore(identifier.first, forced_close);
}

void IndexedDBFactoryImpl::ReleaseBackingStore(const GURL& origin_url,
                                               bool immediate) {
  if (immediate) {
    IndexedDBBackingStoreMap::iterator it =
        backing_stores_with_active_blobs_.find(origin_url);
    if (it != backing_stores_with_active_blobs_.end()) {
      it->second->active_blob_registry()->ForceShutdown();
      backing_stores_with_active_blobs_.erase(it);
    }
  }

  // Only close if this is the last reference.
  if (!HasLastBackingStoreReference(origin_url))
    return;

  // If this factory does hold the last reference to the backing store, it can
  // be closed - but unless requested to close it immediately, keep it around
  // for a short period so that a re-open is fast.
  if (immediate) {
    CloseBackingStore(origin_url);
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  DCHECK(!backing_store_map_[origin_url]->close_timer()->IsRunning());
  backing_store_map_[origin_url]->close_timer()->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kBackingStoreGracePeriodMs),
      base::Bind(
          &IndexedDBFactoryImpl::MaybeCloseBackingStore, this, origin_url));
}

void IndexedDBFactoryImpl::MaybeCloseBackingStore(const GURL& origin_url) {
  // Another reference may have opened since the maybe-close was posted, so it
  // is necessary to check again.
  if (HasLastBackingStoreReference(origin_url))
    CloseBackingStore(origin_url);
}

void IndexedDBFactoryImpl::CloseBackingStore(const GURL& origin_url) {
  IndexedDBBackingStoreMap::iterator it = backing_store_map_.find(origin_url);
  DCHECK(it != backing_store_map_.end());
  // Stop the timer (if it's running) - this may happen if the timer was started
  // and then a forced close occurs.
  it->second->close_timer()->Stop();
  backing_store_map_.erase(it);
}

bool IndexedDBFactoryImpl::HasLastBackingStoreReference(
    const GURL& origin_url) const {
  IndexedDBBackingStore* ptr;
  {
    // Scope so that the implicit scoped_refptr<> is freed.
    IndexedDBBackingStoreMap::const_iterator it =
        backing_store_map_.find(origin_url);
    DCHECK(it != backing_store_map_.end());
    ptr = it->second.get();
  }
  return ptr->HasOneRef();
}

void IndexedDBFactoryImpl::ForceClose(const GURL& origin_url) {
  OriginDBs range = GetOpenDatabasesForOrigin(origin_url);

  while (range.first != range.second) {
    IndexedDBDatabase* db = range.first->second;
    ++range.first;
    db->ForceClose();
  }

  if (backing_store_map_.find(origin_url) != backing_store_map_.end())
    ReleaseBackingStore(origin_url, true /* immediate */);
}

void IndexedDBFactoryImpl::ContextDestroyed() {
  // Timers on backing stores hold a reference to this factory. When the
  // context (which nominally owns this factory) is destroyed during thread
  // termination the timers must be stopped so that this factory and the
  // stores can be disposed of.
  for (const auto& it : backing_store_map_)
    it.second->close_timer()->Stop();
  backing_store_map_.clear();
  backing_stores_with_active_blobs_.clear();
  context_ = NULL;
}

void IndexedDBFactoryImpl::ReportOutstandingBlobs(const GURL& origin_url,
                                                  bool blobs_outstanding) {
  if (!context_)
    return;
  if (blobs_outstanding) {
    DCHECK(!backing_stores_with_active_blobs_.count(origin_url));
    IndexedDBBackingStoreMap::iterator it = backing_store_map_.find(origin_url);
    if (it != backing_store_map_.end())
      backing_stores_with_active_blobs_.insert(*it);
    else
      DCHECK(false);
  } else {
    IndexedDBBackingStoreMap::iterator it =
        backing_stores_with_active_blobs_.find(origin_url);
    if (it != backing_stores_with_active_blobs_.end()) {
      backing_stores_with_active_blobs_.erase(it);
      ReleaseBackingStore(origin_url, false /* immediate */);
    }
  }
}

void IndexedDBFactoryImpl::GetDatabaseNames(
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const GURL& origin_url,
    const base::FilePath& data_directory,
    net::URLRequestContext* request_context) {
  IDB_TRACE("IndexedDBFactoryImpl::GetDatabaseNames");
  // TODO(dgrogan): Plumb data_loss back to script eventually?
  blink::WebIDBDataLoss data_loss;
  std::string data_loss_message;
  bool disk_full;
  leveldb::Status s;
  // TODO(cmumford): Handle this error
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_url,
                       data_directory,
                       request_context,
                       &data_loss,
                       &data_loss_message,
                       &disk_full,
                       &s);
  if (!backing_store.get()) {
    callbacks->OnError(
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError,
                               "Internal error opening backing store for "
                               "indexedDB.webkitGetDatabaseNames."));
    return;
  }

  std::vector<base::string16> names = backing_store->GetDatabaseNames(&s);
  if (!s.ok()) {
    DLOG(ERROR) << "Internal error getting database names";
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error opening backing store for "
                                 "indexedDB.webkitGetDatabaseNames.");
    callbacks->OnError(error);
    backing_store = NULL;
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin_url, error);
    return;
  }
  callbacks->OnSuccess(names);
  backing_store = NULL;
  ReleaseBackingStore(origin_url, false /* immediate */);
}

void IndexedDBFactoryImpl::DeleteDatabase(
    const base::string16& name,
    net::URLRequestContext* request_context,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    const GURL& origin_url,
    const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactoryImpl::DeleteDatabase");
  IndexedDBDatabase::Identifier unique_identifier(origin_url, name);
  IndexedDBDatabaseMap::iterator it = database_map_.find(unique_identifier);
  if (it != database_map_.end()) {
    // If there are any connections to the database, directly delete the
    // database.
    it->second->DeleteDatabase(callbacks);
    return;
  }

  // TODO(dgrogan): Plumb data_loss back to script eventually?
  blink::WebIDBDataLoss data_loss;
  std::string data_loss_message;
  bool disk_full = false;
  leveldb::Status s;
  scoped_refptr<IndexedDBBackingStore> backing_store =
      OpenBackingStore(origin_url,
                       data_directory,
                       request_context,
                       &data_loss,
                       &data_loss_message,
                       &disk_full,
                       &s);
  if (!backing_store.get()) {
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 ASCIIToUTF16(
                                     "Internal error opening backing store "
                                     "for indexedDB.deleteDatabase."));
    callbacks->OnError(error);
    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(origin_url, error);
    }
    return;
  }

  std::vector<base::string16> names = backing_store->GetDatabaseNames(&s);
  if (!s.ok()) {
    DLOG(ERROR) << "Internal error getting database names";
    IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                 "Internal error opening backing store for "
                                 "indexedDB.deleteDatabase.");
    callbacks->OnError(error);
    backing_store = NULL;
    if (s.IsCorruption())
      HandleBackingStoreCorruption(origin_url, error);
    return;
  }
  if (!ContainsValue(names, name)) {
    const int64 version = 0;
    callbacks->OnSuccess(version);
    backing_store = NULL;
    ReleaseBackingStore(origin_url, false /* immediate */);
    return;
  }

  scoped_refptr<IndexedDBDatabase> database = IndexedDBDatabase::Create(
      name, backing_store.get(), this, unique_identifier, &s);
  if (!database.get()) {
    IndexedDBDatabaseError error(
        blink::WebIDBDatabaseExceptionUnknownError,
        ASCIIToUTF16(
            "Internal error creating database backend for "
            "indexedDB.deleteDatabase."));
    callbacks->OnError(error);
    if (s.IsCorruption()) {
      backing_store = NULL;
      HandleBackingStoreCorruption(origin_url, error);
    }
    return;
  }

  database_map_[unique_identifier] = database.get();
  origin_dbs_.insert(std::make_pair(origin_url, database.get()));
  database->DeleteDatabase(callbacks);
  RemoveDatabaseFromMaps(unique_identifier);
  database = NULL;
  backing_store = NULL;
  ReleaseBackingStore(origin_url, false /* immediate */);
}

void IndexedDBFactoryImpl::DatabaseDeleted(
    const IndexedDBDatabase::Identifier& identifier) {
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->DatabaseDeleted(identifier.first);
}

void IndexedDBFactoryImpl::HandleBackingStoreFailure(const GURL& origin_url) {
  // NULL after ContextDestroyed() called, and in some unit tests.
  if (!context_)
    return;
  context_->ForceClose(origin_url,
                       IndexedDBContextImpl::FORCE_CLOSE_BACKING_STORE_FAILURE);
}

void IndexedDBFactoryImpl::HandleBackingStoreCorruption(
    const GURL& origin_url,
    const IndexedDBDatabaseError& error) {
  // Make a copy of origin_url as this is likely a reference to a member of a
  // backing store which this function will be deleting.
  GURL saved_origin_url(origin_url);
  DCHECK(context_);
  base::FilePath path_base = context_->data_path();
  IndexedDBBackingStore::RecordCorruptionInfo(
      path_base, saved_origin_url, base::UTF16ToUTF8(error.message()));
  HandleBackingStoreFailure(saved_origin_url);
  // Note: DestroyBackingStore only deletes LevelDB files, leaving all others,
  //       so our corruption info file will remain.
  leveldb::Status s =
      IndexedDBBackingStore::DestroyBackingStore(path_base, saved_origin_url);
  if (!s.ok())
    DLOG(ERROR) << "Unable to delete backing store: " << s.ToString();
}

bool IndexedDBFactoryImpl::IsDatabaseOpen(const GURL& origin_url,
                                          const base::string16& name) const {
  return !!database_map_.count(IndexedDBDatabase::Identifier(origin_url, name));
}

bool IndexedDBFactoryImpl::IsBackingStoreOpen(const GURL& origin_url) const {
  return backing_store_map_.find(origin_url) != backing_store_map_.end();
}

bool IndexedDBFactoryImpl::IsBackingStorePendingClose(
    const GURL& origin_url) const {
  IndexedDBBackingStoreMap::const_iterator it =
      backing_store_map_.find(origin_url);
  if (it == backing_store_map_.end())
    return false;
  return it->second->close_timer()->IsRunning();
}

scoped_refptr<IndexedDBBackingStore>
IndexedDBFactoryImpl::OpenBackingStoreHelper(
    const GURL& origin_url,
    const base::FilePath& data_directory,
    net::URLRequestContext* request_context,
    blink::WebIDBDataLoss* data_loss,
    std::string* data_loss_message,
    bool* disk_full,
    bool first_time,
    leveldb::Status* status) {
  return IndexedDBBackingStore::Open(this,
                                     origin_url,
                                     data_directory,
                                     request_context,
                                     data_loss,
                                     data_loss_message,
                                     disk_full,
                                     context_->TaskRunner(),
                                     first_time,
                                     status);
}

scoped_refptr<IndexedDBBackingStore> IndexedDBFactoryImpl::OpenBackingStore(
    const GURL& origin_url,
    const base::FilePath& data_directory,
    net::URLRequestContext* request_context,
    blink::WebIDBDataLoss* data_loss,
    std::string* data_loss_message,
    bool* disk_full,
    leveldb::Status* status) {
  const bool open_in_memory = data_directory.empty();

  IndexedDBBackingStoreMap::iterator it2 = backing_store_map_.find(origin_url);
  if (it2 != backing_store_map_.end()) {
    it2->second->close_timer()->Stop();
    return it2->second;
  }

  scoped_refptr<IndexedDBBackingStore> backing_store;
  bool first_time = false;
  if (open_in_memory) {
    backing_store = IndexedDBBackingStore::OpenInMemory(
        origin_url, context_->TaskRunner(), status);
  } else {
    first_time = !backends_opened_since_boot_.count(origin_url);

    backing_store = OpenBackingStoreHelper(origin_url,
                                           data_directory,
                                           request_context,
                                           data_loss,
                                           data_loss_message,
                                           disk_full,
                                           first_time,
                                           status);
  }

  if (backing_store.get()) {
    if (first_time)
      backends_opened_since_boot_.insert(origin_url);
    backing_store_map_[origin_url] = backing_store;
    // If an in-memory database, bind lifetime to this factory instance.
    if (open_in_memory)
      session_only_backing_stores_.insert(backing_store);

    // All backing stores associated with this factory should be of the same
    // type.
    DCHECK_NE(session_only_backing_stores_.empty(), open_in_memory);

    return backing_store;
  }

  return 0;
}

void IndexedDBFactoryImpl::Open(const base::string16& name,
                                const IndexedDBPendingConnection& connection,
                                net::URLRequestContext* request_context,
                                const GURL& origin_url,
                                const base::FilePath& data_directory) {
  IDB_TRACE("IndexedDBFactoryImpl::Open");
  scoped_refptr<IndexedDBDatabase> database;
  IndexedDBDatabase::Identifier unique_identifier(origin_url, name);
  IndexedDBDatabaseMap::iterator it = database_map_.find(unique_identifier);
  blink::WebIDBDataLoss data_loss =
      blink::WebIDBDataLossNone;
  std::string data_loss_message;
  bool disk_full = false;
  bool was_open = (it != database_map_.end());
  if (!was_open) {
    leveldb::Status s;
    scoped_refptr<IndexedDBBackingStore> backing_store =
        OpenBackingStore(origin_url,
                         data_directory,
                         request_context,
                         &data_loss,
                         &data_loss_message,
                         &disk_full,
                         &s);
    if (!backing_store.get()) {
      if (disk_full) {
        connection.callbacks->OnError(
            IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionQuotaError,
                                   ASCIIToUTF16(
                                       "Encountered full disk while opening "
                                       "backing store for indexedDB.open.")));
        return;
      }
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   ASCIIToUTF16(
                                       "Internal error opening backing store"
                                       " for indexedDB.open."));
      connection.callbacks->OnError(error);
      if (s.IsCorruption()) {
        HandleBackingStoreCorruption(origin_url, error);
      }
      return;
    }

    database = IndexedDBDatabase::Create(
        name, backing_store.get(), this, unique_identifier, &s);
    if (!database.get()) {
      DLOG(ERROR) << "Unable to create the database";
      IndexedDBDatabaseError error(blink::WebIDBDatabaseExceptionUnknownError,
                                   ASCIIToUTF16(
                                       "Internal error creating "
                                       "database backend for "
                                       "indexedDB.open."));
      connection.callbacks->OnError(error);
      if (s.IsCorruption()) {
        backing_store = NULL;  // Closes the LevelDB so that it can be deleted
        HandleBackingStoreCorruption(origin_url, error);
      }
      return;
    }
  } else {
    database = it->second;
  }

  if (data_loss != blink::WebIDBDataLossNone)
    connection.callbacks->OnDataLoss(data_loss, data_loss_message);

  database->OpenConnection(connection);

  if (!was_open && database->ConnectionCount() > 0) {
    database_map_[unique_identifier] = database.get();
    origin_dbs_.insert(std::make_pair(origin_url, database.get()));
  }
}

std::pair<IndexedDBFactoryImpl::OriginDBMapIterator,
          IndexedDBFactoryImpl::OriginDBMapIterator>
IndexedDBFactoryImpl::GetOpenDatabasesForOrigin(const GURL& origin_url) const {
  return origin_dbs_.equal_range(origin_url);
}

size_t IndexedDBFactoryImpl::GetConnectionCount(const GURL& origin_url) const {
  size_t count(0);

  OriginDBs range = GetOpenDatabasesForOrigin(origin_url);
  for (OriginDBMapIterator it = range.first; it != range.second; ++it)
    count += it->second->ConnectionCount();

  return count;
}

}  // namespace content
