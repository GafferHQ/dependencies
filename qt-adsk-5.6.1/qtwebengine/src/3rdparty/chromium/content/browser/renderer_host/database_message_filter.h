// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_

#include "base/containers/hash_tables.h"
#include "base/strings/string16.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_platform_file.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/common/database/database_connections.h"
#include "storage/common/quota/quota_types.h"

namespace content {

class DatabaseMessageFilter : public BrowserMessageFilter,
                              public storage::DatabaseTracker::Observer {
 public:
  explicit DatabaseMessageFilter(storage::DatabaseTracker* db_tracker);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  storage::DatabaseTracker* database_tracker() const {
    return db_tracker_.get();
  }

 private:
  ~DatabaseMessageFilter() override;

  class PromptDelegate;

  void AddObserver();
  void RemoveObserver();

  // VFS message handlers (file thread)
  void OnDatabaseOpenFile(const base::string16& vfs_file_name,
                          int desired_flags,
                          IPC::PlatformFileForTransit* handle);
  void OnDatabaseDeleteFile(const base::string16& vfs_file_name,
                            const bool& sync_dir,
                            IPC::Message* reply_msg);
  void OnDatabaseGetFileAttributes(const base::string16& vfs_file_name,
                                   int32* attributes);
  void OnDatabaseGetFileSize(const base::string16& vfs_file_name,
                             int64* size);
  void OnDatabaseSetFileSize(const base::string16& vfs_file_name,
                             int64 size,
                             bool* success);

  // Quota message handler (io thread)
  void OnDatabaseGetSpaceAvailable(const std::string& origin_identifier,
                                   IPC::Message* reply_msg);
  void OnDatabaseGetUsageAndQuota(IPC::Message* reply_msg,
                                  storage::QuotaStatusCode status,
                                  int64 usage,
                                  int64 quota);

  // Database tracker message handlers (file thread)
  void OnDatabaseOpened(const std::string& origin_identifier,
                        const base::string16& database_name,
                        const base::string16& description,
                        int64 estimated_size);
  void OnDatabaseModified(const std::string& origin_identifier,
                          const base::string16& database_name);
  void OnDatabaseClosed(const std::string& origin_identifier,
                        const base::string16& database_name);
  void OnHandleSqliteError(const std::string& origin_identifier,
                           const base::string16& database_name,
                           int error);

  // DatabaseTracker::Observer callbacks (file thread)
  void OnDatabaseSizeChanged(const std::string& origin_identifier,
                             const base::string16& database_name,
                             int64 database_size) override;
  void OnDatabaseScheduledForDeletion(
      const std::string& origin_identifier,
      const base::string16& database_name) override;

  void DatabaseDeleteFile(const base::string16& vfs_file_name,
                          bool sync_dir,
                          IPC::Message* reply_msg,
                          int reschedule_count);

  // The database tracker for the current browser context.
  scoped_refptr<storage::DatabaseTracker> db_tracker_;

  // True if and only if this instance was added as an observer
  // to DatabaseTracker.
  bool observer_added_;

  // Keeps track of all DB connections opened by this renderer
  storage::DatabaseConnections database_connections_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DATABASE_MESSAGE_FILTER_H_
