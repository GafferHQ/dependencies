// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_VIEW_BLOB_INTERNALS_JOB_H_
#define STORAGE_BROWSER_BLOB_VIEW_BLOB_INTERNALS_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_simple_job.h"
#include "storage/browser/storage_browser_export.h"

namespace net {
class URLRequest;
}  // namespace net

namespace storage {

class InternalBlobData;
class BlobStorageContext;

// A job subclass that implements a protocol to inspect the internal
// state of blob registry.
class STORAGE_EXPORT ViewBlobInternalsJob
    : public net::URLRequestSimpleJob {
 public:
  ViewBlobInternalsJob(net::URLRequest* request,
                       net::NetworkDelegate* network_delegate,
                       BlobStorageContext* blob_storage_context);

  void Start() override;
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              const net::CompletionCallback& callback) const override;
  bool IsRedirectResponse(GURL* location, int* http_status_code) override;
  void Kill() override;

 private:
  ~ViewBlobInternalsJob() override;

  void GenerateHTML(std::string* out) const;
  static void GenerateHTMLForBlobData(const InternalBlobData& blob_data,
                                      int refcount,
                                      std::string* out);

  BlobStorageContext* blob_storage_context_;
  base::WeakPtrFactory<ViewBlobInternalsJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewBlobInternalsJob);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_VIEW_BLOB_INTERNALS_JOB_H_
