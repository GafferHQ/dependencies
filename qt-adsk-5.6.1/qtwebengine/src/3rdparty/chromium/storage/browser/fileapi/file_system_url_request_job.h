// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
#define STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_

#include <string>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/storage_browser_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace storage {
class FileStreamReader;
}

namespace storage {
class FileSystemContext;

// A request job that handles reading filesystem: URLs
class STORAGE_EXPORT_PRIVATE FileSystemURLRequestJob
    : public net::URLRequestJob {
 public:
  FileSystemURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const std::string& storage_domain,
      FileSystemContext* file_system_context);

  // URLRequestJob methods:
  void Start() override;
  void Kill() override;
  bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read) override;
  bool IsRedirectResponse(GURL* location, int* http_status_code) override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  int GetResponseCode() const override;

  // FilterContext methods (via URLRequestJob):
  bool GetMimeType(std::string* mime_type) const override;

 private:
  class CallbackDispatcher;

  ~FileSystemURLRequestJob() override;

  void StartAsync();
  void DidAttemptAutoMount(base::File::Error result);
  void DidGetMetadata(base::File::Error error_code,
                      const base::File::Info& file_info);
  void DidRead(int result);
  void NotifyFailed(int rv);

  const std::string storage_domain_;
  FileSystemContext* file_system_context_;
  scoped_ptr<storage::FileStreamReader> reader_;
  FileSystemURL url_;
  bool is_directory_;
  scoped_ptr<net::HttpResponseInfo> response_info_;
  int64 remaining_bytes_;
  net::HttpByteRange byte_range_;
  base::WeakPtrFactory<FileSystemURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLRequestJob);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_H_
