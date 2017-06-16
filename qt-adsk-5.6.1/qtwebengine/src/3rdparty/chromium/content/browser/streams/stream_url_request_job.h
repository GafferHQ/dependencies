// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STREAMS_STREAM_URL_REQUEST_JOB_H_
#define CONTENT_BROWSER_STREAMS_STREAM_URL_REQUEST_JOB_H_

#include "net/http/http_status_code.h"
#include "net/url_request/url_request_job.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/common/content_export.h"

namespace content {

class Stream;

// A request job that handles reading stream URLs.
class CONTENT_EXPORT StreamURLRequestJob
    : public net::URLRequestJob,
      public StreamReadObserver {
 public:
  StreamURLRequestJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      scoped_refptr<Stream> stream);

  // StreamObserver methods.
  void OnDataAvailable(Stream* stream) override;

  // net::URLRequestJob methods.
  void Start() override;
  void Kill() override;
  bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read) override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  int GetResponseCode() const override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;

 protected:
  ~StreamURLRequestJob() override;

 private:
  void DidStart();
  void NotifyFailure(int);
  void HeadersCompleted(net::HttpStatusCode status_code);
  void ClearStream();

  scoped_refptr<content::Stream> stream_;
  bool headers_set_;
  scoped_refptr<net::IOBuffer> pending_buffer_;
  int pending_buffer_size_;
  scoped_ptr<net::HttpResponseInfo> response_info_;

  int total_bytes_read_;
  int max_range_;
  bool request_failed_;

  base::WeakPtrFactory<StreamURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamURLRequestJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STREAMS_STREAM_URL_REQUEST_JOB_H_
