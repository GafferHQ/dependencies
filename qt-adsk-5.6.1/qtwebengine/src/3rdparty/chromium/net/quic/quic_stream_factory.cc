// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include <algorithm>
#include <set>

#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/http/http_server_properties.h"
#include "net/quic/crypto/channel_id_chromium.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/port_suggester.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_id.h"
#include "net/socket/client_socket_factory.h"
#include "net/udp/udp_client_socket.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(USE_OPENSSL)
#include <openssl/aead.h>
#include "crypto/openssl_util.h"
#else
#include "base/cpu.h"
#endif

namespace net {

namespace {

enum CreateSessionFailure {
  CREATION_ERROR_CONNECTING_SOCKET,
  CREATION_ERROR_SETTING_RECEIVE_BUFFER,
  CREATION_ERROR_SETTING_SEND_BUFFER,
  CREATION_ERROR_MAX
};

// When a connection is idle for 30 seconds it will be closed.
const int kIdleConnectionTimeoutSeconds = 30;

// The maximum receive window sizes for QUIC sessions and streams.
const int32 kQuicSessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
const int32 kQuicStreamMaxRecvWindowSize = 6 * 1024 * 1024;    // 6 MB

// Set the maximum number of undecryptable packets the connection will store.
const int32 kMaxUndecryptablePackets = 100;

void HistogramCreateSessionFailure(enum CreateSessionFailure error) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.CreationError", error,
                            CREATION_ERROR_MAX);
}

bool IsEcdsaSupported() {
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;
#endif

  return true;
}

QuicConfig InitializeQuicConfig(const QuicTagVector& connection_options) {
  QuicConfig config;
  config.SetIdleConnectionStateLifetime(
      QuicTime::Delta::FromSeconds(kIdleConnectionTimeoutSeconds),
      QuicTime::Delta::FromSeconds(kIdleConnectionTimeoutSeconds));
  config.SetConnectionOptionsToSend(connection_options);
  return config;
}

class DefaultPacketWriterFactory : public QuicConnection::PacketWriterFactory {
 public:
  explicit DefaultPacketWriterFactory(DatagramClientSocket* socket)
      : socket_(socket) {}
  ~DefaultPacketWriterFactory() override {}

  QuicPacketWriter* Create(QuicConnection* connection) const override;

 private:
  DatagramClientSocket* socket_;
};

QuicPacketWriter* DefaultPacketWriterFactory::Create(
    QuicConnection* connection) const {
  scoped_ptr<QuicDefaultPacketWriter> writer(
      new QuicDefaultPacketWriter(socket_));
  writer->SetConnection(connection);
  return writer.release();
}

}  // namespace

QuicStreamFactory::IpAliasKey::IpAliasKey() {}

QuicStreamFactory::IpAliasKey::IpAliasKey(IPEndPoint ip_endpoint,
                                          bool is_https)
    : ip_endpoint(ip_endpoint),
      is_https(is_https) {}

QuicStreamFactory::IpAliasKey::~IpAliasKey() {}

bool QuicStreamFactory::IpAliasKey::operator<(
    const QuicStreamFactory::IpAliasKey& other) const {
  if (!(ip_endpoint == other.ip_endpoint)) {
    return ip_endpoint < other.ip_endpoint;
  }
  return is_https < other.is_https;
}

bool QuicStreamFactory::IpAliasKey::operator==(
    const QuicStreamFactory::IpAliasKey& other) const {
  return is_https == other.is_https &&
      ip_endpoint == other.ip_endpoint;
};

// Responsible for creating a new QUIC session to the specified server, and
// for notifying any associated requests when complete.
class QuicStreamFactory::Job {
 public:
  Job(QuicStreamFactory* factory,
      HostResolver* host_resolver,
      const HostPortPair& host_port_pair,
      bool server_and_origin_have_same_host,
      bool is_https,
      bool was_alternative_service_recently_broken,
      PrivacyMode privacy_mode,
      int cert_verify_flags,
      bool is_post,
      QuicServerInfo* server_info,
      const BoundNetLog& net_log);

  // Creates a new job to handle the resumption of for connecting an
  // existing session.
  Job(QuicStreamFactory* factory,
      HostResolver* host_resolver,
      QuicClientSession* session,
      QuicServerId server_id);

  ~Job();

  int Run(const CompletionCallback& callback);

  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoLoadServerInfo();
  int DoLoadServerInfoComplete(int rv);
  int DoConnect();
  int DoResumeConnect();
  int DoConnectComplete(int rv);

  void OnIOComplete(int rv);

  void RunAuxilaryJob();

  void Cancel();

  void CancelWaitForDataReadyCallback();

  const QuicServerId server_id() const { return server_id_; }

  base::WeakPtr<Job> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_LOAD_SERVER_INFO,
    STATE_LOAD_SERVER_INFO_COMPLETE,
    STATE_CONNECT,
    STATE_RESUME_CONNECT,
    STATE_CONNECT_COMPLETE,
  };
  IoState io_state_;

  QuicStreamFactory* factory_;
  SingleRequestHostResolver host_resolver_;
  QuicServerId server_id_;
  int cert_verify_flags_;
  // True if and only if server and origin have the same hostname.
  bool server_and_origin_have_same_host_;
  bool is_post_;
  bool was_alternative_service_recently_broken_;
  scoped_ptr<QuicServerInfo> server_info_;
  bool started_another_job_;
  const BoundNetLog net_log_;
  QuicClientSession* session_;
  CompletionCallback callback_;
  AddressList address_list_;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;
  base::WeakPtrFactory<Job> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Job);
};

QuicStreamFactory::Job::Job(QuicStreamFactory* factory,
                            HostResolver* host_resolver,
                            const HostPortPair& host_port_pair,
                            bool server_and_origin_have_same_host,
                            bool is_https,
                            bool was_alternative_service_recently_broken,
                            PrivacyMode privacy_mode,
                            int cert_verify_flags,
                            bool is_post,
                            QuicServerInfo* server_info,
                            const BoundNetLog& net_log)
    : io_state_(STATE_RESOLVE_HOST),
      factory_(factory),
      host_resolver_(host_resolver),
      server_id_(host_port_pair, is_https, privacy_mode),
      cert_verify_flags_(cert_verify_flags),
      server_and_origin_have_same_host_(server_and_origin_have_same_host),
      is_post_(is_post),
      was_alternative_service_recently_broken_(
          was_alternative_service_recently_broken),
      server_info_(server_info),
      started_another_job_(false),
      net_log_(net_log),
      session_(nullptr),
      weak_factory_(this) {
}

QuicStreamFactory::Job::Job(QuicStreamFactory* factory,
                            HostResolver* host_resolver,
                            QuicClientSession* session,
                            QuicServerId server_id)
    : io_state_(STATE_RESUME_CONNECT),
      factory_(factory),
      host_resolver_(host_resolver),  // unused
      server_id_(server_id),
      cert_verify_flags_(0),                            // unused
      server_and_origin_have_same_host_(false),         // unused
      is_post_(false),                                  // unused
      was_alternative_service_recently_broken_(false),  // unused
      started_another_job_(false),                      // unused
      net_log_(session->net_log()),                     // unused
      session_(session),
      weak_factory_(this) {
}

QuicStreamFactory::Job::~Job() {
  // If disk cache has a pending WaitForDataReadyCallback, cancel that callback.
  if (server_info_)
    server_info_->ResetWaitForDataReadyCallback();
}

int QuicStreamFactory::Job::Run(const CompletionCallback& callback) {
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv > 0 ? OK : rv;
}

int QuicStreamFactory::Job::DoLoop(int rv) {
  do {
    IoState state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        CHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_LOAD_SERVER_INFO:
        CHECK_EQ(OK, rv);
        rv = DoLoadServerInfo();
        break;
      case STATE_LOAD_SERVER_INFO_COMPLETE:
        rv = DoLoadServerInfoComplete(rv);
        break;
      case STATE_CONNECT:
        CHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case STATE_RESUME_CONNECT:
        CHECK_EQ(OK, rv);
        rv = DoResumeConnect();
        break;
      case STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "io_state_: " << io_state_;
        break;
    }
  } while (io_state_ != STATE_NONE && rv != ERR_IO_PENDING);
  return rv;
}

void QuicStreamFactory::Job::OnIOComplete(int rv) {
  rv = DoLoop(rv);
  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    callback_.Run(rv);
  }
}

void QuicStreamFactory::Job::RunAuxilaryJob() {
  int rv = Run(base::Bind(&QuicStreamFactory::OnJobComplete,
                          base::Unretained(factory_), this));
  if (rv != ERR_IO_PENDING)
    factory_->OnJobComplete(this, rv);
}

void QuicStreamFactory::Job::Cancel() {
  callback_.Reset();
  if (session_)
    session_->connection()->SendConnectionClose(QUIC_CONNECTION_CANCELLED);
}

void QuicStreamFactory::Job::CancelWaitForDataReadyCallback() {
  // If we are waiting for WaitForDataReadyCallback, then cancel the callback.
  if (io_state_ != STATE_LOAD_SERVER_INFO_COMPLETE)
    return;
  server_info_->CancelWaitForDataReadyCallback();
  OnIOComplete(OK);
}

int QuicStreamFactory::Job::DoResolveHost() {
  // Start loading the data now, and wait for it after we resolve the host.
  if (server_info_) {
    server_info_->Start();
  }

  io_state_ = STATE_RESOLVE_HOST_COMPLETE;
  dns_resolution_start_time_ = base::TimeTicks::Now();
  return host_resolver_.Resolve(
      HostResolver::RequestInfo(server_id_.host_port_pair()), DEFAULT_PRIORITY,
      &address_list_,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete, GetWeakPtr()),
      net_log_);
}

int QuicStreamFactory::Job::DoResolveHostComplete(int rv) {
  dns_resolution_end_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Net.QuicSession.HostResolutionTime",
                      dns_resolution_end_time_ - dns_resolution_start_time_);
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(server_id_));

  // Inform the factory of this resolution, which will set up
  // a session alias, if possible.
  if (factory_->OnResolution(server_id_, address_list_)) {
    return OK;
  }

  if (server_info_)
    io_state_ = STATE_LOAD_SERVER_INFO;
  else
    io_state_ = STATE_CONNECT;
  return OK;
}

int QuicStreamFactory::Job::DoLoadServerInfo() {
  io_state_ = STATE_LOAD_SERVER_INFO_COMPLETE;

  DCHECK(server_info_);

  // To mitigate the effects of disk cache taking too long to load QUIC server
  // information, set up a timer to cancel WaitForDataReady's callback.
  if (factory_->load_server_info_timeout_srtt_multiplier_ > 0) {
    int64 load_server_info_timeout_ms =
        (factory_->load_server_info_timeout_srtt_multiplier_ *
         factory_->GetServerNetworkStatsSmoothedRttInMicroseconds(server_id_)) /
        1000;
    if (load_server_info_timeout_ms > 0) {
      factory_->task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&QuicStreamFactory::Job::CancelWaitForDataReadyCallback,
                     GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(load_server_info_timeout_ms));
    }
  }

  int rv = server_info_->WaitForDataReady(
      base::Bind(&QuicStreamFactory::Job::OnIOComplete, GetWeakPtr()));
  if (rv == ERR_IO_PENDING && factory_->enable_connection_racing()) {
    // If we are waiting to load server config from the disk cache, then start
    // another job.
    started_another_job_ = true;
    factory_->CreateAuxilaryJob(server_id_, cert_verify_flags_,
                                server_and_origin_have_same_host_, is_post_,
                                net_log_);
  }
  return rv;
}

int QuicStreamFactory::Job::DoLoadServerInfoComplete(int rv) {
  UMA_HISTOGRAM_TIMES("Net.QuicServerInfo.DiskCacheWaitForDataReadyTime",
                      base::TimeTicks::Now() - dns_resolution_end_time_);

  if (rv != OK)
    server_info_.reset();

  if (started_another_job_ &&
      (!server_info_ || server_info_->state().server_config.empty() ||
       !factory_->CryptoConfigCacheIsEmpty(server_id_))) {
    // If we have started another job and if we didn't load the server config
    // from the disk cache or if we have received a new server config from the
    // server, then cancel the current job.
    io_state_ = STATE_NONE;
    return ERR_CONNECTION_CLOSED;
  }

  io_state_ = STATE_CONNECT;
  return OK;
}

int QuicStreamFactory::Job::DoConnect() {
  io_state_ = STATE_CONNECT_COMPLETE;

  int rv = factory_->CreateSession(
      server_id_, cert_verify_flags_, server_info_.Pass(), address_list_,
      dns_resolution_end_time_, net_log_, &session_);
  if (rv != OK) {
    DCHECK(rv != ERR_IO_PENDING);
    DCHECK(!session_);
    return rv;
  }

  if (!session_->connection()->connected()) {
    return ERR_CONNECTION_CLOSED;
  }

  session_->StartReading();
  if (!session_->connection()->connected()) {
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  bool require_confirmation = factory_->require_confirmation() ||
                              !server_and_origin_have_same_host_ || is_post_ ||
                              was_alternative_service_recently_broken_;

  rv = session_->CryptoConnect(
      require_confirmation,
      base::Bind(&QuicStreamFactory::Job::OnIOComplete, GetWeakPtr()));
  return rv;
}

int QuicStreamFactory::Job::DoResumeConnect() {
  io_state_ = STATE_CONNECT_COMPLETE;

  int rv = session_->ResumeCryptoConnect(
      base::Bind(&QuicStreamFactory::Job::OnIOComplete, GetWeakPtr()));

  return rv;
}

int QuicStreamFactory::Job::DoConnectComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(!factory_->HasActiveSession(server_id_));
  // There may well now be an active session for this IP.  If so, use the
  // existing session instead.
  AddressList address(session_->connection()->peer_address());
  if (factory_->OnResolution(server_id_, address)) {
    session_->connection()->SendConnectionClose(QUIC_CONNECTION_IP_POOLED);
    session_ = nullptr;
    return OK;
  }

  factory_->ActivateSession(server_id_, session_);

  return OK;
}

QuicStreamRequest::QuicStreamRequest(QuicStreamFactory* factory)
    : factory_(factory) {}

QuicStreamRequest::~QuicStreamRequest() {
  if (factory_ && !callback_.is_null())
    factory_->CancelRequest(this);
}

int QuicStreamRequest::Request(const HostPortPair& host_port_pair,
                               bool is_https,
                               PrivacyMode privacy_mode,
                               int cert_verify_flags,
                               base::StringPiece origin_host,
                               base::StringPiece method,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) {
  DCHECK(!stream_);
  DCHECK(callback_.is_null());
  DCHECK(factory_);
  origin_host_ = origin_host.as_string();
  privacy_mode_ = privacy_mode;
  int rv =
      factory_->Create(host_port_pair, is_https, privacy_mode,
                       cert_verify_flags, origin_host, method, net_log, this);
  if (rv == ERR_IO_PENDING) {
    host_port_pair_ = host_port_pair;
    net_log_ = net_log;
    callback_ = callback;
  } else {
    factory_ = nullptr;
  }
  if (rv == OK)
    DCHECK(stream_);
  return rv;
}

void QuicStreamRequest::set_stream(scoped_ptr<QuicHttpStream> stream) {
  DCHECK(stream);
  stream_ = stream.Pass();
}

void QuicStreamRequest::OnRequestComplete(int rv) {
  factory_ = nullptr;
  callback_.Run(rv);
}

scoped_ptr<QuicHttpStream> QuicStreamRequest::ReleaseStream() {
  DCHECK(stream_);
  return stream_.Pass();
}

QuicStreamFactory::QuicStreamFactory(
    HostResolver* host_resolver,
    ClientSocketFactory* client_socket_factory,
    base::WeakPtr<HttpServerProperties> http_server_properties,
    CertVerifier* cert_verifier,
    ChannelIDService* channel_id_service,
    TransportSecurityState* transport_security_state,
    QuicCryptoClientStreamFactory* quic_crypto_client_stream_factory,
    QuicRandom* random_generator,
    QuicClock* clock,
    size_t max_packet_length,
    const std::string& user_agent_id,
    const QuicVersionVector& supported_versions,
    bool enable_port_selection,
    bool always_require_handshake_confirmation,
    bool disable_connection_pooling,
    float load_server_info_timeout_srtt_multiplier,
    bool enable_connection_racing,
    bool enable_non_blocking_io,
    bool disable_disk_cache,
    bool prefer_aes,
    int max_number_of_lossy_connections,
    float packet_loss_threshold,
    int max_disabled_reasons,
    int threshold_public_resets_post_handshake,
    int threshold_timeouts_with_open_streams,
    int socket_receive_buffer_size,
    const QuicTagVector& connection_options)
    : require_confirmation_(true),
      host_resolver_(host_resolver),
      client_socket_factory_(client_socket_factory),
      http_server_properties_(http_server_properties),
      transport_security_state_(transport_security_state),
      quic_server_info_factory_(nullptr),
      quic_crypto_client_stream_factory_(quic_crypto_client_stream_factory),
      random_generator_(random_generator),
      clock_(clock),
      max_packet_length_(max_packet_length),
      config_(InitializeQuicConfig(connection_options)),
      supported_versions_(supported_versions),
      enable_port_selection_(enable_port_selection),
      always_require_handshake_confirmation_(
          always_require_handshake_confirmation),
      disable_connection_pooling_(disable_connection_pooling),
      load_server_info_timeout_srtt_multiplier_(
          load_server_info_timeout_srtt_multiplier),
      enable_connection_racing_(enable_connection_racing),
      enable_non_blocking_io_(enable_non_blocking_io),
      disable_disk_cache_(disable_disk_cache),
      prefer_aes_(prefer_aes),
      max_number_of_lossy_connections_(max_number_of_lossy_connections),
      packet_loss_threshold_(packet_loss_threshold),
      max_disabled_reasons_(max_disabled_reasons),
      num_public_resets_post_handshake_(0),
      num_timeouts_with_open_streams_(0),
      max_public_resets_post_handshake_(0),
      max_timeouts_with_open_streams_(0),
      threshold_timeouts_with_open_streams_(
          threshold_timeouts_with_open_streams),
      threshold_public_resets_post_handshake_(
          threshold_public_resets_post_handshake),
      socket_receive_buffer_size_(socket_receive_buffer_size),
      port_seed_(random_generator_->RandUint64()),
      check_persisted_supports_quic_(true),
      task_runner_(nullptr),
      weak_factory_(this) {
  DCHECK(transport_security_state_);
  crypto_config_.set_user_agent_id(user_agent_id);
  crypto_config_.AddCanonicalSuffix(".c.youtube.com");
  crypto_config_.AddCanonicalSuffix(".googlevideo.com");
  crypto_config_.AddCanonicalSuffix(".googleusercontent.com");
  crypto_config_.SetProofVerifier(
      new ProofVerifierChromium(cert_verifier, transport_security_state));
  // TODO(rtenneti): http://crbug.com/487355. Temporary fix for b/20760730 until
  // channel_id_service is supported in cronet.
  if (channel_id_service) {
    crypto_config_.SetChannelIDSource(
        new ChannelIDSourceChromium(channel_id_service));
  }
#if defined(USE_OPENSSL)
  crypto::EnsureOpenSSLInit();
  bool has_aes_hardware_support = !!EVP_has_aes_hardware();
#else
  base::CPU cpu;
  bool has_aes_hardware_support = cpu.has_aesni() && cpu.has_avx();
#endif
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.PreferAesGcm",
                        has_aes_hardware_support);
  if (has_aes_hardware_support || prefer_aes_)
    crypto_config_.PreferAesGcm();
  if (!IsEcdsaSupported())
    crypto_config_.DisableEcdsa();
}

QuicStreamFactory::~QuicStreamFactory() {
  CloseAllSessions(ERR_ABORTED);
  while (!all_sessions_.empty()) {
    delete all_sessions_.begin()->first;
    all_sessions_.erase(all_sessions_.begin());
  }
  while (!active_jobs_.empty()) {
    const QuicServerId server_id = active_jobs_.begin()->first;
    STLDeleteElements(&(active_jobs_[server_id]));
    active_jobs_.erase(server_id);
  }
}

void QuicStreamFactory::set_require_confirmation(bool require_confirmation) {
  require_confirmation_ = require_confirmation;
  if (http_server_properties_ && (!(local_address_ == IPEndPoint()))) {
    http_server_properties_->SetSupportsQuic(!require_confirmation,
                                             local_address_.address());
  }
}

int QuicStreamFactory::Create(const HostPortPair& host_port_pair,
                              bool is_https,
                              PrivacyMode privacy_mode,
                              int cert_verify_flags,
                              base::StringPiece origin_host,
                              base::StringPiece method,
                              const BoundNetLog& net_log,
                              QuicStreamRequest* request) {
  QuicServerId server_id(host_port_pair, is_https, privacy_mode);
  SessionMap::iterator it = active_sessions_.find(server_id);
  if (it != active_sessions_.end()) {
    QuicClientSession* session = it->second;
    if (!session->CanPool(origin_host.as_string(), privacy_mode))
      return ERR_ALTERNATIVE_CERT_NOT_VALID_FOR_ORIGIN;
    request->set_stream(CreateFromSession(session));
    return OK;
  }

  if (HasActiveJob(server_id)) {
    active_requests_[request] = server_id;
    job_requests_map_[server_id].insert(request);
    return ERR_IO_PENDING;
  }

  // TODO(rtenneti): |task_runner_| is used by the Job. Initialize task_runner_
  // in the constructor after WebRequestActionWithThreadsTest.* tests are fixed.
  if (!task_runner_)
    task_runner_ = base::ThreadTaskRunnerHandle::Get().get();

  QuicServerInfo* quic_server_info = nullptr;
  if (quic_server_info_factory_) {
    bool load_from_disk_cache = !disable_disk_cache_;
    if (http_server_properties_) {
      const AlternativeServiceMap& alternative_service_map =
          http_server_properties_->alternative_service_map();
      AlternativeServiceMap::const_iterator map_it =
          alternative_service_map.Peek(server_id.host_port_pair());
      if (map_it != alternative_service_map.end()) {
        const AlternativeServiceInfoVector& alternative_service_info_vector =
            map_it->second;
        AlternativeServiceInfoVector::const_iterator it;
        for (it = alternative_service_info_vector.begin();
             it != alternative_service_info_vector.end(); ++it) {
          if (it->alternative_service.protocol == QUIC)
            break;
        }
        // If there is no entry for QUIC, consider that as a new server and
        // don't wait for Cache thread to load the data for that server.
        if (it == alternative_service_info_vector.end())
          load_from_disk_cache = false;
      }
    }
    if (load_from_disk_cache && CryptoConfigCacheIsEmpty(server_id)) {
      quic_server_info = quic_server_info_factory_->GetForServer(server_id);
    }
  }

  bool server_and_origin_have_same_host = host_port_pair.host() == origin_host;
  scoped_ptr<Job> job(new Job(this, host_resolver_, host_port_pair,
                              server_and_origin_have_same_host, is_https,
                              WasQuicRecentlyBroken(server_id), privacy_mode,
                              cert_verify_flags, method == "POST" /* is_post */,
                              quic_server_info, net_log));
  int rv = job->Run(base::Bind(&QuicStreamFactory::OnJobComplete,
                               base::Unretained(this), job.get()));
  if (rv == ERR_IO_PENDING) {
    active_requests_[request] = server_id;
    job_requests_map_[server_id].insert(request);
    active_jobs_[server_id].insert(job.release());
    return rv;
  }
  if (rv == OK) {
    it = active_sessions_.find(server_id);
    DCHECK(it != active_sessions_.end());
    QuicClientSession* session = it->second;
    if (!session->CanPool(origin_host.as_string(), privacy_mode))
      return ERR_ALTERNATIVE_CERT_NOT_VALID_FOR_ORIGIN;
    request->set_stream(CreateFromSession(session));
  }
  return rv;
}

void QuicStreamFactory::CreateAuxilaryJob(const QuicServerId server_id,
                                          int cert_verify_flags,
                                          bool server_and_origin_have_same_host,
                                          bool is_post,
                                          const BoundNetLog& net_log) {
  Job* aux_job =
      new Job(this, host_resolver_, server_id.host_port_pair(),
              server_and_origin_have_same_host, server_id.is_https(),
              WasQuicRecentlyBroken(server_id), server_id.privacy_mode(),
              cert_verify_flags, is_post, nullptr, net_log);
  active_jobs_[server_id].insert(aux_job);
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&QuicStreamFactory::Job::RunAuxilaryJob,
                                    aux_job->GetWeakPtr()));
}

bool QuicStreamFactory::OnResolution(
    const QuicServerId& server_id,
    const AddressList& address_list) {
  DCHECK(!HasActiveSession(server_id));
  if (disable_connection_pooling_) {
    return false;
  }
  for (const IPEndPoint& address : address_list) {
    const IpAliasKey ip_alias_key(address, server_id.is_https());
    if (!ContainsKey(ip_aliases_, ip_alias_key))
      continue;

    const SessionSet& sessions = ip_aliases_[ip_alias_key];
    for (QuicClientSession* session : sessions) {
      if (!session->CanPool(server_id.host(), server_id.privacy_mode()))
        continue;
      active_sessions_[server_id] = session;
      session_aliases_[session].insert(server_id);
      return true;
    }
  }
  return false;
}

void QuicStreamFactory::OnJobComplete(Job* job, int rv) {
  QuicServerId server_id = job->server_id();
  if (rv != OK) {
    JobSet* jobs = &(active_jobs_[server_id]);
    if (jobs->size() > 1) {
      // If there is another pending job, then we can delete this job and let
      // the other job handle the request.
      job->Cancel();
      jobs->erase(job);
      delete job;
      return;
    }
  }

  if (rv == OK) {
    if (!always_require_handshake_confirmation_)
      set_require_confirmation(false);

    // Create all the streams, but do not notify them yet.
    SessionMap::iterator session_it = active_sessions_.find(server_id);
    for (RequestSet::iterator request_it = job_requests_map_[server_id].begin();
         request_it != job_requests_map_[server_id].end();) {
      DCHECK(session_it != active_sessions_.end());
      QuicClientSession* session = session_it->second;
      QuicStreamRequest* request = *request_it;
      if (!session->CanPool(request->origin_host(), request->privacy_mode())) {
        RequestSet::iterator old_request_it = request_it;
        ++request_it;
        // Remove request from containers so that OnRequestComplete() is not
        // called later again on the same request.
        job_requests_map_[server_id].erase(old_request_it);
        active_requests_.erase(request);
        // Notify request of certificate error.
        request->OnRequestComplete(ERR_ALTERNATIVE_CERT_NOT_VALID_FOR_ORIGIN);
        continue;
      }
      request->set_stream(CreateFromSession(session));
      ++request_it;
    }
  }

  while (!job_requests_map_[server_id].empty()) {
    RequestSet::iterator it = job_requests_map_[server_id].begin();
    QuicStreamRequest* request = *it;
    job_requests_map_[server_id].erase(it);
    active_requests_.erase(request);
    // Even though we're invoking callbacks here, we don't need to worry
    // about |this| being deleted, because the factory is owned by the
    // profile which can not be deleted via callbacks.
    request->OnRequestComplete(rv);
  }

  for (Job* other_job : active_jobs_[server_id]) {
    if (other_job != job)
      other_job->Cancel();
  }

  STLDeleteElements(&(active_jobs_[server_id]));
  active_jobs_.erase(server_id);
  job_requests_map_.erase(server_id);
}

scoped_ptr<QuicHttpStream> QuicStreamFactory::CreateFromSession(
    QuicClientSession* session) {
  return scoped_ptr<QuicHttpStream>(new QuicHttpStream(session->GetWeakPtr()));
}

QuicClientSession::QuicDisabledReason QuicStreamFactory::QuicDisabledReason(
    uint16 port) const {
  if (max_number_of_lossy_connections_ > 0 &&
      number_of_lossy_connections_.find(port) !=
          number_of_lossy_connections_.end() &&
      number_of_lossy_connections_.at(port) >=
          max_number_of_lossy_connections_) {
    return QuicClientSession::QUIC_DISABLED_BAD_PACKET_LOSS_RATE;
  }
  if (threshold_public_resets_post_handshake_ > 0 &&
      num_public_resets_post_handshake_ >=
          threshold_public_resets_post_handshake_) {
    return QuicClientSession::QUIC_DISABLED_PUBLIC_RESET_POST_HANDSHAKE;
  }
  if (threshold_timeouts_with_open_streams_ > 0 &&
      num_timeouts_with_open_streams_ >=
          threshold_timeouts_with_open_streams_) {
    return QuicClientSession::QUIC_DISABLED_TIMEOUT_WITH_OPEN_STREAMS;
  }
  return QuicClientSession::QUIC_DISABLED_NOT;
}

const char* QuicStreamFactory::QuicDisabledReasonString() const {
  // TODO(ckrasic) - better solution for port/lossy connections?
  const uint16 port = 443;
  switch (QuicDisabledReason(port)) {
    case QuicClientSession::QUIC_DISABLED_BAD_PACKET_LOSS_RATE:
      return "Bad packet loss rate.";
    case QuicClientSession::QUIC_DISABLED_PUBLIC_RESET_POST_HANDSHAKE:
      return "Public resets after successful handshakes.";
    case QuicClientSession::QUIC_DISABLED_TIMEOUT_WITH_OPEN_STREAMS:
      return "Connection timeouts with streams open.";
    default:
      return "";
  }
}

bool QuicStreamFactory::IsQuicDisabled(uint16 port) {
  return QuicDisabledReason(port) != QuicClientSession::QUIC_DISABLED_NOT;
}

bool QuicStreamFactory::OnHandshakeConfirmed(QuicClientSession* session,
                                             float packet_loss_rate) {
  DCHECK(session);
  uint16 port = session->server_id().port();
  if (packet_loss_rate < packet_loss_threshold_) {
    number_of_lossy_connections_[port] = 0;
    return false;
  }

  if (http_server_properties_) {
    // We mark it as recently broken, which means that 0-RTT will be disabled
    // but we'll still race.
    http_server_properties_->MarkAlternativeServiceRecentlyBroken(
        AlternativeService(QUIC, session->server_id().host(), port));
  }

  bool was_quic_disabled = IsQuicDisabled(port);
  ++number_of_lossy_connections_[port];

  // Collect data for port 443 for packet loss events.
  if (port == 443 && max_number_of_lossy_connections_ > 0) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        base::StringPrintf("Net.QuicStreamFactory.BadPacketLossEvents%d",
                           max_number_of_lossy_connections_),
        std::min(number_of_lossy_connections_[port],
                 max_number_of_lossy_connections_));
  }

  bool is_quic_disabled = IsQuicDisabled(port);
  if (is_quic_disabled) {
    // Close QUIC connection if Quic is disabled for this port.
    session->CloseSessionOnErrorAndNotifyFactoryLater(
        ERR_ABORTED, QUIC_BAD_PACKET_LOSS_RATE);

    // If this bad packet loss rate disabled the QUIC, then record it.
    if (!was_quic_disabled)
      UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicStreamFactory.QuicIsDisabled", port);
  }
  return is_quic_disabled;
}

void QuicStreamFactory::OnIdleSession(QuicClientSession* session) {
}

void QuicStreamFactory::OnSessionGoingAway(QuicClientSession* session) {
  const AliasSet& aliases = session_aliases_[session];
  for (AliasSet::const_iterator it = aliases.begin(); it != aliases.end();
       ++it) {
    DCHECK(active_sessions_.count(*it));
    DCHECK_EQ(session, active_sessions_[*it]);
    // Track sessions which have recently gone away so that we can disable
    // port suggestions.
    if (session->goaway_received()) {
      gone_away_aliases_.insert(*it);
    }

    active_sessions_.erase(*it);
    ProcessGoingAwaySession(session, *it, true);
  }
  ProcessGoingAwaySession(session, all_sessions_[session], false);
  if (!aliases.empty()) {
    const IpAliasKey ip_alias_key(session->connection()->peer_address(),
                                  aliases.begin()->is_https());
    ip_aliases_[ip_alias_key].erase(session);
    if (ip_aliases_[ip_alias_key].empty()) {
      ip_aliases_.erase(ip_alias_key);
    }
  }
  session_aliases_.erase(session);
}

void QuicStreamFactory::MaybeDisableQuic(QuicClientSession* session) {
  DCHECK(session);
  uint16 port = session->server_id().port();
  if (IsQuicDisabled(port))
    return;

  // Expire the oldest disabled_reason if appropriate.  This enforces that we
  // only consider the max_disabled_reasons_ most recent sessions.
  QuicClientSession::QuicDisabledReason disabled_reason;
  if (static_cast<int>(disabled_reasons_.size()) == max_disabled_reasons_) {
    disabled_reason = disabled_reasons_.front();
    disabled_reasons_.pop_front();
    if (disabled_reason ==
        QuicClientSession::QUIC_DISABLED_PUBLIC_RESET_POST_HANDSHAKE) {
      --num_public_resets_post_handshake_;
    } else if (disabled_reason ==
               QuicClientSession::QUIC_DISABLED_TIMEOUT_WITH_OPEN_STREAMS) {
      --num_timeouts_with_open_streams_;
    }
  }
  disabled_reason = session->disabled_reason();
  disabled_reasons_.push_back(disabled_reason);
  if (disabled_reason ==
      QuicClientSession::QUIC_DISABLED_PUBLIC_RESET_POST_HANDSHAKE) {
    ++num_public_resets_post_handshake_;
  } else if (disabled_reason ==
             QuicClientSession::QUIC_DISABLED_TIMEOUT_WITH_OPEN_STREAMS) {
    ++num_timeouts_with_open_streams_;
  }
  if (num_timeouts_with_open_streams_ > max_timeouts_with_open_streams_) {
    max_timeouts_with_open_streams_ = num_timeouts_with_open_streams_;
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicStreamFactory.TimeoutsWithOpenStreams",
                                num_timeouts_with_open_streams_, 0, 20, 10);
  }

  if (num_public_resets_post_handshake_ > max_public_resets_post_handshake_) {
    max_public_resets_post_handshake_ = num_public_resets_post_handshake_;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.QuicStreamFactory.PublicResetsPostHandshake",
        num_public_resets_post_handshake_, 0, 20, 10);
  }

  if (IsQuicDisabled(port)) {
    if (disabled_reason ==
        QuicClientSession::QUIC_DISABLED_PUBLIC_RESET_POST_HANDSHAKE) {
      session->CloseSessionOnErrorAndNotifyFactoryLater(
          ERR_ABORTED, QUIC_PUBLIC_RESETS_POST_HANDSHAKE);
    } else if (disabled_reason ==
               QuicClientSession::QUIC_DISABLED_TIMEOUT_WITH_OPEN_STREAMS) {
      session->CloseSessionOnErrorAndNotifyFactoryLater(
          ERR_ABORTED, QUIC_TIMEOUTS_WITH_OPEN_STREAMS);
    }
    UMA_HISTOGRAM_ENUMERATION("Net.QuicStreamFactory.DisabledReasons",
                              disabled_reason,
                              QuicClientSession::QUIC_DISABLED_MAX);
  }
}

void QuicStreamFactory::OnSessionClosed(QuicClientSession* session) {
  DCHECK_EQ(0u, session->GetNumOpenStreams());
  MaybeDisableQuic(session);
  OnSessionGoingAway(session);
  delete session;
  all_sessions_.erase(session);
}

void QuicStreamFactory::OnSessionConnectTimeout(
    QuicClientSession* session) {
  const AliasSet& aliases = session_aliases_[session];
  for (AliasSet::const_iterator it = aliases.begin(); it != aliases.end();
       ++it) {
    DCHECK(active_sessions_.count(*it));
    DCHECK_EQ(session, active_sessions_[*it]);
    active_sessions_.erase(*it);
  }

  if (aliases.empty()) {
    return;
  }

  const IpAliasKey ip_alias_key(session->connection()->peer_address(),
                                aliases.begin()->is_https());
  ip_aliases_[ip_alias_key].erase(session);
  if (ip_aliases_[ip_alias_key].empty()) {
    ip_aliases_.erase(ip_alias_key);
  }
  QuicServerId server_id = *aliases.begin();
  session_aliases_.erase(session);
  Job* job = new Job(this, host_resolver_, session, server_id);
  active_jobs_[server_id].insert(job);
  int rv = job->Run(base::Bind(&QuicStreamFactory::OnJobComplete,
                               base::Unretained(this), job));
  DCHECK_EQ(ERR_IO_PENDING, rv);
}

void QuicStreamFactory::CancelRequest(QuicStreamRequest* request) {
  DCHECK(ContainsKey(active_requests_, request));
  QuicServerId server_id = active_requests_[request];
  job_requests_map_[server_id].erase(request);
  active_requests_.erase(request);
}

void QuicStreamFactory::CloseAllSessions(int error) {
  while (!active_sessions_.empty()) {
    size_t initial_size = active_sessions_.size();
    active_sessions_.begin()->second->CloseSessionOnError(error,
                                                          QUIC_INTERNAL_ERROR);
    DCHECK_NE(initial_size, active_sessions_.size());
  }
  while (!all_sessions_.empty()) {
    size_t initial_size = all_sessions_.size();
    all_sessions_.begin()->first->CloseSessionOnError(error,
                                                      QUIC_INTERNAL_ERROR);
    DCHECK_NE(initial_size, all_sessions_.size());
  }
  DCHECK(all_sessions_.empty());
}

scoped_ptr<base::Value> QuicStreamFactory::QuicStreamFactoryInfoToValue()
    const {
  scoped_ptr<base::ListValue> list(new base::ListValue());

  for (SessionMap::const_iterator it = active_sessions_.begin();
       it != active_sessions_.end(); ++it) {
    const QuicServerId& server_id = it->first;
    QuicClientSession* session = it->second;
    const AliasSet& aliases = session_aliases_.find(session)->second;
    // Only add a session to the list once.
    if (server_id == *aliases.begin()) {
      std::set<HostPortPair> hosts;
      for (AliasSet::const_iterator alias_it = aliases.begin();
           alias_it != aliases.end(); ++alias_it) {
        hosts.insert(alias_it->host_port_pair());
      }
      list->Append(session->GetInfoAsValue(hosts));
    }
  }
  return list.Pass();
}

void QuicStreamFactory::ClearCachedStatesInCryptoConfig() {
  crypto_config_.ClearCachedStates();
}

void QuicStreamFactory::OnIPAddressChanged() {
  CloseAllSessions(ERR_NETWORK_CHANGED);
  set_require_confirmation(true);
}

void QuicStreamFactory::OnCertAdded(const X509Certificate* cert) {
  CloseAllSessions(ERR_CERT_DATABASE_CHANGED);
}

void QuicStreamFactory::OnCACertChanged(const X509Certificate* cert) {
  // We should flush the sessions if we removed trust from a
  // cert, because a previously trusted server may have become
  // untrusted.
  //
  // We should not flush the sessions if we added trust to a cert.
  //
  // Since the OnCACertChanged method doesn't tell us what
  // kind of change it is, we have to flush the socket
  // pools to be safe.
  CloseAllSessions(ERR_CERT_DATABASE_CHANGED);
}

bool QuicStreamFactory::HasActiveSession(
    const QuicServerId& server_id) const {
  return ContainsKey(active_sessions_, server_id);
}

bool QuicStreamFactory::HasActiveJob(const QuicServerId& key) const {
  return ContainsKey(active_jobs_, key);
}

int QuicStreamFactory::CreateSession(const QuicServerId& server_id,
                                     int cert_verify_flags,
                                     scoped_ptr<QuicServerInfo> server_info,
                                     const AddressList& address_list,
                                     base::TimeTicks dns_resolution_end_time,
                                     const BoundNetLog& net_log,
                                     QuicClientSession** session) {
  bool enable_port_selection = enable_port_selection_;
  if (enable_port_selection &&
      ContainsKey(gone_away_aliases_, server_id)) {
    // Disable port selection when the server is going away.
    // There is no point in trying to return to the same server, if
    // that server is no longer handling requests.
    enable_port_selection = false;
    gone_away_aliases_.erase(server_id);
  }

  QuicConnectionId connection_id = random_generator_->RandUint64();
  IPEndPoint addr = *address_list.begin();
  scoped_refptr<PortSuggester> port_suggester =
      new PortSuggester(server_id.host_port_pair(), port_seed_);
  DatagramSocket::BindType bind_type = enable_port_selection ?
      DatagramSocket::RANDOM_BIND :  // Use our callback.
      DatagramSocket::DEFAULT_BIND;  // Use OS to randomize.
  scoped_ptr<DatagramClientSocket> socket(
      client_socket_factory_->CreateDatagramClientSocket(
          bind_type,
          base::Bind(&PortSuggester::SuggestPort, port_suggester),
          net_log.net_log(), net_log.source()));

  if (enable_non_blocking_io_ &&
      client_socket_factory_ == ClientSocketFactory::GetDefaultFactory()) {
#if defined(OS_WIN)
    static_cast<UDPClientSocket*>(socket.get())->UseNonBlockingIO();
#endif
  }

  int rv = socket->Connect(addr);

  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_CONNECTING_SOCKET);
    return rv;
  }
  UMA_HISTOGRAM_COUNTS("Net.QuicEphemeralPortsSuggested",
                       port_suggester->call_count());
  if (enable_port_selection) {
    DCHECK_LE(1u, port_suggester->call_count());
  } else {
    DCHECK_EQ(0u, port_suggester->call_count());
  }

  rv = socket->SetReceiveBufferSize(socket_receive_buffer_size_);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_RECEIVE_BUFFER);
    return rv;
  }
  // Set a buffer large enough to contain the initial CWND's worth of packet
  // to work around the problem with CHLO packets being sent out with the
  // wrong encryption level, when the send buffer is full.
  rv = socket->SetSendBufferSize(kMaxPacketSize * 20);
  if (rv != OK) {
    HistogramCreateSessionFailure(CREATION_ERROR_SETTING_SEND_BUFFER);
    return rv;
  }

  socket->GetLocalAddress(&local_address_);
  if (check_persisted_supports_quic_ && http_server_properties_) {
    check_persisted_supports_quic_ = false;
    IPAddressNumber last_address;
    if (http_server_properties_->GetSupportsQuic(&last_address) &&
        last_address == local_address_.address()) {
      require_confirmation_ = false;
    }
  }

  DefaultPacketWriterFactory packet_writer_factory(socket.get());

  if (!helper_.get()) {
    helper_.reset(
        new QuicConnectionHelper(base::ThreadTaskRunnerHandle::Get().get(),
                                 clock_.get(), random_generator_));
  }

  QuicConnection* connection = new QuicConnection(
      connection_id, addr, helper_.get(), packet_writer_factory,
      true /* owns_writer */, Perspective::IS_CLIENT, server_id.is_https(),
      supported_versions_);
  connection->set_max_packet_length(max_packet_length_);

  InitializeCachedStateInCryptoConfig(server_id, server_info);

  QuicConfig config = config_;
  config.SetSocketReceiveBufferToSend(socket_receive_buffer_size_);
  config.set_max_undecryptable_packets(kMaxUndecryptablePackets);
  config.SetInitialSessionFlowControlWindowToSend(
      kQuicSessionMaxRecvWindowSize);
  config.SetInitialStreamFlowControlWindowToSend(kQuicStreamMaxRecvWindowSize);
  int64 srtt = GetServerNetworkStatsSmoothedRttInMicroseconds(server_id);
  if (srtt > 0)
    config.SetInitialRoundTripTimeUsToSend(static_cast<uint32>(srtt));
  config.SetBytesForConnectionIdToSend(0);

  if (quic_server_info_factory_ && !server_info) {
    // Start the disk cache loading so that we can persist the newer QUIC server
    // information and/or inform the disk cache that we have reused
    // |server_info|.
    server_info.reset(quic_server_info_factory_->GetForServer(server_id));
    server_info->Start();
  }

  *session = new QuicClientSession(
      connection, socket.Pass(), this, quic_crypto_client_stream_factory_,
      transport_security_state_, server_info.Pass(), server_id,
      cert_verify_flags, config, &crypto_config_,
      network_connection_.GetDescription(), dns_resolution_end_time,
      base::ThreadTaskRunnerHandle::Get().get(), net_log.net_log());

  all_sessions_[*session] = server_id;  // owning pointer

  (*session)->Initialize();
  bool closed_during_initialize =
      !ContainsKey(all_sessions_, *session) ||
      !(*session)->connection()->connected();
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ClosedDuringInitializeSession",
                        closed_during_initialize);
  if (closed_during_initialize) {
    DLOG(DFATAL) << "Session closed during initialize";
    *session = nullptr;
    return ERR_CONNECTION_CLOSED;
  }
  return OK;
}

void QuicStreamFactory::ActivateSession(
    const QuicServerId& server_id,
    QuicClientSession* session) {
  DCHECK(!HasActiveSession(server_id));
  UMA_HISTOGRAM_COUNTS("Net.QuicActiveSessions", active_sessions_.size());
  active_sessions_[server_id] = session;
  session_aliases_[session].insert(server_id);
  const IpAliasKey ip_alias_key(session->connection()->peer_address(),
                                server_id.is_https());
  DCHECK(!ContainsKey(ip_aliases_[ip_alias_key], session));
  ip_aliases_[ip_alias_key].insert(session);
}

int64 QuicStreamFactory::GetServerNetworkStatsSmoothedRttInMicroseconds(
    const QuicServerId& server_id) const {
  if (!http_server_properties_)
    return 0;
  const ServerNetworkStats* stats =
      http_server_properties_->GetServerNetworkStats(
          server_id.host_port_pair());
  if (stats == nullptr)
    return 0;
  return stats->srtt.InMicroseconds();
}

bool QuicStreamFactory::WasQuicRecentlyBroken(
    const QuicServerId& server_id) const {
  if (!http_server_properties_)
    return false;
  const AlternativeService alternative_service(QUIC,
                                               server_id.host_port_pair());
  return http_server_properties_->WasAlternativeServiceRecentlyBroken(
      alternative_service);
}

bool QuicStreamFactory::CryptoConfigCacheIsEmpty(
    const QuicServerId& server_id) {
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_.LookupOrCreate(server_id);
  return cached->IsEmpty();
}

void QuicStreamFactory::InitializeCachedStateInCryptoConfig(
    const QuicServerId& server_id,
    const scoped_ptr<QuicServerInfo>& server_info) {
  // |server_info| will be NULL, if a non-empty server config already exists in
  // the memory cache. This is a minor optimization to avoid LookupOrCreate.
  if (!server_info)
    return;

  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_.LookupOrCreate(server_id);
  if (!cached->IsEmpty())
    return;

  if (http_server_properties_) {
    if (quic_supported_servers_at_startup_.empty()) {
      for (const std::pair<const HostPortPair, AlternativeServiceInfoVector>&
               key_value : http_server_properties_->alternative_service_map()) {
        for (const AlternativeServiceInfo& alternative_service_info :
             key_value.second) {
          if (alternative_service_info.alternative_service.protocol == QUIC) {
            quic_supported_servers_at_startup_.insert(key_value.first);
            break;
          }
        }
      }
    }

    // TODO(rtenneti): Delete the following histogram after collecting stats.
    // If the AlternativeServiceMap contained an entry for this host, check if
    // the disk cache contained an entry for it.
    if (ContainsKey(quic_supported_servers_at_startup_,
                    server_id.host_port_pair())) {
      UMA_HISTOGRAM_BOOLEAN(
          "Net.QuicServerInfo.ExpectConfigMissingFromDiskCache",
          server_info->state().server_config.empty());
    }
  }

  if (!cached->Initialize(server_info->state().server_config,
                          server_info->state().source_address_token,
                          server_info->state().certs,
                          server_info->state().server_config_sig,
                          clock_->WallNow()))
    return;

  if (!server_id.is_https()) {
    // Don't check the certificates for insecure QUIC.
    cached->SetProofValid();
  }
}

void QuicStreamFactory::ProcessGoingAwaySession(
    QuicClientSession* session,
    const QuicServerId& server_id,
    bool session_was_active) {
  if (!http_server_properties_)
    return;

  const QuicConnectionStats& stats = session->connection()->GetStats();
  const AlternativeService alternative_service(QUIC,
                                               server_id.host_port_pair());
  if (session->IsCryptoHandshakeConfirmed()) {
    http_server_properties_->ConfirmAlternativeService(alternative_service);
    ServerNetworkStats network_stats;
    network_stats.srtt = base::TimeDelta::FromMicroseconds(stats.srtt_us);
    network_stats.bandwidth_estimate = stats.estimated_bandwidth;
    http_server_properties_->SetServerNetworkStats(server_id.host_port_pair(),
                                                   network_stats);
    return;
  }

  UMA_HISTOGRAM_COUNTS("Net.QuicHandshakeNotConfirmedNumPacketsReceived",
                       stats.packets_received);

  if (!session_was_active)
    return;

  // TODO(rch):  In the special case where the session has received no
  // packets from the peer, we should consider blacklisting this
  // differently so that we still race TCP but we don't consider the
  // session connected until the handshake has been confirmed.
  HistogramBrokenAlternateProtocolLocation(
      BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_STREAM_FACTORY);

  // Since the session was active, there's no longer an
  // HttpStreamFactoryImpl::Job running which can mark it broken, unless the TCP
  // job also fails. So to avoid not using QUIC when we otherwise could, we mark
  // it as recently broken, which means that 0-RTT will be disabled but we'll
  // still race.
  http_server_properties_->MarkAlternativeServiceRecentlyBroken(
      alternative_service);
}

}  // namespace net
