// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_FILEAPI_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_options.h"
#include "storage/browser/fileapi/file_system_quota_util.h"
#include "storage/browser/fileapi/task_runner_bound_observer_list.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class PluginPrivateFileSystemBackendTest;
}

namespace storage {
class SpecialStoragePolicy;
}

namespace storage {

class ObfuscatedFileUtil;
class WatcherManager;

class STORAGE_EXPORT PluginPrivateFileSystemBackend
    : public FileSystemBackend,
      public FileSystemQuotaUtil {
 public:
  class FileSystemIDToPluginMap;
  typedef base::Callback<void(base::File::Error result)> StatusCallback;

  PluginPrivateFileSystemBackend(
      base::SequencedTaskRunner* file_task_runner,
      const base::FilePath& profile_path,
      storage::SpecialStoragePolicy* special_storage_policy,
      const FileSystemOptions& file_system_options);
  ~PluginPrivateFileSystemBackend() override;

  // This must be used to open 'private' filesystem instead of regular
  // OpenFileSystem.
  // |plugin_id| must be an identifier string for per-plugin
  // isolation, e.g. name, MIME type etc.
  // NOTE: |plugin_id| must be sanitized ASCII string that doesn't
  // include *any* dangerous character like '/'.
  void OpenPrivateFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      const std::string& filesystem_id,
      const std::string& plugin_id,
      OpenFileSystemMode mode,
      const StatusCallback& callback);

  // FileSystemBackend overrides.
  bool CanHandleType(FileSystemType type) const override;
  void Initialize(FileSystemContext* context) override;
  void ResolveURL(const FileSystemURL& url,
                  OpenFileSystemMode mode,
                  const OpenFileSystemCallback& callback) override;
  AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) override;
  WatcherManager* GetWatcherManager(FileSystemType type) override;
  CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) override;
  FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const override;
  bool SupportsStreaming(const FileSystemURL& url) const override;
  bool HasInplaceCopyImplementation(
      storage::FileSystemType type) const override;
  scoped_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      int64 max_bytes_to_read,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const override;
  scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const override;
  FileSystemQuotaUtil* GetQuotaUtil() override;
  const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const override;
  const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const override;
  const AccessObserverList* GetAccessObservers(
      FileSystemType type) const override;

  // FileSystemQuotaUtil overrides.
  base::File::Error DeleteOriginDataOnFileTaskRunner(
      FileSystemContext* context,
      storage::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type) override;
  void GetOriginsForTypeOnFileTaskRunner(FileSystemType type,
                                         std::set<GURL>* origins) override;
  void GetOriginsForHostOnFileTaskRunner(FileSystemType type,
                                         const std::string& host,
                                         std::set<GURL>* origins) override;
  int64 GetOriginUsageOnFileTaskRunner(FileSystemContext* context,
                                       const GURL& origin_url,
                                       FileSystemType type) override;
  scoped_refptr<QuotaReservation> CreateQuotaReservationOnFileTaskRunner(
      const GURL& origin_url,
      FileSystemType type) override;

 private:
  friend class content::PluginPrivateFileSystemBackendTest;

  ObfuscatedFileUtil* obfuscated_file_util();
  const base::FilePath& base_path() const { return base_path_; }

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  const FileSystemOptions file_system_options_;
  const base::FilePath base_path_;
  scoped_ptr<AsyncFileUtil> file_util_;
  FileSystemIDToPluginMap* plugin_map_;  // Owned by file_util_.
  base::WeakPtrFactory<PluginPrivateFileSystemBackend> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginPrivateFileSystemBackend);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_
