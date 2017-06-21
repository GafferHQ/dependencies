// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceBuffer;
class ResourceContext;
class ResourceDispatcherHostImpl;
class ResourceMessageFilter;
class SharedIOBuffer;

// Used to complete an asynchronous resource request in response to resource
// load events from the resource dispatcher host.
class AsyncResourceHandler : public ResourceHandler,
                             public ResourceMessageDelegate {
 public:
  AsyncResourceHandler(net::URLRequest* request,
                       ResourceDispatcherHostImpl* rdh);
  ~AsyncResourceHandler() override;

  bool OnMessageReceived(const IPC::Message& message) override;

  // ResourceHandler implementation:
  bool OnUploadProgress(uint64 position, uint64 size) override;
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL& url, bool* defer) override;
  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;
  void OnDataDownloaded(int bytes_downloaded) override;

 private:
  // IPC message handlers:
  void OnFollowRedirect(int request_id);
  void OnDataReceivedACK(int request_id);

  bool EnsureResourceBufferIsInitialized();
  void ResumeIfDeferred();
  void OnDefer();

  scoped_refptr<ResourceBuffer> buffer_;
  ResourceDispatcherHostImpl* rdh_;

  // Number of messages we've sent to the renderer that we haven't gotten an
  // ACK for. This allows us to avoid having too many messages in flight.
  int pending_data_count_;

  int allocation_size_;

  bool did_defer_;

  bool has_checked_for_sufficient_resources_;
  bool sent_received_response_msg_;
  bool sent_first_data_msg_;

  int64_t reported_transfer_size_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_
