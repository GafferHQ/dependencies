// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_WRITE_TO_CACHE_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_WRITE_TO_CACHE_JOB_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerResponseWriter;
class ServiceWorkerVersions;

// A URLRequestJob derivative used to cache the main script
// and its imports during the initial install of a new version.
// Another separate URLRequest is started which will perform
// a network fetch. The response produced for that separate
// request is written to the service worker script cache and piped
// to the consumer of the ServiceWorkerWriteToCacheJob for delivery
// to the renderer process housing the worker.
//
// For updates, the main script is not written to disk until a change with the
// incumbent script is detected. The incumbent script is progressively compared
// with the new script as it is read from network. Once a change is detected,
// everything that matched is copied to disk, and from then on the script is
// written as it continues to be read from network. If the scripts were
// identical, the job fails so the worker can be discarded.
class CONTENT_EXPORT ServiceWorkerWriteToCacheJob
    : public net::URLRequestJob,
      public net::URLRequest::Delegate {
 public:
  ServiceWorkerWriteToCacheJob(net::URLRequest* request,
                               net::NetworkDelegate* network_delegate,
                               ResourceType resource_type,
                               base::WeakPtr<ServiceWorkerContextCore> context,
                               ServiceWorkerVersion* version,
                               int extra_load_flags,
                               int64 response_id,
                               int64 incumbent_response_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateBefore24Hours);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateAfter24Hours);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextRequestHandlerTest,
                           UpdateForceBypassCache);
  class NetDataConsumer;
  class PassThroughConsumer;
  class Comparer;
  class Copier;

  ~ServiceWorkerWriteToCacheJob() override;

  // net::URLRequestJob overrides
  void Start() override;
  void Kill() override;
  net::LoadState GetLoadState() const override;
  bool GetCharset(std::string* charset) override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  int GetResponseCode() const override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;
  bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read) override;

  const net::HttpResponseInfo* http_info() const;

  // Methods to drive the net request forward and
  // write data to the disk cache.
  void InitNetRequest(int extra_load_flags);
  void StartNetRequest();
  net::URLRequestStatus ReadNetData(net::IOBuffer* buf,
                                    int buf_size,
                                    int* bytes_read);

  void CommitHeadersAndNotifyHeadersComplete();
  void WriteHeaders(const base::Closure& callback);
  void OnWriteHeadersComplete(const base::Closure& callback, int result);
  void WriteData(net::IOBuffer* buf,
                 int amount_to_write,
                 const base::Callback<void(int result)>& callback);
  void OnWriteDataComplete(const base::Callback<void(int result)>& callback,
                           int result);

  // net::URLRequest::Delegate overrides that observe the net request.
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnBeforeNetworkStart(net::URLRequest* request, bool* defer) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  bool CheckPathRestriction(net::URLRequest* request);

  void SetPendingIO();
  void ClearPendingIO();
  void OnPassThroughComplete();
  void OnCompareComplete(int bytes_matched, bool is_equal);
  void CopyIncumbent(int bytes_to_copy);
  void OnCopyComplete(ServiceWorkerStatusCode status);
  void HandleNetData(int bytes_read);

  void AsyncNotifyDoneHelper(const net::URLRequestStatus& status,
                             const std::string& status_message);

  void NotifyFinishedCaching(net::URLRequestStatus status,
                             const std::string& status_message);

  ResourceType resource_type_;  // Differentiate main script and imports
  scoped_refptr<net::IOBuffer> io_buffer_;
  int io_buffer_bytes_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  GURL url_;
  int64 response_id_;
  int64 incumbent_response_id_;
  scoped_ptr<net::URLRequest> net_request_;
  scoped_ptr<net::HttpResponseInfo> http_info_;
  scoped_ptr<ServiceWorkerResponseWriter> writer_;
  scoped_refptr<ServiceWorkerVersion> version_;
  scoped_ptr<NetDataConsumer> consumer_;
  bool has_been_killed_;
  bool did_notify_started_;
  bool did_notify_finished_;
  base::WeakPtrFactory<ServiceWorkerWriteToCacheJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerWriteToCacheJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_WRITE_TO_CACHE_JOB_H_
