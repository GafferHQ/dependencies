// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_version_info.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_delegate.h"
#include "net/base/sdch_manager.h"
#include "net/base/sdch_net_log_params.h"
#include "net/cert/cert_status_flags.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_info.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/fraudulent_certificate_reporter.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/websockets/websocket_handshake_stream_base.h"

static const char kAvailDictionaryHeader[] = "Avail-Dictionary";

namespace net {

class URLRequestHttpJob::HttpFilterContext : public FilterContext {
 public:
  explicit HttpFilterContext(URLRequestHttpJob* job);
  ~HttpFilterContext() override;

  // FilterContext implementation.
  bool GetMimeType(std::string* mime_type) const override;
  bool GetURL(GURL* gurl) const override;
  base::Time GetRequestTime() const override;
  bool IsCachedContent() const override;
  SdchManager::DictionarySet* SdchDictionariesAdvertised() const override;
  int64 GetByteReadCount() const override;
  int GetResponseCode() const override;
  const URLRequestContext* GetURLRequestContext() const override;
  void RecordPacketStats(StatisticSelector statistic) const override;
  const BoundNetLog& GetNetLog() const override;

 private:
  URLRequestHttpJob* job_;

  // URLRequestHttpJob may be detached from URLRequest, but we still need to
  // return something.
  BoundNetLog dummy_log_;

  DISALLOW_COPY_AND_ASSIGN(HttpFilterContext);
};

URLRequestHttpJob::HttpFilterContext::HttpFilterContext(URLRequestHttpJob* job)
    : job_(job) {
  DCHECK(job_);
}

URLRequestHttpJob::HttpFilterContext::~HttpFilterContext() {
}

bool URLRequestHttpJob::HttpFilterContext::GetMimeType(
    std::string* mime_type) const {
  return job_->GetMimeType(mime_type);
}

bool URLRequestHttpJob::HttpFilterContext::GetURL(GURL* gurl) const {
  if (!job_->request())
    return false;
  *gurl = job_->request()->url();
  return true;
}

base::Time URLRequestHttpJob::HttpFilterContext::GetRequestTime() const {
  return job_->request() ? job_->request()->request_time() : base::Time();
}

bool URLRequestHttpJob::HttpFilterContext::IsCachedContent() const {
  return job_->is_cached_content_;
}

SdchManager::DictionarySet*
URLRequestHttpJob::HttpFilterContext::SdchDictionariesAdvertised() const {
  return job_->dictionaries_advertised_.get();
}

int64 URLRequestHttpJob::HttpFilterContext::GetByteReadCount() const {
  return job_->prefilter_bytes_read();
}

int URLRequestHttpJob::HttpFilterContext::GetResponseCode() const {
  return job_->GetResponseCode();
}

const URLRequestContext*
URLRequestHttpJob::HttpFilterContext::GetURLRequestContext() const {
  return job_->request() ? job_->request()->context() : NULL;
}

void URLRequestHttpJob::HttpFilterContext::RecordPacketStats(
    StatisticSelector statistic) const {
  job_->RecordPacketStats(statistic);
}

const BoundNetLog& URLRequestHttpJob::HttpFilterContext::GetNetLog() const {
  return job_->request() ? job_->request()->net_log() : dummy_log_;
}

// TODO(darin): make sure the port blocking code is not lost
// static
URLRequestJob* URLRequestHttpJob::Factory(URLRequest* request,
                                          NetworkDelegate* network_delegate,
                                          const std::string& scheme) {
  DCHECK(scheme == "http" || scheme == "https" || scheme == "ws" ||
         scheme == "wss");

  if (!request->context()->http_transaction_factory()) {
    NOTREACHED() << "requires a valid context";
    return new URLRequestErrorJob(
        request, network_delegate, ERR_INVALID_ARGUMENT);
  }

  GURL redirect_url;
  if (request->GetHSTSRedirect(&redirect_url)) {
    return new URLRequestRedirectJob(
        request, network_delegate, redirect_url,
        // Use status code 307 to preserve the method, so POST requests work.
        URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT, "HSTS");
  }
  return new URLRequestHttpJob(request,
                               network_delegate,
                               request->context()->http_user_agent_settings());
}

URLRequestHttpJob::URLRequestHttpJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const HttpUserAgentSettings* http_user_agent_settings)
    : URLRequestJob(request, network_delegate),
      priority_(DEFAULT_PRIORITY),
      response_info_(NULL),
      response_cookies_save_index_(0),
      proxy_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      server_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      start_callback_(base::Bind(&URLRequestHttpJob::OnStartCompleted,
                                 base::Unretained(this))),
      notify_before_headers_sent_callback_(
          base::Bind(&URLRequestHttpJob::NotifyBeforeSendHeadersCallback,
                     base::Unretained(this))),
      read_in_progress_(false),
      throttling_entry_(NULL),
      sdch_test_activated_(false),
      sdch_test_control_(false),
      is_cached_content_(false),
      request_creation_time_(),
      packet_timing_enabled_(false),
      done_(false),
      bytes_observed_in_packets_(0),
      request_time_snapshot_(),
      final_packet_time_(),
      filter_context_(new HttpFilterContext(this)),
      on_headers_received_callback_(
          base::Bind(&URLRequestHttpJob::OnHeadersReceivedCallback,
                     base::Unretained(this))),
      awaiting_callback_(false),
      http_user_agent_settings_(http_user_agent_settings),
      weak_factory_(this) {
  URLRequestThrottlerManager* manager = request->context()->throttler_manager();
  if (manager)
    throttling_entry_ = manager->RegisterRequestUrl(request->url());

  ResetTimer();
}

URLRequestHttpJob::~URLRequestHttpJob() {
  CHECK(!awaiting_callback_);

  DCHECK(!sdch_test_control_ || !sdch_test_activated_);
  if (!is_cached_content_) {
    if (sdch_test_control_)
      RecordPacketStats(FilterContext::SDCH_EXPERIMENT_HOLDBACK);
    if (sdch_test_activated_)
      RecordPacketStats(FilterContext::SDCH_EXPERIMENT_DECODE);
  }
  // Make sure SDCH filters are told to emit histogram data while
  // filter_context_ is still alive.
  DestroyFilters();

  DoneWithRequest(ABORTED);
}

void URLRequestHttpJob::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (transaction_)
    transaction_->SetPriority(priority_);
}

void URLRequestHttpJob::Start() {
  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("456327 URLRequestHttpJob::Start"));

  DCHECK(!transaction_.get());

  // URLRequest::SetReferrer ensures that we do not send username and password
  // fields in the referrer.
  GURL referrer(request_->referrer());

  request_info_.url = request_->url();
  request_info_.method = request_->method();
  request_info_.load_flags = request_->load_flags();
  // Enable privacy mode if cookie settings or flags tell us not send or
  // save cookies.
  bool enable_privacy_mode =
      (request_info_.load_flags & LOAD_DO_NOT_SEND_COOKIES) ||
      (request_info_.load_flags & LOAD_DO_NOT_SAVE_COOKIES) ||
      CanEnablePrivacyMode();
  // Privacy mode could still be disabled in OnCookiesLoaded if we are going
  // to send previously saved cookies.
  request_info_.privacy_mode = enable_privacy_mode ?
      PRIVACY_MODE_ENABLED : PRIVACY_MODE_DISABLED;

  // Strip Referer from request_info_.extra_headers to prevent, e.g., plugins
  // from overriding headers that are controlled using other means. Otherwise a
  // plugin could set a referrer although sending the referrer is inhibited.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kReferer);

  // Our consumer should have made sure that this is a safe referrer. See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (referrer.is_valid()) {
    request_info_.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                          referrer.spec());
  }

  request_info_.extra_headers.SetHeaderIfMissing(
      HttpRequestHeaders::kUserAgent,
      http_user_agent_settings_ ?
          http_user_agent_settings_->GetUserAgent() : std::string());

  AddExtraHeaders();
  AddCookieHeaderAndStart();
}

void URLRequestHttpJob::Kill() {
  if (!transaction_.get())
    return;

  weak_factory_.InvalidateWeakPtrs();
  DestroyTransaction();
  URLRequestJob::Kill();
}

void URLRequestHttpJob::GetConnectionAttempts(ConnectionAttempts* out) const {
  if (transaction_)
    transaction_->GetConnectionAttempts(out);
  else
    out->clear();
}

void URLRequestHttpJob::NotifyBeforeSendProxyHeadersCallback(
    const ProxyInfo& proxy_info,
    HttpRequestHeaders* request_headers) {
  DCHECK(request_headers);
  DCHECK_NE(URLRequestStatus::CANCELED, GetStatus().status());
  if (network_delegate()) {
    network_delegate()->NotifyBeforeSendProxyHeaders(
        request_,
        proxy_info,
        request_headers);
  }
}

void URLRequestHttpJob::NotifyHeadersComplete() {
  DCHECK(!response_info_);

  response_info_ = transaction_->GetResponseInfo();

  // Save boolean, as we'll need this info at destruction time, and filters may
  // also need this info.
  is_cached_content_ = response_info_->was_cached;

  if (!is_cached_content_ && throttling_entry_.get())
    throttling_entry_->UpdateWithResponse(GetResponseCode());

  // The ordering of these calls is not important.
  ProcessStrictTransportSecurityHeader();
  ProcessPublicKeyPinsHeader();

  // Handle the server notification of a new SDCH dictionary.
  SdchManager* sdch_manager(request()->context()->sdch_manager());
  if (sdch_manager) {
    SdchProblemCode rv = sdch_manager->IsInSupportedDomain(request()->url());
    if (rv != SDCH_OK) {
      // If SDCH is just disabled, it is not a real error.
      if (rv != SDCH_DISABLED) {
        SdchManager::SdchErrorRecovery(rv);
        request()->net_log().AddEvent(
            NetLog::TYPE_SDCH_DECODING_ERROR,
            base::Bind(&NetLogSdchResourceProblemCallback, rv));
      }
    } else {
      const std::string name = "Get-Dictionary";
      std::string url_text;
      void* iter = NULL;
      // TODO(jar): We need to not fetch dictionaries the first time they are
      // seen, but rather wait until we can justify their usefulness.
      // For now, we will only fetch the first dictionary, which will at least
      // require multiple suggestions before we get additional ones for this
      // site. Eventually we should wait until a dictionary is requested
      // several times
      // before we even download it (so that we don't waste memory or
      // bandwidth).
      if (GetResponseHeaders()->EnumerateHeader(&iter, name, &url_text)) {
        // Resolve suggested URL relative to request url.
        GURL sdch_dictionary_url = request_->url().Resolve(url_text);
        if (sdch_dictionary_url.is_valid()) {
          rv = sdch_manager->OnGetDictionary(request_->url(),
                                             sdch_dictionary_url);
          if (rv != SDCH_OK) {
            SdchManager::SdchErrorRecovery(rv);
            request_->net_log().AddEvent(
                NetLog::TYPE_SDCH_DICTIONARY_ERROR,
                base::Bind(&NetLogSdchDictionaryFetchProblemCallback, rv,
                           sdch_dictionary_url, false));
          }
        }
      }
    }
  }

  // Handle the server signalling no SDCH encoding.
  if (dictionaries_advertised_) {
    // We are wary of proxies that discard or damage SDCH encoding. If a server
    // explicitly states that this is not SDCH content, then we can correct our
    // assumption that this is an SDCH response, and avoid the need to recover
    // as though the content is corrupted (when we discover it is not SDCH
    // encoded).
    std::string sdch_response_status;
    void* iter = NULL;
    while (GetResponseHeaders()->EnumerateHeader(&iter, "X-Sdch-Encode",
                                                 &sdch_response_status)) {
      if (sdch_response_status == "0") {
        dictionaries_advertised_.reset();
        break;
      }
    }
  }

  // The HTTP transaction may be restarted several times for the purposes
  // of sending authorization information. Each time it restarts, we get
  // notified of the headers completion so that we can update the cookie store.
  if (transaction_->IsReadyToRestartForAuth()) {
    DCHECK(!response_info_->auth_challenge.get());
    // TODO(battre): This breaks the webrequest API for
    // URLRequestTestHTTP.BasicAuthWithCookies
    // where OnBeforeSendHeaders -> OnSendHeaders -> OnBeforeSendHeaders
    // occurs.
    RestartTransactionWithAuth(AuthCredentials());
    return;
  }

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::NotifyDone(const URLRequestStatus& status) {
  DoneWithRequest(FINISHED);
  URLRequestJob::NotifyDone(status);
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  DoneWithRequest(ABORTED);
  transaction_.reset();
  response_info_ = NULL;
  receive_headers_end_ = base::TimeTicks();
}

void URLRequestHttpJob::StartTransaction() {
  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequestHttpJob::StartTransaction"));

  if (network_delegate()) {
    OnCallToDelegate();
    int rv = network_delegate()->NotifyBeforeSendHeaders(
        request_, notify_before_headers_sent_callback_,
        &request_info_.extra_headers);
    // If an extension blocks the request, we rely on the callback to
    // MaybeStartTransactionInternal().
    if (rv == ERR_IO_PENDING)
      return;
    MaybeStartTransactionInternal(rv);
    return;
  }
  StartTransactionInternal();
}

void URLRequestHttpJob::NotifyBeforeSendHeadersCallback(int result) {
  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, GetStatus().status());

  MaybeStartTransactionInternal(result);
}

void URLRequestHttpJob::MaybeStartTransactionInternal(int result) {
  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequestHttpJob::MaybeStartTransactionInternal"));

  OnCallToDelegateComplete();
  if (result == OK) {
    StartTransactionInternal();
  } else {
    std::string source("delegate");
    request_->net_log().AddEvent(NetLog::TYPE_CANCELLED,
                                 NetLog::StringCallback("source", &source));
    NotifyCanceled();
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestHttpJob::StartTransactionInternal() {
  // NOTE: This method assumes that request_info_ is already setup properly.

  // If we already have a transaction, then we should restart the transaction
  // with auth provided by auth_credentials_.

  int rv;

  if (network_delegate()) {
    network_delegate()->NotifySendHeaders(
        request_, request_info_.extra_headers);
  }

  if (transaction_.get()) {
    rv = transaction_->RestartWithAuth(auth_credentials_, start_callback_);
    auth_credentials_ = AuthCredentials();
  } else {
    DCHECK(request_->context()->http_transaction_factory());

    rv = request_->context()->http_transaction_factory()->CreateTransaction(
        priority_, &transaction_);

    if (rv == OK && request_info_.url.SchemeIsWSOrWSS()) {
      base::SupportsUserData::Data* data = request_->GetUserData(
          WebSocketHandshakeStreamBase::CreateHelper::DataKey());
      if (data) {
        transaction_->SetWebSocketHandshakeStreamCreateHelper(
            static_cast<WebSocketHandshakeStreamBase::CreateHelper*>(data));
      } else {
        rv = ERR_DISALLOWED_URL_SCHEME;
      }
    }

    if (rv == OK) {
      transaction_->SetBeforeNetworkStartCallback(
          base::Bind(&URLRequestHttpJob::NotifyBeforeNetworkStart,
                     base::Unretained(this)));
      transaction_->SetBeforeProxyHeadersSentCallback(
          base::Bind(&URLRequestHttpJob::NotifyBeforeSendProxyHeadersCallback,
                     base::Unretained(this)));

      if (!throttling_entry_.get() ||
          !throttling_entry_->ShouldRejectRequest(*request_)) {
        rv = transaction_->Start(
            &request_info_, start_callback_, request_->net_log());
        start_time_ = base::TimeTicks::Now();
      } else {
        // Special error code for the exponential back-off module.
        rv = ERR_TEMPORARILY_THROTTLED;
      }
    }
  }

  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&URLRequestHttpJob::OnStartCompleted,
                            weak_factory_.GetWeakPtr(), rv));
}

void URLRequestHttpJob::AddExtraHeaders() {
  SdchManager* sdch_manager = request()->context()->sdch_manager();

  // Supply Accept-Encoding field only if it is not already provided.
  // It should be provided IF the content is known to have restrictions on
  // potential encoding, such as streaming multi-media.
  // For details see bug 47381.
  // TODO(jar, enal): jpeg files etc. should set up a request header if
  // possible. Right now it is done only by buffered_resource_loader and
  // simple_data_source.
  if (!request_info_.extra_headers.HasHeader(
      HttpRequestHeaders::kAcceptEncoding)) {
    // We don't support SDCH responses to POST as there is a possibility
    // of having SDCH encoded responses returned (e.g. by the cache)
    // which we cannot decode, and in those situations, we will need
    // to retransmit the request without SDCH, which is illegal for a POST.
    bool advertise_sdch = sdch_manager != NULL && request()->method() != "POST";
    if (advertise_sdch) {
      SdchProblemCode rv = sdch_manager->IsInSupportedDomain(request()->url());
      if (rv != SDCH_OK) {
        advertise_sdch = false;
        // If SDCH is just disabled, it is not a real error.
        if (rv != SDCH_DISABLED) {
          SdchManager::SdchErrorRecovery(rv);
          request()->net_log().AddEvent(
              NetLog::TYPE_SDCH_DECODING_ERROR,
              base::Bind(&NetLogSdchResourceProblemCallback, rv));
        }
      }
    }
    if (advertise_sdch) {
      dictionaries_advertised_ =
          sdch_manager->GetDictionarySet(request_->url());
    }

    // The AllowLatencyExperiment() is only true if we've successfully done a
    // full SDCH compression recently in this browser session for this host.
    // Note that for this path, there might be no applicable dictionaries,
    // and hence we can't participate in the experiment.
    if (dictionaries_advertised_ &&
        sdch_manager->AllowLatencyExperiment(request_->url())) {
      // We are participating in the test (or control), and hence we'll
      // eventually record statistics via either SDCH_EXPERIMENT_DECODE or
      // SDCH_EXPERIMENT_HOLDBACK, and we'll need some packet timing data.
      packet_timing_enabled_ = true;
      if (base::RandDouble() < .01) {
        sdch_test_control_ = true;  // 1% probability.
        dictionaries_advertised_.reset();
        advertise_sdch = false;
      } else {
        sdch_test_activated_ = true;
      }
    }

    // Supply Accept-Encoding headers first so that it is more likely that they
    // will be in the first transmitted packet. This can sometimes make it
    // easier to filter and analyze the streams to assure that a proxy has not
    // damaged these headers. Some proxies deliberately corrupt Accept-Encoding
    // headers.
    if (!advertise_sdch) {
      // Tell the server what compression formats we support (other than SDCH).
      request_info_.extra_headers.SetHeader(
          HttpRequestHeaders::kAcceptEncoding, "gzip, deflate");
    } else {
      // Include SDCH in acceptable list.
      request_info_.extra_headers.SetHeader(
          HttpRequestHeaders::kAcceptEncoding, "gzip, deflate, sdch");
      if (dictionaries_advertised_) {
        request_info_.extra_headers.SetHeader(
            kAvailDictionaryHeader,
            dictionaries_advertised_->GetDictionaryClientHashList());
        // Since we're tagging this transaction as advertising a dictionary,
        // we'll definitely employ an SDCH filter (or tentative sdch filter)
        // when we get a response. When done, we'll record histograms via
        // SDCH_DECODE or SDCH_PASSTHROUGH. Hence we need to record packet
        // arrival times.
        packet_timing_enabled_ = true;
      }
    }
  }

  if (http_user_agent_settings_) {
    // Only add default Accept-Language if the request didn't have it
    // specified.
    std::string accept_language =
        http_user_agent_settings_->GetAcceptLanguage();
    if (!accept_language.empty()) {
      request_info_.extra_headers.SetHeaderIfMissing(
          HttpRequestHeaders::kAcceptLanguage,
          accept_language);
    }
  }
}

void URLRequestHttpJob::AddCookieHeaderAndStart() {
  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  // If the request was destroyed, then there is no more work to do.
  if (!request_)
    return;

  CookieStore* cookie_store = request_->context()->cookie_store();
  if (cookie_store && !(request_info_.load_flags & LOAD_DO_NOT_SEND_COOKIES)) {
    cookie_store->GetAllCookiesForURLAsync(
        request_->url(),
        base::Bind(&URLRequestHttpJob::CheckCookiePolicyAndLoad,
                   weak_factory_.GetWeakPtr()));
  } else {
    DoStartTransaction();
  }
}

void URLRequestHttpJob::DoLoadCookies() {
  CookieOptions options;
  options.set_include_httponly();

  // TODO(mkwst): Drop this `if` once we decide whether or not to ship
  // first-party cookies: https://crbug.com/459154
  if (network_delegate() &&
      network_delegate()->FirstPartyOnlyCookieExperimentEnabled())
    options.set_first_party_url(request_->first_party_for_cookies());
  else
    options.set_include_first_party_only();

  request_->context()->cookie_store()->GetCookiesWithOptionsAsync(
      request_->url(), options, base::Bind(&URLRequestHttpJob::OnCookiesLoaded,
                                           weak_factory_.GetWeakPtr()));
}

void URLRequestHttpJob::CheckCookiePolicyAndLoad(
    const CookieList& cookie_list) {
  if (CanGetCookies(cookie_list))
    DoLoadCookies();
  else
    DoStartTransaction();
}

void URLRequestHttpJob::OnCookiesLoaded(const std::string& cookie_line) {
  if (!cookie_line.empty()) {
    request_info_.extra_headers.SetHeader(
        HttpRequestHeaders::kCookie, cookie_line);
    // Disable privacy mode as we are sending cookies anyway.
    request_info_.privacy_mode = PRIVACY_MODE_DISABLED;
  }
  DoStartTransaction();
}

void URLRequestHttpJob::DoStartTransaction() {
  // We may have been canceled while retrieving cookies.
  if (GetStatus().is_success()) {
    StartTransaction();
  } else {
    NotifyCanceled();
  }
}

void URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete(int result) {
  // End of the call started in OnStartCompleted.
  OnCallToDelegateComplete();

  if (result != OK) {
    std::string source("delegate");
    request_->net_log().AddEvent(NetLog::TYPE_CANCELLED,
                                 NetLog::StringCallback("source", &source));
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
    return;
  }

  DCHECK(transaction_.get());

  const HttpResponseInfo* response_info = transaction_->GetResponseInfo();
  DCHECK(response_info);

  response_cookies_.clear();
  response_cookies_save_index_ = 0;

  FetchResponseCookies(&response_cookies_);

  if (!GetResponseHeaders()->GetDateValue(&response_date_))
    response_date_ = base::Time();

  // Now, loop over the response cookies, and attempt to persist each.
  SaveNextCookie();
}

// If the save occurs synchronously, SaveNextCookie will loop and save the next
// cookie. If the save is deferred, the callback is responsible for continuing
// to iterate through the cookies.
// TODO(erikwright): Modify the CookieStore API to indicate via return value
// whether it completed synchronously or asynchronously.
// See http://crbug.com/131066.
void URLRequestHttpJob::SaveNextCookie() {
  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  // Used to communicate with the callback. See the implementation of
  // OnCookieSaved.
  scoped_refptr<SharedBoolean> callback_pending = new SharedBoolean(false);
  scoped_refptr<SharedBoolean> save_next_cookie_running =
      new SharedBoolean(true);

  if (!(request_info_.load_flags & LOAD_DO_NOT_SAVE_COOKIES) &&
      request_->context()->cookie_store() && response_cookies_.size() > 0) {
    CookieOptions options;
    options.set_include_httponly();
    options.set_server_time(response_date_);

    CookieStore::SetCookiesCallback callback(base::Bind(
        &URLRequestHttpJob::OnCookieSaved, weak_factory_.GetWeakPtr(),
        save_next_cookie_running, callback_pending));

    // Loop through the cookies as long as SetCookieWithOptionsAsync completes
    // synchronously.
    while (!callback_pending->data &&
           response_cookies_save_index_ < response_cookies_.size()) {
      if (CanSetCookie(
          response_cookies_[response_cookies_save_index_], &options)) {
        callback_pending->data = true;
        request_->context()->cookie_store()->SetCookieWithOptionsAsync(
            request_->url(), response_cookies_[response_cookies_save_index_],
            options, callback);
      }
      ++response_cookies_save_index_;
    }
  }

  save_next_cookie_running->data = false;

  if (!callback_pending->data) {
    response_cookies_.clear();
    response_cookies_save_index_ = 0;
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status
    NotifyHeadersComplete();
    return;
  }
}

// |save_next_cookie_running| is true when the callback is bound and set to
// false when SaveNextCookie exits, allowing the callback to determine if the
// save occurred synchronously or asynchronously.
// |callback_pending| is false when the callback is invoked and will be set to
// true by the callback, allowing SaveNextCookie to detect whether the save
// occurred synchronously.
// See SaveNextCookie() for more information.
void URLRequestHttpJob::OnCookieSaved(
    scoped_refptr<SharedBoolean> save_next_cookie_running,
    scoped_refptr<SharedBoolean> callback_pending,
    bool cookie_status) {
  callback_pending->data = false;

  // If we were called synchronously, return.
  if (save_next_cookie_running->data) {
    return;
  }

  // We were called asynchronously, so trigger the next save.
  // We may have been canceled within OnSetCookie.
  if (GetStatus().is_success()) {
    SaveNextCookie();
  } else {
    NotifyCanceled();
  }
}

void URLRequestHttpJob::FetchResponseCookies(
    std::vector<std::string>* cookies) {
  const std::string name = "Set-Cookie";
  std::string value;

  void* iter = NULL;
  HttpResponseHeaders* headers = GetResponseHeaders();
  while (headers->EnumerateHeader(&iter, name, &value)) {
    if (!value.empty())
      cookies->push_back(value);
  }
}

// NOTE: |ProcessStrictTransportSecurityHeader| and
// |ProcessPublicKeyPinsHeader| have very similar structures, by design.
void URLRequestHttpJob::ProcessStrictTransportSecurityHeader() {
  DCHECK(response_info_);
  TransportSecurityState* security_state =
      request_->context()->transport_security_state();
  const SSLInfo& ssl_info = response_info_->ssl_info;

  // Only accept HSTS headers on HTTPS connections that have no
  // certificate errors.
  if (!ssl_info.is_valid() || IsCertStatusError(ssl_info.cert_status) ||
      !security_state)
    return;

  // Don't accept HSTS headers when the hostname is an IP address.
  if (request_info_.url.HostIsIPAddress())
    return;

  // http://tools.ietf.org/html/draft-ietf-websec-strict-transport-sec:
  //
  //   If a UA receives more than one STS header field in a HTTP response
  //   message over secure transport, then the UA MUST process only the
  //   first such header field.
  HttpResponseHeaders* headers = GetResponseHeaders();
  std::string value;
  if (headers->EnumerateHeader(NULL, "Strict-Transport-Security", &value))
    security_state->AddHSTSHeader(request_info_.url.host(), value);
}

void URLRequestHttpJob::ProcessPublicKeyPinsHeader() {
  DCHECK(response_info_);
  TransportSecurityState* security_state =
      request_->context()->transport_security_state();
  const SSLInfo& ssl_info = response_info_->ssl_info;

  // Only accept HPKP headers on HTTPS connections that have no
  // certificate errors.
  if (!ssl_info.is_valid() || IsCertStatusError(ssl_info.cert_status) ||
      !security_state)
    return;

  // Don't accept HSTS headers when the hostname is an IP address.
  if (request_info_.url.HostIsIPAddress())
    return;

  // http://tools.ietf.org/html/draft-ietf-websec-key-pinning:
  //
  //   If a UA receives more than one PKP header field in an HTTP
  //   response message over secure transport, then the UA MUST process
  //   only the first such header field.
  HttpResponseHeaders* headers = GetResponseHeaders();
  std::string value;
  if (headers->EnumerateHeader(NULL, "Public-Key-Pins", &value))
    security_state->AddHPKPHeader(request_info_.url.host(), value, ssl_info);
}

void URLRequestHttpJob::OnStartCompleted(int result) {
  RecordTimer();

  // If the request was destroyed, then there is no more work to do.
  if (!request_)
    return;

  // If the job is done (due to cancellation), can just ignore this
  // notification.
  if (done_)
    return;

  receive_headers_end_ = base::TimeTicks::Now();

  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());

  const URLRequestContext* context = request_->context();

  if (result == ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN &&
      transaction_->GetResponseInfo() != NULL) {
    FraudulentCertificateReporter* reporter =
      context->fraudulent_certificate_reporter();
    if (reporter != NULL) {
      const SSLInfo& ssl_info = transaction_->GetResponseInfo()->ssl_info;
      const std::string& host = request_->url().host();

      reporter->SendReport(host, ssl_info);
    }
  }

  if (result == OK) {
    if (transaction_ && transaction_->GetResponseInfo()) {
      SetProxyServer(transaction_->GetResponseInfo()->proxy_server);
    }
    scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();
    if (network_delegate()) {
      // Note that |this| may not be deleted until
      // |on_headers_received_callback_| or
      // |NetworkDelegate::URLRequestDestroyed()| has been called.
      OnCallToDelegate();
      allowed_unsafe_redirect_url_ = GURL();
      int error = network_delegate()->NotifyHeadersReceived(
          request_,
          on_headers_received_callback_,
          headers.get(),
          &override_response_headers_,
          &allowed_unsafe_redirect_url_);
      if (error != OK) {
        if (error == ERR_IO_PENDING) {
          awaiting_callback_ = true;
        } else {
          std::string source("delegate");
          request_->net_log().AddEvent(NetLog::TYPE_CANCELLED,
                                       NetLog::StringCallback("source",
                                                              &source));
          OnCallToDelegateComplete();
          NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, error));
        }
        return;
      }
    }

    SaveCookiesAndNotifyHeadersComplete(OK);
  } else if (IsCertificateError(result)) {
    // We encountered an SSL certificate error.
    if (result == ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY ||
        result == ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN) {
      // These are hard failures. They're handled separately and don't have
      // the correct cert status, so set it here.
      SSLInfo info(transaction_->GetResponseInfo()->ssl_info);
      info.cert_status = MapNetErrorToCertStatus(result);
      NotifySSLCertificateError(info, true);
    } else {
      // Maybe overridable, maybe not. Ask the delegate to decide.
      TransportSecurityState* state = context->transport_security_state();
      const bool fatal =
          state && state->ShouldSSLErrorsBeFatal(request_info_.url.host());
      NotifySSLCertificateError(
          transaction_->GetResponseInfo()->ssl_info, fatal);
    }
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    NotifyCertificateRequested(
        transaction_->GetResponseInfo()->cert_request_info.get());
  } else {
    // Even on an error, there may be useful information in the response
    // info (e.g. whether there's a cached copy).
    if (transaction_.get())
      response_info_ = transaction_->GetResponseInfo();
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestHttpJob::OnHeadersReceivedCallback(int result) {
  awaiting_callback_ = false;

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, GetStatus().status());

  SaveCookiesAndNotifyHeadersComplete(result);
}

void URLRequestHttpJob::OnReadCompleted(int result) {
  read_in_progress_ = false;

  if (ShouldFixMismatchedContentLength(result))
    result = OK;

  if (result == OK) {
    NotifyDone(URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  } else {
    // Clear the IO_PENDING status
    SetStatus(URLRequestStatus());
  }

  NotifyReadComplete(result);
}

void URLRequestHttpJob::RestartTransactionWithAuth(
    const AuthCredentials& credentials) {
  auth_credentials_ = credentials;

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  receive_headers_end_ = base::TimeTicks();
  response_cookies_.clear();

  ResetTimer();

  // Update the cookies, since the cookie store may have been updated from the
  // headers in the 401/407. Since cookies were already appended to
  // extra_headers, we need to strip them out before adding them again.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kCookie);

  AddCookieHeaderAndStart();
}

void URLRequestHttpJob::SetUpload(UploadDataStream* upload) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.upload_data_stream = upload;
}

void URLRequestHttpJob::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.extra_headers.CopyFrom(headers);
}

LoadState URLRequestHttpJob::GetLoadState() const {
  return transaction_.get() ?
      transaction_->GetLoadState() : LOAD_STATE_IDLE;
}

UploadProgress URLRequestHttpJob::GetUploadProgress() const {
  return transaction_.get() ?
      transaction_->GetUploadProgress() : UploadProgress();
}

bool URLRequestHttpJob::GetMimeType(std::string* mime_type) const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  HttpResponseHeaders* headers = GetResponseHeaders();
  if (!headers)
    return false;
  return headers->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return GetResponseHeaders()->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(HttpResponseInfo* info) {
  DCHECK(request_);

  if (response_info_) {
    DCHECK(transaction_.get());

    *info = *response_info_;
    if (override_response_headers_.get())
      info->headers = override_response_headers_;
  }
}

void URLRequestHttpJob::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  // If haven't made it far enough to receive any headers, don't return
  // anything. This makes for more consistent behavior in the case of errors.
  if (!transaction_ || receive_headers_end_.is_null())
    return;
  if (transaction_->GetLoadTimingInfo(load_timing_info))
    load_timing_info->receive_headers_end = receive_headers_end_;
}

bool URLRequestHttpJob::GetResponseCookies(std::vector<std::string>* cookies) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  // TODO(darin): Why are we extracting response cookies again?  Perhaps we
  // should just leverage response_cookies_.

  cookies->clear();
  FetchResponseCookies(cookies);
  return true;
}

int URLRequestHttpJob::GetResponseCode() const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return -1;

  return GetResponseHeaders()->response_code();
}

Filter* URLRequestHttpJob::SetupFilter() const {
  DCHECK(transaction_.get());
  if (!response_info_)
    return NULL;

  std::vector<Filter::FilterType> encoding_types;
  std::string encoding_type;
  HttpResponseHeaders* headers = GetResponseHeaders();
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, "Content-Encoding", &encoding_type)) {
    encoding_types.push_back(Filter::ConvertEncodingToType(encoding_type));
  }

  // Even if encoding types are empty, there is a chance that we need to add
  // some decoding, as some proxies strip encoding completely. In such cases,
  // we may need to add (for example) SDCH filtering (when the context suggests
  // it is appropriate).
  Filter::FixupEncodingTypes(*filter_context_, &encoding_types);

  return !encoding_types.empty()
      ? Filter::Factory(encoding_types, *filter_context_) : NULL;
}

bool URLRequestHttpJob::CopyFragmentOnRedirect(const GURL& location) const {
  // Allow modification of reference fragments by default, unless
  // |allowed_unsafe_redirect_url_| is set and equal to the redirect URL.
  // When this is the case, we assume that the network delegate has set the
  // desired redirect URL (with or without fragment), so it must not be changed
  // any more.
  return !allowed_unsafe_redirect_url_.is_valid() ||
       allowed_unsafe_redirect_url_ != location;
}

bool URLRequestHttpJob::IsSafeRedirect(const GURL& location) {
  // HTTP is always safe.
  // TODO(pauljensen): Remove once crbug.com/146591 is fixed.
  if (location.is_valid() &&
      (location.scheme() == "http" || location.scheme() == "https")) {
    return true;
  }
  // Delegates may mark a URL as safe for redirection.
  if (allowed_unsafe_redirect_url_.is_valid() &&
      allowed_unsafe_redirect_url_ == location) {
    return true;
  }
  // Query URLRequestJobFactory as to whether |location| would be safe to
  // redirect to.
  return request_->context()->job_factory() &&
      request_->context()->job_factory()->IsSafeRedirectTarget(location);
}

bool URLRequestHttpJob::NeedsAuth() {
  int code = GetResponseCode();
  if (code == -1)
    return false;

  // Check if we need either Proxy or WWW Authentication. This could happen
  // because we either provided no auth info, or provided incorrect info.
  switch (code) {
    case 407:
      if (proxy_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      proxy_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
    case 401:
      if (server_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      server_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
  }
  return false;
}

void URLRequestHttpJob::GetAuthChallengeInfo(
    scoped_refptr<AuthChallengeInfo>* result) {
  DCHECK(transaction_.get());
  DCHECK(response_info_);

  // sanity checks:
  DCHECK(proxy_auth_state_ == AUTH_STATE_NEED_AUTH ||
         server_auth_state_ == AUTH_STATE_NEED_AUTH);
  DCHECK((GetResponseHeaders()->response_code() == HTTP_UNAUTHORIZED) ||
         (GetResponseHeaders()->response_code() ==
          HTTP_PROXY_AUTHENTICATION_REQUIRED));

  *result = response_info_->auth_challenge;
}

void URLRequestHttpJob::SetAuth(const AuthCredentials& credentials) {
  DCHECK(transaction_.get());

  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_HAVE_AUTH;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_HAVE_AUTH;
  }

  RestartTransactionWithAuth(credentials);
}

void URLRequestHttpJob::CancelAuth() {
  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_CANCELED;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_CANCELED;
  }

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  receive_headers_end_ = base::TimeTicks::Now();
  response_cookies_.clear();

  ResetTimer();

  // OK, let the consumer read the error page...
  //
  // Because we set the AUTH_STATE_CANCELED flag, NeedsAuth will return false,
  // which will cause the consumer to receive OnResponseStarted instead of
  // OnAuthRequired.
  //
  // We have to do this via InvokeLater to avoid "recursing" the consumer.
  //
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&URLRequestHttpJob::OnStartCompleted,
                            weak_factory_.GetWeakPtr(), OK));
}

void URLRequestHttpJob::ContinueWithCertificate(
    X509Certificate* client_cert) {
  DCHECK(transaction_.get());

  DCHECK(!response_info_) << "should not have a response yet";
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartWithCertificate(client_cert, start_callback_);
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&URLRequestHttpJob::OnStartCompleted,
                            weak_factory_.GetWeakPtr(), rv));
}

void URLRequestHttpJob::ContinueDespiteLastError() {
  // If the transaction was destroyed, then the job was cancelled.
  if (!transaction_.get())
    return;

  DCHECK(!response_info_) << "should not have a response yet";
  receive_headers_end_ = base::TimeTicks();

  ResetTimer();

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartIgnoringLastError(start_callback_);
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&URLRequestHttpJob::OnStartCompleted,
                            weak_factory_.GetWeakPtr(), rv));
}

void URLRequestHttpJob::ResumeNetworkStart() {
  DCHECK(transaction_.get());
  transaction_->ResumeNetworkStart();
}

bool URLRequestHttpJob::ShouldFixMismatchedContentLength(int rv) const {
  // Some servers send the body compressed, but specify the content length as
  // the uncompressed size. Although this violates the HTTP spec we want to
  // support it (as IE and FireFox do), but *only* for an exact match.
  // See http://crbug.com/79694.
  if (rv == ERR_CONTENT_LENGTH_MISMATCH ||
      rv == ERR_INCOMPLETE_CHUNKED_ENCODING) {
    if (request_ && request_->response_headers()) {
      int64 expected_length = request_->response_headers()->GetContentLength();
      VLOG(1) << __FUNCTION__ << "() "
              << "\"" << request_->url().spec() << "\""
              << " content-length = " << expected_length
              << " pre total = " << prefilter_bytes_read()
              << " post total = " << postfilter_bytes_read();
      if (postfilter_bytes_read() == expected_length) {
        // Clear the error.
        return true;
      }
    }
  }
  return false;
}

bool URLRequestHttpJob::ReadRawData(IOBuffer* buf, int buf_size,
                                    int* bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);

  int rv = transaction_->Read(
      buf, buf_size,
      base::Bind(&URLRequestHttpJob::OnReadCompleted, base::Unretained(this)));

  if (ShouldFixMismatchedContentLength(rv))
    rv = 0;

  if (rv >= 0) {
    *bytes_read = rv;
    if (!rv)
      DoneWithRequest(FINISHED);
    return true;
  }

  if (rv == ERR_IO_PENDING) {
    read_in_progress_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }

  return false;
}

void URLRequestHttpJob::StopCaching() {
  if (transaction_.get())
    transaction_->StopCaching();
}

bool URLRequestHttpJob::GetFullRequestHeaders(
    HttpRequestHeaders* headers) const {
  if (!transaction_)
    return false;

  return transaction_->GetFullRequestHeaders(headers);
}

int64 URLRequestHttpJob::GetTotalReceivedBytes() const {
  if (!transaction_)
    return 0;

  return transaction_->GetTotalReceivedBytes();
}

void URLRequestHttpJob::DoneReading() {
  if (transaction_) {
    transaction_->DoneReading();
  }
  DoneWithRequest(FINISHED);
}

void URLRequestHttpJob::DoneReadingRedirectResponse() {
  if (transaction_) {
    if (transaction_->GetResponseInfo()->headers->IsRedirect(NULL)) {
      // If the original headers indicate a redirect, go ahead and cache the
      // response, even if the |override_response_headers_| are a redirect to
      // another location.
      transaction_->DoneReading();
    } else {
      // Otherwise, |override_response_headers_| must be non-NULL and contain
      // bogus headers indicating a redirect.
      DCHECK(override_response_headers_.get());
      DCHECK(override_response_headers_->IsRedirect(NULL));
      transaction_->StopCaching();
    }
  }
  DoneWithRequest(FINISHED);
}

HostPortPair URLRequestHttpJob::GetSocketAddress() const {
  return response_info_ ? response_info_->socket_address : HostPortPair();
}

void URLRequestHttpJob::RecordTimer() {
  if (request_creation_time_.is_null()) {
    NOTREACHED()
        << "The same transaction shouldn't start twice without new timing.";
    return;
  }

  base::TimeDelta to_start = base::Time::Now() - request_creation_time_;
  request_creation_time_ = base::Time();

  UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpTimeToFirstByte", to_start);
}

void URLRequestHttpJob::ResetTimer() {
  if (!request_creation_time_.is_null()) {
    NOTREACHED()
        << "The timer was reset before it was recorded.";
    return;
  }
  request_creation_time_ = base::Time::Now();
}

void URLRequestHttpJob::UpdatePacketReadTimes() {
  if (!packet_timing_enabled_)
    return;

  DCHECK_GT(prefilter_bytes_read(), bytes_observed_in_packets_);

  base::Time now(base::Time::Now());
  if (!bytes_observed_in_packets_)
    request_time_snapshot_ = now;
  final_packet_time_ = now;

  bytes_observed_in_packets_ = prefilter_bytes_read();
}

void URLRequestHttpJob::RecordPacketStats(
    FilterContext::StatisticSelector statistic) const {
  if (!packet_timing_enabled_ || (final_packet_time_ == base::Time()))
    return;

  base::TimeDelta duration = final_packet_time_ - request_time_snapshot_;
  switch (statistic) {
    case FilterContext::SDCH_DECODE: {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Sdch3.Network_Decode_Bytes_Processed_b",
          static_cast<int>(bytes_observed_in_packets_), 500, 100000, 100);
      return;
    }
    case FilterContext::SDCH_PASSTHROUGH: {
      // Despite advertising a dictionary, we handled non-sdch compressed
      // content.
      return;
    }

    case FilterContext::SDCH_EXPERIMENT_DECODE: {
      UMA_HISTOGRAM_CUSTOM_TIMES("Sdch3.Experiment3_Decode",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      return;
    }
    case FilterContext::SDCH_EXPERIMENT_HOLDBACK: {
      UMA_HISTOGRAM_CUSTOM_TIMES("Sdch3.Experiment3_Holdback",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      return;
    }
    default:
      NOTREACHED();
      return;
  }
}

void URLRequestHttpJob::RecordPerfHistograms(CompletionCause reason) {
  if (start_time_.is_null())
    return;

  base::TimeDelta total_time = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTime", total_time);

  if (reason == FINISHED) {
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeSuccess", total_time);
  } else {
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeCancel", total_time);
  }

  if (response_info_) {
    bool is_google = request() && HasGoogleHost(request()->url());
    bool used_quic = response_info_->DidUseQuic();
    if (is_google) {
      if (used_quic) {
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTime.Quic", total_time);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTime.NotQuic", total_time);
      }
    }
    if (response_info_->was_cached) {
      UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeCached", total_time);
      if (is_google) {
        if (used_quic) {
          UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTimeCached.Quic",
                                     total_time);
        } else {
          UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTimeCached.NotQuic",
                                     total_time);
        }
      }
    } else  {
      UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeNotCached", total_time);
      if (is_google) {
        if (used_quic) {
          UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTimeNotCached.Quic",
                                     total_time);
        } else {
          UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpJob.TotalTimeNotCached.NotQuic",
                                     total_time);
        }
      }
    }
  }

  if (request_info_.load_flags & LOAD_PREFETCH && !request_->was_cached())
    UMA_HISTOGRAM_COUNTS("Net.Prefetch.PrefilterBytesReadFromNetwork",
                         prefilter_bytes_read());

  start_time_ = base::TimeTicks();
}

void URLRequestHttpJob::DoneWithRequest(CompletionCause reason) {
  if (done_)
    return;
  done_ = true;
  RecordPerfHistograms(reason);
  if (reason == FINISHED) {
    request_->set_received_response_content_length(prefilter_bytes_read());
  }
}

HttpResponseHeaders* URLRequestHttpJob::GetResponseHeaders() const {
  DCHECK(transaction_.get());
  DCHECK(transaction_->GetResponseInfo());
  return override_response_headers_.get() ?
             override_response_headers_.get() :
             transaction_->GetResponseInfo()->headers.get();
}

void URLRequestHttpJob::NotifyURLRequestDestroyed() {
  awaiting_callback_ = false;
}

}  // namespace net
