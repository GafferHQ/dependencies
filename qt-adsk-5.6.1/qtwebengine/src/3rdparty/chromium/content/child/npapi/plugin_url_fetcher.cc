// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_url_fetcher.h"

#include "base/memory/scoped_ptr.h"
#include "content/child/child_thread_impl.h"
#include "content/child/multipart_response_delegate.h"
#include "content/child/npapi/plugin_host.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_stream_url.h"
#include "content/child/npapi/webplugin.h"
#include "content/child/npapi/webplugin_resource_client.h"
#include "content/child/plugin_messages.h"
#include "content/child/request_extra_data.h"
#include "content/child/request_info.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/web_url_loader_impl.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_response_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"

namespace content {
namespace {

// This class handles individual multipart responses. It is instantiated when
// we receive HTTP status code 206 in the HTTP response. This indicates
// that the response could have multiple parts each separated by a boundary
// specified in the response header.
// TODO(jam): this is similar to MultiPartResponseClient in webplugin_impl.cc,
// we should remove that other class once we switch to loading from the plugin
// process by default.
class MultiPartResponseClient : public blink::WebURLLoaderClient {
 public:
  explicit MultiPartResponseClient(PluginStreamUrl* plugin_stream)
      : byte_range_lower_bound_(0), plugin_stream_(plugin_stream) {}

  // blink::WebURLLoaderClient implementation:
  void didReceiveResponse(blink::WebURLLoader* loader,
                          const blink::WebURLResponse& response) override {
    int64 byte_range_upper_bound, instance_size;
    if (!MultipartResponseDelegate::ReadContentRanges(response,
                                                      &byte_range_lower_bound_,
                                                      &byte_range_upper_bound,
                                                      &instance_size)) {
      NOTREACHED();
    }
  }
  void didReceiveData(blink::WebURLLoader* loader,
                      const char* data,
                      int data_length,
                      int encoded_data_length) override {
    // TODO(ananta)
    // We should defer further loads on multipart resources on the same lines
    // as regular resources requested by plugins to prevent reentrancy.
    int64 data_offset = byte_range_lower_bound_;
    byte_range_lower_bound_ += data_length;
    plugin_stream_->DidReceiveData(data, data_length, data_offset);
    // DANGER: this instance may be deleted at this point.
  }

 private:
  // The lower bound of the byte range.
  int64 byte_range_lower_bound_;
  // The handler for the data.
  PluginStreamUrl* plugin_stream_;
};

}  // namespace

PluginURLFetcher::PluginURLFetcher(PluginStreamUrl* plugin_stream,
                                   const GURL& url,
                                   const GURL& first_party_for_cookies,
                                   const std::string& method,
                                   const char* buf,
                                   unsigned int len,
                                   const Referrer& referrer,
                                   const std::string& range,
                                   bool notify_redirects,
                                   bool is_plugin_src_load,
                                   int origin_pid,
                                   int render_frame_id,
                                   int render_view_id,
                                   unsigned long resource_id,
                                   bool copy_stream_data)
    : plugin_stream_(plugin_stream),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies),
      referrer_(referrer),
      notify_redirects_(notify_redirects),
      is_plugin_src_load_(is_plugin_src_load),
      origin_pid_(origin_pid),
      render_frame_id_(render_frame_id),
      render_view_id_(render_view_id),
      resource_id_(resource_id),
      copy_stream_data_(copy_stream_data),
      data_offset_(0),
      pending_failure_notification_(false),
      request_id_(-1),
      weak_factory_(this) {
  RequestInfo request_info;
  request_info.method = method;
  request_info.url = url;
  request_info.first_party_for_cookies = first_party_for_cookies;
  request_info.referrer = referrer;
  request_info.load_flags = net::LOAD_NORMAL;
  request_info.requestor_pid = origin_pid;
  request_info.request_type = RESOURCE_TYPE_OBJECT;
  request_info.routing_id = render_view_id;
  // ServiceWorker is disabled for NPAPI.
  request_info.skip_service_worker = true;

  RequestExtraData extra_data;
  extra_data.set_render_frame_id(render_frame_id);
  extra_data.set_is_main_frame(false);
  request_info.extra_data = &extra_data;

  std::vector<char> body;
  if (method == "POST") {
    bool content_type_found = false;
    std::vector<std::string> names;
    std::vector<std::string> values;
    PluginHost::SetPostData(buf, len, &names, &values, &body);
    for (size_t i = 0; i < names.size(); ++i) {
      if (!request_info.headers.empty())
        request_info.headers += "\r\n";
      request_info.headers += names[i] + ": " + values[i];
      if (base::LowerCaseEqualsASCII(names[i], "content-type"))
        content_type_found = true;
    }

    if (!content_type_found) {
      if (!request_info.headers.empty())
        request_info.headers += "\r\n";
      request_info.headers += "Content-Type: application/x-www-form-urlencoded";
    }
  } else {
    if (!range.empty())
      request_info.headers = std::string("Range: ") + range;
  }

  scoped_refptr<ResourceRequestBody> request_body = new ResourceRequestBody;
  if (!body.empty())
    request_body->AppendBytes(&body[0], body.size());

  request_id_ = ChildThreadImpl::current()->resource_dispatcher()->StartAsync(
      request_info, request_body.get(), this);

  // TODO(jam): range requests
}

PluginURLFetcher::~PluginURLFetcher() {
  if (request_id_ >= 0) {
    ChildThreadImpl::current()->resource_dispatcher()->RemovePendingRequest(
        request_id_);
  }
}

void PluginURLFetcher::Cancel() {
  ChildThreadImpl::current()->resource_dispatcher()->Cancel(request_id_);

  // Due to races and nested event loops, PluginURLFetcher may still receive
  // events from the bridge before being destroyed. Do not forward additional
  // events back to the plugin, via either |plugin_stream_| or
  // |multipart_delegate_| which has its own pointer via
  // MultiPartResponseClient.
  if (multipart_delegate_)
    multipart_delegate_->Cancel();
  plugin_stream_ = NULL;
}

void PluginURLFetcher::URLRedirectResponse(bool allow) {
  if (!plugin_stream_)
    return;

  if (allow) {
    ChildThreadImpl::current()->resource_dispatcher()->SetDefersLoading(
        request_id_, false);
  } else {
    ChildThreadImpl::current()->resource_dispatcher()->Cancel(request_id_);
    plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
  }
}

void PluginURLFetcher::OnUploadProgress(uint64 position, uint64 size) {
}

bool PluginURLFetcher::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseInfo& info) {
  if (!plugin_stream_)
    return false;

  // TODO(jam): THIS LOGIC IS COPIED FROM WebPluginImpl::willSendRequest until
  // kDirectNPAPIRequests is the default and we can remove the old path there.

  // Currently this check is just to catch an https -> http redirect when
  // loading the main plugin src URL. Longer term, we could investigate
  // firing mixed diplay or scripting issues for subresource loads
  // initiated by plugins.
  if (is_plugin_src_load_ &&
      !plugin_stream_->instance()->webplugin()->CheckIfRunInsecureContent(
          redirect_info.new_url)) {
    plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
    return false;
  }

  GURL old_url = url_;
  url_ = redirect_info.new_url;
  first_party_for_cookies_ = redirect_info.new_first_party_for_cookies;

  // If the plugin does not participate in url redirect notifications then just
  // block cross origin 307 POST redirects.
  if (!notify_redirects_) {
    if (redirect_info.status_code == 307 &&
        redirect_info.new_method == "POST" &&
        old_url.GetOrigin() != url_.GetOrigin()) {
      plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
      return false;
    }
  } else {
    // Pause the request while we ask the plugin what to do about the redirect.
    ChildThreadImpl::current()->resource_dispatcher()->SetDefersLoading(
        request_id_, true);
    plugin_stream_->WillSendRequest(url_, redirect_info.status_code);
  }

  return true;
}

void PluginURLFetcher::OnReceivedResponse(const ResourceResponseInfo& info) {
  if (!plugin_stream_)
    return;

  // TODO(jam): THIS LOGIC IS COPIED FROM WebPluginImpl::didReceiveResponse
  // GetAllHeaders, and GetResponseInfo until kDirectNPAPIRequests is the
  // default and we can remove the old path there.

  bool request_is_seekable = true;
  DCHECK(!multipart_delegate_.get());
  if (plugin_stream_->seekable()) {
    int response_code = info.headers->response_code();
    if (response_code == 206) {
      blink::WebURLResponse response;
      response.initialize();
      WebURLLoaderImpl::PopulateURLResponse(url_, info, &response);

      std::string multipart_boundary;
      if (MultipartResponseDelegate::ReadMultipartBoundary(
              response, &multipart_boundary)) {
        plugin_stream_->instance()->webplugin()->DidStartLoading();

        MultiPartResponseClient* multi_part_response_client =
            new MultiPartResponseClient(plugin_stream_);

        multipart_delegate_.reset(new MultipartResponseDelegate(
            multi_part_response_client, NULL, response, multipart_boundary));

        // Multiple ranges requested, data will be delivered by
        // MultipartResponseDelegate.
        data_offset_ = 0;
        return;
      }

      int64 upper_bound = 0, instance_size = 0;
      // Single range requested - go through original processing for
      // non-multipart requests, but update data offset.
      MultipartResponseDelegate::ReadContentRanges(
          response, &data_offset_, &upper_bound, &instance_size);
    } else if (response_code == 200) {
      // TODO: should we handle this case? We used to but it's not clear that we
      // still need to. This was bug 5403, fixed in r7139.
    }
  }

  // If the length comes in as -1, then it indicates that it was not
  // read off the HTTP headers. We replicate Safari webkit behavior here,
  // which is to set it to 0.
  int expected_length = std::max(static_cast<int>(info.content_length), 0);

  base::Time temp;
  uint32 last_modified = 0;
  std::string headers;
  if (info.headers.get()) {  // NULL for data: urls.
    if (info.headers->GetLastModifiedValue(&temp))
      last_modified = static_cast<uint32>(temp.ToDoubleT());

     // TODO(darin): Shouldn't we also report HTTP version numbers?
    int response_code = info.headers->response_code();
    headers = base::StringPrintf("HTTP %d ", response_code);
    headers += info.headers->GetStatusText();
    headers += "\n";

    void* iter = NULL;
    std::string name, value;
    while (info.headers->EnumerateHeaderLines(&iter, &name, &value)) {
      // TODO(darin): Should we really exclude headers with an empty value?
      if (!name.empty() && !value.empty())
        headers += name + ": " + value + "\n";
    }

    // Bug http://b/issue?id=925559. The flash plugin would not handle the HTTP
    // error codes in the stream header and as a result, was unaware of the fate
    // of the HTTP requests issued via NPN_GetURLNotify. Webkit and FF destroy
    // the stream and invoke the NPP_DestroyStream function on the plugin if the
    // HTTPrequest fails.
    if ((url_.SchemeIs(url::kHttpScheme) || url_.SchemeIs(url::kHttpsScheme)) &&
        (response_code < 100 || response_code >= 400)) {
      pending_failure_notification_ = true;
    }
  }

  plugin_stream_->DidReceiveResponse(info.mime_type,
                                     headers,
                                     expected_length,
                                     last_modified,
                                     request_is_seekable);
}

void PluginURLFetcher::OnDownloadedData(int len,
                                        int encoded_data_length) {
}

void PluginURLFetcher::OnReceivedData(scoped_ptr<ReceivedData> data) {
  const char* payload = data->payload();
  int data_length = data->length();
  int encoded_data_length = data->encoded_length();
  if (!plugin_stream_)
    return;

  if (multipart_delegate_) {
    multipart_delegate_->OnReceivedData(payload, data_length,
                                        encoded_data_length);
  } else {
    int64 offset = data_offset_;
    data_offset_ += data_length;

    if (copy_stream_data_) {
      // QuickTime writes to this memory, and since we got it from
      // ResourceDispatcher it's not mapped for write access in this process.
      // http://crbug.com/308466.
      scoped_ptr<char[]> data_copy(new char[data_length]);
      memcpy(data_copy.get(), payload, data_length);
      plugin_stream_->DidReceiveData(data_copy.get(), data_length, offset);
    } else {
      plugin_stream_->DidReceiveData(payload, data_length, offset);
    }
    // DANGER: this instance may be deleted at this point.
  }
}

void PluginURLFetcher::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const std::string& security_info,
    const base::TimeTicks& completion_time,
    int64 total_transfer_size) {
  if (!plugin_stream_)
    return;

  if (multipart_delegate_) {
    multipart_delegate_->OnCompletedRequest();
    multipart_delegate_.reset();
  }

  if (error_code == net::OK) {
    plugin_stream_->DidFinishLoading(resource_id_);
  } else {
    plugin_stream_->DidFail(resource_id_);
  }
}

void PluginURLFetcher::OnReceivedCompletedResponse(
    const content::ResourceResponseInfo& info,
    scoped_ptr<ReceivedData> data,
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const std::string& security_info,
    const base::TimeTicks& completion_time,
    int64 total_transfer_size) {
  // |this| can be deleted on each callback. |weak_this| is placed here to
  // detect the deletion.
  base::WeakPtr<PluginURLFetcher> weak_this = weak_factory_.GetWeakPtr();
  OnReceivedResponse(info);

  if (!weak_this)
    return;
  if (data)
    OnReceivedData(data.Pass());

  if (!weak_this)
    return;
  OnCompletedRequest(error_code, was_ignored_by_handler, stale_copy_in_cache,
                     security_info, completion_time, total_transfer_size);
}
}  // namespace content
