// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_request_job.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_response_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

net::NetLog::EventType RequestJobResultToNetEventType(
    ServiceWorkerMetrics::URLRequestJobResult result) {
  using n = net::NetLog;
  using m = ServiceWorkerMetrics;
  switch (result) {
    case m::REQUEST_JOB_FALLBACK_RESPONSE:
      return n::TYPE_SERVICE_WORKER_FALLBACK_RESPONSE;
    case m::REQUEST_JOB_FALLBACK_FOR_CORS:
      return n::TYPE_SERVICE_WORKER_FALLBACK_FOR_CORS;
    case m::REQUEST_JOB_HEADERS_ONLY_RESPONSE:
      return n::TYPE_SERVICE_WORKER_HEADERS_ONLY_RESPONSE;
    case m::REQUEST_JOB_STREAM_RESPONSE:
      return n::TYPE_SERVICE_WORKER_STREAM_RESPONSE;
    case m::REQUEST_JOB_BLOB_RESPONSE:
      return n::TYPE_SERVICE_WORKER_BLOB_RESPONSE;
    case m::REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO:
      return n::TYPE_SERVICE_WORKER_ERROR_RESPONSE_STATUS_ZERO;
    case m::REQUEST_JOB_ERROR_BAD_BLOB:
      return n::TYPE_SERVICE_WORKER_ERROR_BAD_BLOB;
    case m::REQUEST_JOB_ERROR_NO_PROVIDER_HOST:
      return n::TYPE_SERVICE_WORKER_ERROR_NO_PROVIDER_HOST;
    case m::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION:
      return n::TYPE_SERVICE_WORKER_ERROR_NO_ACTIVE_VERSION;
    case m::REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH:
      return n::TYPE_SERVICE_WORKER_ERROR_FETCH_EVENT_DISPATCH;
    case m::REQUEST_JOB_ERROR_BLOB_READ:
      return n::TYPE_SERVICE_WORKER_ERROR_BLOB_READ;
    case m::REQUEST_JOB_ERROR_STREAM_ABORTED:
      return n::TYPE_SERVICE_WORKER_ERROR_STREAM_ABORTED;
    case m::REQUEST_JOB_ERROR_KILLED:
      return n::TYPE_SERVICE_WORKER_ERROR_KILLED;
    case m::REQUEST_JOB_ERROR_KILLED_WITH_BLOB:
      return n::TYPE_SERVICE_WORKER_ERROR_KILLED_WITH_BLOB;
    case m::REQUEST_JOB_ERROR_KILLED_WITH_STREAM:
      return n::TYPE_SERVICE_WORKER_ERROR_KILLED_WITH_STREAM;
    // We can't log if there's no request; fallthrough.
    case m::REQUEST_JOB_ERROR_NO_REQUEST:
    // Obsolete types; fallthrough.
    case m::REQUEST_JOB_ERROR_DESTROYED:
    case m::REQUEST_JOB_ERROR_DESTROYED_WITH_BLOB:
    case m::REQUEST_JOB_ERROR_DESTROYED_WITH_STREAM:
    // Invalid type.
    case m::NUM_REQUEST_JOB_RESULT_TYPES:
      NOTREACHED() << result;
  }
  NOTREACHED() << result;
  return n::TYPE_FAILED;
}

}  // namespace

ServiceWorkerURLRequestJob::ServiceWorkerURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    const ResourceContext* resource_context,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    bool is_main_resource_load,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBody> body)
    : net::URLRequestJob(request, network_delegate),
      provider_host_(provider_host),
      response_type_(NOT_DETERMINED),
      is_started_(false),
      service_worker_response_type_(blink::WebServiceWorkerResponseTypeDefault),
      blob_storage_context_(blob_storage_context),
      resource_context_(resource_context),
      stream_pending_buffer_size_(0),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      is_main_resource_load_(is_main_resource_load),
      request_context_type_(request_context_type),
      frame_type_(frame_type),
      fall_back_required_(false),
      body_(body),
      weak_factory_(this) {
}

void ServiceWorkerURLRequestJob::FallbackToNetwork() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  response_type_ = FALLBACK_TO_NETWORK;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::ForwardToServiceWorker() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  response_type_ = FORWARD_TO_SERVICE_WORKER;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Start() {
  is_started_ = true;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  ClearStream();
  fetch_dispatcher_.reset();
  blob_request_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

net::LoadState ServiceWorkerURLRequestJob::GetLoadState() const {
  // TODO(kinuko): refine this for better debug.
  return net::URLRequestJob::GetLoadState();
}

bool ServiceWorkerURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  const base::Time request_time = info->request_time;
  *info = *http_info();
  info->request_time = request_time;
  info->response_time = response_time_;
}

void ServiceWorkerURLRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  *load_timing_info = load_timing_info_;
}

int ServiceWorkerURLRequestJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

void ServiceWorkerURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
    return;
  }

  // We don't support multiple range requests in one single URL request.
  if (ranges.size() == 1U)
    byte_range_ = ranges[0];
}

bool ServiceWorkerURLRequestJob::ReadRawData(
    net::IOBuffer* buf, int buf_size, int *bytes_read) {
  DCHECK(buf);
  DCHECK_GE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(waiting_stream_url_.is_empty());
  if (stream_.get()) {
    switch (stream_->ReadRawData(buf, buf_size, bytes_read)) {
      case Stream::STREAM_HAS_DATA:
        DCHECK_GT(*bytes_read, 0);
        return true;
      case Stream::STREAM_COMPLETE:
        DCHECK(!*bytes_read);
        RecordResult(ServiceWorkerMetrics::REQUEST_JOB_STREAM_RESPONSE);
        return true;
      case Stream::STREAM_EMPTY:
        stream_pending_buffer_ = buf;
        stream_pending_buffer_size_ = buf_size;
        SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
        return false;
      case Stream::STREAM_ABORTED:
        // Handle this as connection reset.
        RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_STREAM_ABORTED);
        NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                         net::ERR_CONNECTION_RESET));
        return false;
    }
    NOTREACHED();
    return false;
  }

  if (!blob_request_) {
    *bytes_read = 0;
    return true;
  }
  blob_request_->Read(buf, buf_size, bytes_read);
  net::URLRequestStatus status = blob_request_->status();
  SetStatus(status);
  if (status.is_io_pending())
    return false;
  if (status.is_success() && *bytes_read == 0)
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_BLOB_RESPONSE);
  return status.is_success();
}

// TODO(falken): Refactor Blob and Stream specific handling to separate classes.
// Overrides for Blob reading -------------------------------------------------

void ServiceWorkerURLRequestJob::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnAuthRequired(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnSSLCertificateError(
    net::URLRequest* request,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnBeforeNetworkStart(net::URLRequest* request,
                                                      bool* defer) {
  NOTREACHED();
}

void ServiceWorkerURLRequestJob::OnResponseStarted(net::URLRequest* request) {
  // TODO(falken): Add Content-Length, Content-Type if they were not provided in
  // the ServiceWorkerResponse.
  response_time_ = base::Time::Now();
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::OnReadCompleted(net::URLRequest* request,
                                                 int bytes_read) {
  SetStatus(request->status());
  if (!request->status().is_success()) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_BLOB_READ);
    NotifyDone(request->status());
    return;
  }

  if (bytes_read == 0) {
    // Protect because NotifyReadComplete() can destroy |this|.
    scoped_refptr<ServiceWorkerURLRequestJob> protect(this);
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_BLOB_RESPONSE);
    NotifyReadComplete(bytes_read);
    NotifyDone(request->status());
    return;
  }
  NotifyReadComplete(bytes_read);
}

// Overrides for Stream reading -----------------------------------------------

void ServiceWorkerURLRequestJob::OnDataAvailable(Stream* stream) {
  // Clear the IO_PENDING status.
  SetStatus(net::URLRequestStatus());
  // Do nothing if stream_pending_buffer_ is empty, i.e. there's no ReadRawData
  // operation waiting for IO completion.
  if (!stream_pending_buffer_.get())
    return;

  // stream_pending_buffer_ is set to the IOBuffer instance provided to
  // ReadRawData() by URLRequestJob.

  int bytes_read = 0;
  switch (stream_->ReadRawData(
      stream_pending_buffer_.get(), stream_pending_buffer_size_, &bytes_read)) {
    case Stream::STREAM_HAS_DATA:
      DCHECK_GT(bytes_read, 0);
      break;
    case Stream::STREAM_COMPLETE:
      // Calling NotifyReadComplete with 0 signals completion.
      DCHECK(!bytes_read);
      RecordResult(ServiceWorkerMetrics::REQUEST_JOB_STREAM_RESPONSE);
      break;
    case Stream::STREAM_EMPTY:
      NOTREACHED();
      break;
    case Stream::STREAM_ABORTED:
      // Handle this as connection reset.
      RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_STREAM_ABORTED);
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_CONNECTION_RESET));
      break;
  }

  // Clear the buffers before notifying the read is complete, so that it is
  // safe for the observer to read.
  stream_pending_buffer_ = nullptr;
  stream_pending_buffer_size_ = 0;
  NotifyReadComplete(bytes_read);
}

void ServiceWorkerURLRequestJob::OnStreamRegistered(Stream* stream) {
  StreamContext* stream_context =
      GetStreamContextForResourceContext(resource_context_);
  stream_context->registry()->RemoveRegisterObserver(waiting_stream_url_);
  waiting_stream_url_ = GURL();
  stream_ = stream;
  stream_->SetReadObserver(this);
  CommitResponseHeader();
}

// Misc -----------------------------------------------------------------------

const net::HttpResponseInfo* ServiceWorkerURLRequestJob::http_info() const {
  if (!http_response_info_)
    return nullptr;
  if (range_response_info_)
    return range_response_info_.get();
  return http_response_info_.get();
}

void ServiceWorkerURLRequestJob::GetExtraResponseInfo(
    ResourceResponseInfo* response_info) const {
  if (response_type_ != FORWARD_TO_SERVICE_WORKER) {
    response_info->was_fetched_via_service_worker = false;
    response_info->was_fallback_required_by_service_worker = false;
    response_info->original_url_via_service_worker = GURL();
    response_info->response_type_via_service_worker =
        blink::WebServiceWorkerResponseTypeDefault;
    return;
  }
  response_info->was_fetched_via_service_worker = true;
  response_info->was_fallback_required_by_service_worker = fall_back_required_;
  response_info->original_url_via_service_worker = response_url_;
  response_info->response_type_via_service_worker =
      service_worker_response_type_;
  response_info->service_worker_start_time = worker_start_time_;
  response_info->service_worker_ready_time = worker_ready_time_;
}


ServiceWorkerURLRequestJob::~ServiceWorkerURLRequestJob() {
  ClearStream();

  if (!ShouldRecordResult())
    return;
  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED;
  if (response_body_type_ == STREAM)
    result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED_WITH_STREAM;
  else if (response_body_type_ == BLOB)
    result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED_WITH_BLOB;
  RecordResult(result);
}

void ServiceWorkerURLRequestJob::MaybeStartRequest() {
  if (is_started_ && response_type_ != NOT_DETERMINED) {
    // Start asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ServiceWorkerURLRequestJob::StartRequest,
                              weak_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerURLRequestJob::StartRequest() {
  if (request()) {
    request()->net_log().AddEvent(
        net::NetLog::TYPE_SERVICE_WORKER_START_REQUEST);
  }

  switch (response_type_) {
    case NOT_DETERMINED:
      NOTREACHED();
      return;

    case FALLBACK_TO_NETWORK:
      // Restart the request to create a new job. Our request handler will
      // return nullptr, and the default job (which will hit network) should be
      // created.
      NotifyRestartRequired();
      return;

    case FORWARD_TO_SERVICE_WORKER:
      if (!provider_host_) {
        RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_PROVIDER_HOST);
        DeliverErrorResponse();
        return;
      }
      if (!provider_host_->active_version()) {
        RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION);
        DeliverErrorResponse();
        return;
      }

      DCHECK(!fetch_dispatcher_);
      // Send a fetch event to the ServiceWorker associated to the
      // provider_host.
      fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
          CreateFetchRequest(),
          provider_host_->active_version(),
          base::Bind(&ServiceWorkerURLRequestJob::DidPrepareFetchEvent,
                     weak_factory_.GetWeakPtr()),
          base::Bind(&ServiceWorkerURLRequestJob::DidDispatchFetchEvent,
                     weak_factory_.GetWeakPtr())));
      worker_start_time_ = base::TimeTicks::Now();
      fetch_dispatcher_->Run();
      return;
  }

  NOTREACHED();
}

scoped_ptr<ServiceWorkerFetchRequest>
ServiceWorkerURLRequestJob::CreateFetchRequest() {
  std::string blob_uuid;
  uint64 blob_size = 0;
  CreateRequestBodyBlob(&blob_uuid, &blob_size);
  scoped_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest());
  request->mode = request_mode_;
  request->request_context_type = request_context_type_;
  request->frame_type = frame_type_;
  request->url = request_->url();
  request->method = request_->method();
  const net::HttpRequestHeaders& headers = request_->extra_request_headers();
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
    if (ServiceWorkerContext::IsExcludedHeaderNameForFetchEvent(it.name()))
      continue;
    request->headers[it.name()] = it.value();
  }
  request->blob_uuid = blob_uuid;
  request->blob_size = blob_size;
  request->credentials_mode = credentials_mode_;
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (info) {
    request->is_reload = ui::PageTransitionCoreTypeIs(
        info->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
    request->referrer =
        Referrer(GURL(request_->referrer()), info->GetReferrerPolicy());
  } else {
    CHECK(
        request_->referrer_policy() ==
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
    request->referrer =
        Referrer(GURL(request_->referrer()), blink::WebReferrerPolicyDefault);
  }
  return request.Pass();
}

bool ServiceWorkerURLRequestJob::CreateRequestBodyBlob(std::string* blob_uuid,
                                                       uint64* blob_size) {
  if (!body_.get() || !blob_storage_context_)
    return false;

  // To ensure the blobs stick around until the end of the reading.
  ScopedVector<storage::BlobDataHandle> handles;
  ScopedVector<storage::BlobDataSnapshot> snapshots;
  // TODO(dmurph): Allow blobs to be added below, so that the context can
  // efficiently re-use blob items for the new blob.
  std::vector<const ResourceRequestBody::Element*> resolved_elements;
  for (const ResourceRequestBody::Element& element : (*body_->elements())) {
    if (element.type() != ResourceRequestBody::Element::TYPE_BLOB) {
      resolved_elements.push_back(&element);
      continue;
    }
    scoped_ptr<storage::BlobDataHandle> handle =
        blob_storage_context_->GetBlobDataFromUUID(element.blob_uuid());
    scoped_ptr<storage::BlobDataSnapshot> snapshot = handle->CreateSnapshot();
    if (snapshot->items().empty())
      continue;
    const auto& items = snapshot->items();
    for (const auto& item : items) {
      DCHECK_NE(storage::DataElement::TYPE_BLOB, item->type());
      resolved_elements.push_back(item->data_element_ptr());
    }
    handles.push_back(handle.release());
    snapshots.push_back(snapshot.release());
  }

  const std::string uuid(base::GenerateGUID());
  uint64 total_size = 0;

  storage::BlobDataBuilder blob_builder(uuid);
  for (size_t i = 0; i < resolved_elements.size(); ++i) {
    const ResourceRequestBody::Element& element = *resolved_elements[i];
    if (total_size != kuint64max && element.length() != kuint64max)
      total_size += element.length();
    else
      total_size = kuint64max;
    switch (element.type()) {
      case ResourceRequestBody::Element::TYPE_BYTES:
        blob_builder.AppendData(element.bytes(), element.length());
        break;
      case ResourceRequestBody::Element::TYPE_FILE:
        blob_builder.AppendFile(element.path(), element.offset(),
                                element.length(),
                                element.expected_modification_time());
        break;
      case ResourceRequestBody::Element::TYPE_BLOB:
        // Blob elements should be resolved beforehand.
        NOTREACHED();
        break;
      case ResourceRequestBody::Element::TYPE_FILE_FILESYSTEM:
        blob_builder.AppendFileSystemFile(element.filesystem_url(),
                                          element.offset(), element.length(),
                                          element.expected_modification_time());
        break;
      default:
        NOTIMPLEMENTED();
    }
  }

  request_body_blob_data_handle_ =
      blob_storage_context_->AddFinishedBlob(&blob_builder);
  *blob_uuid = uuid;
  *blob_size = total_size;
  return true;
}

void ServiceWorkerURLRequestJob::DidPrepareFetchEvent() {
  worker_ready_time_ = base::TimeTicks::Now();
  load_timing_info_.send_start = worker_ready_time_;
}

void ServiceWorkerURLRequestJob::DidDispatchFetchEvent(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response,
    scoped_refptr<ServiceWorkerVersion> version) {
  fetch_dispatcher_.reset();
  ServiceWorkerMetrics::RecordFetchEventStatus(is_main_resource_load_, status);

  // Check if we're not orphaned.
  if (!request()) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_NO_REQUEST);
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH);
    if (is_main_resource_load_) {
      // Using the service worker failed, so fallback to network. Detach the
      // controller so subresource requests also skip the worker.
      provider_host_->NotifyControllerLost();
      response_type_ = FALLBACK_TO_NETWORK;
      NotifyRestartRequired();
    } else {
      DeliverErrorResponse();
    }
    return;
  }

  if (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK) {
    // When the request_mode is |CORS| or |CORS-with-forced-preflight| we can't
    // simply fallback to the network in the browser process. It is because the
    // CORS preflight logic is implemented in the renderer. So we returns a
    // fall_back_required response to the renderer.
    if (request_mode_ == FETCH_REQUEST_MODE_CORS ||
        request_mode_ == FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT) {
      fall_back_required_ = true;
      RecordResult(ServiceWorkerMetrics::REQUEST_JOB_FALLBACK_FOR_CORS);
      CreateResponseHeader(
          400, "Service Worker Fallback Required", ServiceWorkerHeaderMap());
      CommitResponseHeader();
      return;
    }
    // Change the response type and restart the request to fallback to
    // the network.
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_FALLBACK_RESPONSE);
    response_type_ = FALLBACK_TO_NETWORK;
    NotifyRestartRequired();
    return;
  }

  // We should have a response now.
  DCHECK_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, fetch_result);

  // A response with status code 0 is Blink telling us to respond with network
  // error.
  if (response.status_code == 0) {
    RecordStatusZeroResponseError(response.error);
    NotifyDone(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  load_timing_info_.send_end = base::TimeTicks::Now();

  // Creates a new HttpResponseInfo using the the ServiceWorker script's
  // HttpResponseInfo to show HTTPS padlock.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  DCHECK(!http_response_info_);
  DCHECK(version);
  const net::HttpResponseInfo* main_script_http_info =
      version->GetMainScriptHttpResponseInfo();
  if (main_script_http_info) {
    // In normal case |main_script_http_info| must be set while starting the
    // ServiceWorker. But when the ServiceWorker registration database was not
    // written correctly, it may be null.
    // TODO(horo): Change this line to DCHECK when crbug.com/485900 is fixed.
    http_response_info_.reset(
        new net::HttpResponseInfo(*main_script_http_info));
  }

  // Set up a request for reading the stream.
  if (response.stream_url.is_valid()) {
    DCHECK(response.blob_uuid.empty());
    SetResponseBodyType(STREAM);
    streaming_version_ = version;
    streaming_version_->AddStreamingURLRequestJob(this);
    response_url_ = response.url;
    service_worker_response_type_ = response.response_type;
    CreateResponseHeader(
        response.status_code, response.status_text, response.headers);
    load_timing_info_.receive_headers_end = base::TimeTicks::Now();
    StreamContext* stream_context =
        GetStreamContextForResourceContext(resource_context_);
    stream_ =
        stream_context->registry()->GetStream(response.stream_url);
    if (!stream_.get()) {
      waiting_stream_url_ = response.stream_url;
      // Wait for StreamHostMsg_StartBuilding message from the ServiceWorker.
      stream_context->registry()->SetRegisterObserver(waiting_stream_url_,
                                                      this);
      return;
    }
    stream_->SetReadObserver(this);
    CommitResponseHeader();
    return;
  }

  // Set up a request for reading the blob.
  if (!response.blob_uuid.empty() && blob_storage_context_) {
    SetResponseBodyType(BLOB);
    scoped_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response.blob_uuid);
    if (!blob_data_handle) {
      // The renderer gave us a bad blob UUID.
      RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_BLOB);
      DeliverErrorResponse();
      return;
    }
    blob_request_ = storage::BlobProtocolHandler::CreateBlobRequest(
        blob_data_handle.Pass(), request()->context(), this);
    blob_request_->Start();
  }

  response_url_ = response.url;
  service_worker_response_type_ = response.response_type;
  CreateResponseHeader(
      response.status_code, response.status_text, response.headers);
  load_timing_info_.receive_headers_end = base::TimeTicks::Now();
  if (!blob_request_) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_HEADERS_ONLY_RESPONSE);
    CommitResponseHeader();
  }
}

void ServiceWorkerURLRequestJob::CreateResponseHeader(
    int status_code,
    const std::string& status_text,
    const ServiceWorkerHeaderMap& headers) {
  // TODO(kinuko): If the response has an identifier to on-disk cache entry,
  // pull response header from the disk.
  std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", status_code, status_text.c_str()));
  status_line.push_back('\0');
  http_response_headers_ = new net::HttpResponseHeaders(status_line);
  for (ServiceWorkerHeaderMap::const_iterator it = headers.begin();
       it != headers.end();
       ++it) {
    std::string header;
    header.reserve(it->first.size() + 2 + it->second.size());
    header.append(it->first);
    header.append(": ");
    header.append(it->second);
    http_response_headers_->AddHeader(header);
  }
}

void ServiceWorkerURLRequestJob::CommitResponseHeader() {
  if (!http_response_info_)
    http_response_info_.reset(new net::HttpResponseInfo());
  http_response_info_->headers.swap(http_response_headers_);
  http_response_info_->vary_data = net::HttpVaryData();
  http_response_info_->metadata = nullptr;
  NotifyHeadersComplete();
}

void ServiceWorkerURLRequestJob::DeliverErrorResponse() {
  // TODO(falken): Print an error to the console of the ServiceWorker and of
  // the requesting page.
  CreateResponseHeader(
      500, "Service Worker Response Error", ServiceWorkerHeaderMap());
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::SetResponseBodyType(ResponseBodyType type) {
  DCHECK_EQ(response_body_type_, UNKNOWN);
  DCHECK_NE(type, UNKNOWN);
  response_body_type_ = type;
}

bool ServiceWorkerURLRequestJob::ShouldRecordResult() {
  return !did_record_result_ && is_started_ &&
         response_type_ == FORWARD_TO_SERVICE_WORKER;
}

void ServiceWorkerURLRequestJob::RecordResult(
    ServiceWorkerMetrics::URLRequestJobResult result) {
  // It violates style guidelines to handle a NOTREACHED() failure but if there
  // is a bug don't let it corrupt UMA results by double-counting.
  if (!ShouldRecordResult()) {
    NOTREACHED();
    return;
  }
  did_record_result_ = true;
  ServiceWorkerMetrics::RecordURLRequestJobResult(is_main_resource_load_,
                                                  result);
  if (request())
    request()->net_log().AddEvent(RequestJobResultToNetEventType(result));
}

void ServiceWorkerURLRequestJob::RecordStatusZeroResponseError(
    blink::WebServiceWorkerResponseError error) {
  // It violates style guidelines to handle a NOTREACHED() failure but if there
  // is a bug don't let it corrupt UMA results by double-counting.
  if (!ShouldRecordResult()) {
    NOTREACHED();
    return;
  }
  RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO);
  ServiceWorkerMetrics::RecordStatusZeroResponseError(is_main_resource_load_,
                                                      error);
}

void ServiceWorkerURLRequestJob::ClearStream() {
  if (streaming_version_) {
    streaming_version_->RemoveStreamingURLRequestJob(this);
    streaming_version_ = nullptr;
  }
  if (stream_) {
    stream_->RemoveReadObserver(this);
    stream_->Abort();
    stream_ = nullptr;
  }
  if (!waiting_stream_url_.is_empty()) {
    StreamRegistry* stream_registry =
        GetStreamContextForResourceContext(resource_context_)->registry();
    stream_registry->RemoveRegisterObserver(waiting_stream_url_);
    stream_registry->AbortPendingStream(waiting_stream_url_);
  }
}

}  // namespace content
