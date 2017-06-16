// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_IMPL_JOB_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_IMPL_JOB_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory_impl.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/quic_stream_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class ClientSocketHandle;
class HttpAuthController;
class HttpNetworkSession;
class HttpStream;
class SpdySessionPool;
class QuicHttpStream;

// An HttpStreamRequestImpl exists for each stream which is in progress of being
// created for the StreamFactory.
class HttpStreamFactoryImpl::Job {
 public:
  // Constructor for non-alternative Job.
  Job(HttpStreamFactoryImpl* stream_factory,
      HttpNetworkSession* session,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      NetLog* net_log);
  // Constructor for alternative Job.
  Job(HttpStreamFactoryImpl* stream_factory,
      HttpNetworkSession* session,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      AlternativeService alternative_service,
      NetLog* net_log);
  ~Job();

  // Start initiates the process of creating a new HttpStream. |request| will be
  // notified upon completion if the Job has not been Orphan()'d.
  void Start(Request* request);

  // Preconnect will attempt to request |num_streams| sockets from the
  // appropriate ClientSocketPool.
  int Preconnect(int num_streams);

  int RestartTunnelWithProxyAuth(const AuthCredentials& credentials);
  LoadState GetLoadState() const;

  // Tells |this| to wait for |job| to resume it.
  void WaitFor(Job* job);

  // Tells |this| that |job| has determined it still needs to continue
  // connecting, so allow |this| to continue. If this is not called, then
  // |request_| is expected to cancel |this| by deleting it.
  void Resume(Job* job);

  // Used to detach the Job from |request|.
  void Orphan(const Request* request);

  void SetPriority(RequestPriority priority);

  RequestPriority priority() const { return priority_; }
  bool was_npn_negotiated() const;
  NextProto protocol_negotiated() const;
  bool using_spdy() const;
  const BoundNetLog& net_log() const { return net_log_; }

  const SSLConfig& server_ssl_config() const;
  const SSLConfig& proxy_ssl_config() const;
  const ProxyInfo& proxy_info() const;

  // Indicates whether or not this job is performing a preconnect.
  bool IsPreconnecting() const;

  // Indicates whether or not this Job has been orphaned by a Request.
  bool IsOrphaned() const;

  // Called to indicate that this job succeeded, and some other jobs
  // will be orphaned.
  void ReportJobSucceededForRequest();

  // Marks that the other |job| has completed.
  void MarkOtherJobComplete(const Job& job);

 private:
  enum State {
    STATE_START,
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,

    // Note that when Alternate-Protocol says we can connect to an alternate
    // port using a different protocol, we have the choice of communicating over
    // the original protocol, or speaking the alternate protocol (currently,
    // only npn-spdy) over an alternate port. For a cold page load, the http
    // connection that delivers the http response that has the
    // Alternate-Protocol header will already be warm. So, blocking the next
    // http request on establishing a new npn-spdy connection would incur extra
    // latency. Even if the http connection was not reused, establishing a new
    // http connection is typically faster than npn-spdy, since npn-spdy
    // requires a SSL handshake. Therefore, we start both the http and the
    // npn-spdy jobs in parallel. In order not to unnecessarily waste sockets,
    // we have the http job block on the npn-spdy job after proxy resolution.
    // The npn-spdy job will Resume() the http job if, in
    // STATE_INIT_CONNECTION_COMPLETE, it detects an error or does not find an
    // existing SpdySession. In that case, the http and npn-spdy jobs will race.
    STATE_WAIT_FOR_JOB,
    STATE_WAIT_FOR_JOB_COMPLETE,

    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_WAITING_USER_ACTION,
    STATE_RESTART_TUNNEL_AUTH,
    STATE_RESTART_TUNNEL_AUTH_COMPLETE,
    STATE_CREATE_STREAM,
    STATE_CREATE_STREAM_COMPLETE,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE,
    STATE_DONE,
    STATE_NONE
  };

  enum JobStatus {
    STATUS_RUNNING,
    STATUS_FAILED,
    STATUS_BROKEN,
    STATUS_SUCCEEDED
  };

  // Wrapper class for SpdySessionPool methods to enforce certificate
  // requirements for SpdySessions.
  class ValidSpdySessionPool {
   public:
    ValidSpdySessionPool(SpdySessionPool* spdy_session_pool,
                         GURL& origin_url,
                         bool is_spdy_alternative);

    // Returns OK if a SpdySession was not found (in which case |spdy_session|
    // is set to nullptr), or if one was found (in which case |spdy_session| is
    // set to it) and it has an associated SSL certificate with is valid for
    // |origin_url_|, or if this requirement does not apply because the Job is
    // not a SPDY alternative job.  Returns the appropriate error code
    // otherwise,
    // in which case |spdy_session| should not be used.
    int FindAvailableSession(const SpdySessionKey& key,
                             const BoundNetLog& net_log,
                             base::WeakPtr<SpdySession>* spdy_session);

    // Creates a SpdySession and sets |spdy_session| to point to it.  Returns OK
    // if the associated SSL certificate is valid for |origin_url_|, or if this
    // requirement does not apply because the Job is not a SPDY alternative job.
    // Returns the appropriate error code otherwise, in which case
    // |spdy_session| should not be used.
    int CreateAvailableSessionFromSocket(
        const SpdySessionKey& key,
        scoped_ptr<ClientSocketHandle> connection,
        const BoundNetLog& net_log,
        int certificate_error_code,
        bool is_secure,
        base::WeakPtr<SpdySession>* spdy_session);

   private:
    // Returns OK if |spdy_session| has an associated SSL certificate with is
    // valid for |origin_url_|, or if this requirement does not apply because
    // the Job is not a SPDY alternative job, or if |spdy_session| is null.
    // Returns appropriate error code otherwise.
    int CheckAlternativeServiceValidityForOrigin(
        base::WeakPtr<SpdySession> spdy_session);

    SpdySessionPool* const spdy_session_pool_;
    const GURL origin_url_;
    const bool is_spdy_alternative_;
  };

  void OnStreamReadyCallback();
  void OnWebSocketHandshakeStreamReadyCallback();
  // This callback function is called when a new SPDY session is created.
  void OnNewSpdySessionReadyCallback();
  void OnStreamFailedCallback(int result);
  void OnCertificateErrorCallback(int result, const SSLInfo& ssl_info);
  void OnNeedsProxyAuthCallback(const HttpResponseInfo& response_info,
                                HttpAuthController* auth_controller);
  void OnNeedsClientAuthCallback(SSLCertRequestInfo* cert_info);
  void OnHttpsProxyTunnelResponseCallback(const HttpResponseInfo& response_info,
                                          HttpStream* stream);
  void OnPreconnectsComplete();

  void OnIOComplete(int result);
  int RunLoop(int result);
  int DoLoop(int result);
  int StartInternal();

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoStart();
  int DoResolveProxy();
  int DoResolveProxyComplete(int result);
  int DoWaitForJob();
  int DoWaitForJobComplete(int result);
  int DoInitConnection();
  int DoInitConnectionComplete(int result);
  int DoWaitingUserAction(int result);
  int DoCreateStream();
  int DoCreateStreamComplete(int result);
  int DoRestartTunnelAuth();
  int DoRestartTunnelAuthComplete(int result);

  // Creates a SpdyHttpStream from the given values and sets to |stream_|. Does
  // nothing if |stream_factory_| is for WebSockets.
  int SetSpdyHttpStream(base::WeakPtr<SpdySession> session, bool direct);

  // Returns to STATE_INIT_CONNECTION and resets some state.
  void ReturnToStateInitConnection(bool close_connection);

  // Set the motivation for this request onto the underlying socket.
  void SetSocketMotivation();

  bool IsHttpsProxyAndHttpUrl() const;

  // Is this a SPDY or QUIC alternative Job?
  bool IsSpdyAlternative() const;
  bool IsQuicAlternative() const;

  // Sets several fields of |ssl_config| for |server| based on the proxy info
  // and other factors.
  void InitSSLConfig(const HostPortPair& server,
                     SSLConfig* ssl_config,
                     bool is_proxy) const;

  // Retrieve SSLInfo from our SSL Socket.
  // This must only be called when we are using an SSLSocket.
  // After calling, the caller can use ssl_info_.
  void GetSSLInfo();

  SpdySessionKey GetSpdySessionKey() const;

  // Returns true if the current request can use an existing spdy session.
  bool CanUseExistingSpdySession() const;

  // Called when we encounter a network error that could be resolved by trying
  // a new proxy configuration.  If there is another proxy configuration to try
  // then this method sets next_state_ appropriately and returns either OK or
  // ERR_IO_PENDING depending on whether or not the new proxy configuration is
  // available synchronously or asynchronously.  Otherwise, the given error
  // code is simply returned.
  int ReconsiderProxyAfterError(int error);

  // Called to handle a certificate error.  Stores the certificate in the
  // allowed_bad_certs list, and checks if the error can be ignored.  Returns
  // OK if it can be ignored, or the error code otherwise.
  int HandleCertificateError(int error);

  // Called to handle a client certificate request.
  int HandleCertificateRequest(int error);

  // Moves this stream request into SPDY mode.
  void SwitchToSpdyMode();

  // Should we force QUIC for this stream request.
  bool ShouldForceQuic() const;

  void MaybeMarkAlternativeServiceBroken();

  ClientSocketPoolManager::SocketGroupType GetSocketGroup() const;

  void MaybeCopyConnectionAttemptsFromSocketOrHandle();

  // Record histograms of latency until Connect() completes.
  static void LogHttpConnectedMetrics(const ClientSocketHandle& handle);

  // Invoked by the transport socket pool after host resolution is complete
  // to allow the connection to be aborted, if a matching SPDY session can
  // be found.  Will return ERR_SPDY_SESSION_ALREADY_EXISTS if such a
  // session is found, and OK otherwise.
  static int OnHostResolution(SpdySessionPool* spdy_session_pool,
                              const SpdySessionKey& spdy_session_key,
                              const AddressList& addresses,
                              const BoundNetLog& net_log);

  Request* request_;

  const HttpRequestInfo request_info_;
  RequestPriority priority_;
  ProxyInfo proxy_info_;
  SSLConfig server_ssl_config_;
  SSLConfig proxy_ssl_config_;
  const BoundNetLog net_log_;

  CompletionCallback io_callback_;
  scoped_ptr<ClientSocketHandle> connection_;
  HttpNetworkSession* const session_;
  HttpStreamFactoryImpl* const stream_factory_;
  State next_state_;
  ProxyService::PacRequest* pac_request_;
  SSLInfo ssl_info_;

  // The server we are trying to reach, could be that of the origin or of the
  // alternative service.
  HostPortPair server_;

  // The origin url we're trying to reach. This url may be different from the
  // original request when host mapping rules are set-up.
  GURL origin_url_;

  // AlternativeService for this Job if this is an alternative Job.
  const AlternativeService alternative_service_;

  // AlternativeService for the other Job if this is not an alternative Job.
  AlternativeService other_job_alternative_service_;

  // This is the Job we're dependent on. It will notify us if/when it's OK to
  // proceed.
  Job* blocking_job_;

  // |waiting_job_| is a Job waiting to see if |this| can reuse a connection.
  // If |this| is unable to do so, we'll notify |waiting_job_| that it's ok to
  // proceed and then race the two Jobs.
  Job* waiting_job_;

  // True if handling a HTTPS request, or using SPDY with SSL
  bool using_ssl_;

  // True if this network transaction is using SPDY instead of HTTP.
  bool using_spdy_;

  // True if this network transaction is using QUIC instead of HTTP.
  bool using_quic_;
  QuicStreamRequest quic_request_;

  // True if this job used an existing QUIC session.
  bool using_existing_quic_session_;

  // Force quic for a specific port.
  int force_quic_port_;

  // The certificate error while using SPDY over SSL for insecure URLs.
  int spdy_certificate_error_;

  scoped_refptr<HttpAuthController>
      auth_controllers_[HttpAuth::AUTH_NUM_TARGETS];

  // True when the tunnel is in the process of being established - we can't
  // read from the socket until the tunnel is done.
  bool establishing_tunnel_;

  scoped_ptr<HttpStream> stream_;
  scoped_ptr<WebSocketHandshakeStreamBase> websocket_stream_;

  // True if we negotiated NPN.
  bool was_npn_negotiated_;

  // Protocol negotiated with the server.
  NextProto protocol_negotiated_;

  // 0 if we're not preconnecting. Otherwise, the number of streams to
  // preconnect.
  int num_streams_;

  scoped_ptr<ValidSpdySessionPool> valid_spdy_session_pool_;

  // Initialized when we create a new SpdySession.
  base::WeakPtr<SpdySession> new_spdy_session_;

  // Initialized when we have an existing SpdySession.
  base::WeakPtr<SpdySession> existing_spdy_session_;

  // Only used if |new_spdy_session_| is non-NULL.
  bool spdy_session_direct_;

  JobStatus job_status_;
  JobStatus other_job_status_;

  base::WeakPtrFactory<Job> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_IMPL_JOB_H_
