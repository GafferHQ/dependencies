// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_
#define STORAGE_BROWSER_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_stream_writer.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/task_runner_bound_observer_list.h"
#include "storage/browser/storage_browser_export.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

namespace storage {

class FileSystemContext;
class FileSystemQuotaUtil;
class FileStreamWriter;

class STORAGE_EXPORT_PRIVATE SandboxFileStreamWriter
    : public NON_EXPORTED_BASE(FileStreamWriter) {
 public:
  SandboxFileStreamWriter(FileSystemContext* file_system_context,
                          const FileSystemURL& url,
                          int64 initial_offset,
                          const UpdateObserverList& observers);
  ~SandboxFileStreamWriter() override;

  // FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            const net::CompletionCallback& callback) override;
  int Cancel(const net::CompletionCallback& callback) override;
  int Flush(const net::CompletionCallback& callback) override;

  // Used only by tests.
  void set_default_quota(int64 quota) {
    default_quota_ = quota;
  }

 private:
  // Performs quota calculation and calls local_file_writer_->Write().
  int WriteInternal(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback);

  // Callbacks that are chained for the first write.  This eventually calls
  // WriteInternal.
  void DidCreateSnapshotFile(
      const net::CompletionCallback& callback,
      base::File::Error file_error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<storage::ShareableFileReference>& file_ref);
  void DidGetUsageAndQuota(const net::CompletionCallback& callback,
                           storage::QuotaStatusCode status,
                           int64 usage,
                           int64 quota);
  void DidInitializeForWrite(net::IOBuffer* buf, int buf_len,
                             const net::CompletionCallback& callback,
                             int init_status);

  void DidWrite(const net::CompletionCallback& callback, int write_response);

  // Stops the in-flight operation, calls |cancel_callback_| and returns true
  // if there's a pending cancel request.
  bool CancelIfRequested();

  scoped_refptr<FileSystemContext> file_system_context_;
  FileSystemURL url_;
  int64 initial_offset_;
  scoped_ptr<FileStreamWriter> local_file_writer_;
  net::CompletionCallback cancel_callback_;

  UpdateObserverList observers_;

  base::FilePath file_path_;
  int64 file_size_;
  int64 total_bytes_written_;
  int64 allowed_bytes_to_write_;
  bool has_pending_operation_;

  int64 default_quota_;

  base::WeakPtrFactory<SandboxFileStreamWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SandboxFileStreamWriter);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_SANDBOX_FILE_STREAM_WRITER_H_
