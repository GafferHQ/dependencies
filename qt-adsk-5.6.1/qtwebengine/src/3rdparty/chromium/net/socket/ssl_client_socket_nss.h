// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_

#include <certt.h>
#include <keyt.h>
#include <nspr.h>
#include <nss.h>

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/nss_memio.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/ssl_config_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {

class BoundNetLog;
class CertPolicyEnforcer;
class CertVerifier;
class ChannelIDService;
class CTVerifier;
class ClientSocketHandle;
class TransportSecurityState;
class X509Certificate;

// An SSL client socket implemented with Mozilla NSS.
class SSLClientSocketNSS : public SSLClientSocket {
 public:
  // Takes ownership of the |transport_socket|, which must already be connected.
  // The hostname specified in |host_and_port| will be compared with the name(s)
  // in the server's certificate during the SSL handshake.  If SSL client
  // authentication is requested, the host_and_port field of SSLCertRequestInfo
  // will be populated with |host_and_port|.  |ssl_config| specifies
  // the SSL settings.
  //
  // Because calls to NSS may block, such as due to needing to access slow
  // hardware or needing to synchronously unlock protected tokens, calls to
  // NSS may optionally be run on a dedicated thread. If synchronous/blocking
  // behaviour is desired, for performance or compatibility, the current task
  // runner should be supplied instead.
  SSLClientSocketNSS(base::SequencedTaskRunner* nss_task_runner,
                     scoped_ptr<ClientSocketHandle> transport_socket,
                     const HostPortPair& host_and_port,
                     const SSLConfig& ssl_config,
                     const SSLClientSocketContext& context);
  ~SSLClientSocketNSS() override;

  // SSLClientSocket implementation.
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override;
  NextProtoStatus GetNextProto(std::string* proto) const override;

  // SSLSocket implementation.
  int ExportKeyingMaterial(const base::StringPiece& label,
                           bool has_context,
                           const base::StringPiece& context,
                           unsigned char* out,
                           unsigned int outlen) override;
  int GetTLSUniqueChannelBinding(std::string* out) override;

  // StreamSocket implementation.
  int Connect(const CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32 size) override;
  int SetSendBufferSize(int32 size) override;

  // SSLClientSocket implementation.
  ChannelIDService* GetChannelIDService() const override;
  SSLFailureState GetSSLFailureState() const override;

 private:
  // Helper class to handle marshalling any NSS interaction to and from the
  // NSS and network task runners. Not every call needs to happen on the Core
  class Core;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_HANDSHAKE_COMPLETE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };

  int Init();
  void InitCore();

  // Initializes NSS SSL options.  Returns a net error code.
  int InitializeSSLOptions();

  // Initializes the socket peer name in SSL.  Returns a net error code.
  int InitializeSSLPeerName();

  void DoConnectCallback(int result);
  void OnHandshakeIOComplete(int result);

  int DoHandshakeLoop(int last_io_result);
  int DoHandshake();
  int DoHandshakeComplete(int result);
  int DoVerifyCert(int result);
  int DoVerifyCertComplete(int result);

  void VerifyCT();

  // The following methods are for debugging bug 65948. Will remove this code
  // after fixing bug 65948.
  void EnsureThreadIdAssigned() const;
  bool CalledOnValidThread() const;

  // Adds the SignedCertificateTimestamps from ct_verify_result_ to |ssl_info|.
  // SCTs are held in three separate vectors in ct_verify_result, each
  // vetor representing a particular verification state, this method associates
  // each of the SCTs with the corresponding SCTVerifyStatus as it adds it to
  // the |ssl_info|.signed_certificate_timestamps list.
  void AddSCTInfoToSSLInfo(SSLInfo* ssl_info) const;

  // The task runner used to perform NSS operations.
  scoped_refptr<base::SequencedTaskRunner> nss_task_runner_;
  scoped_ptr<ClientSocketHandle> transport_;
  HostPortPair host_and_port_;
  SSLConfig ssl_config_;

  scoped_refptr<Core> core_;

  CompletionCallback user_connect_callback_;

  CertVerifyResult server_cert_verify_result_;

  CertVerifier* const cert_verifier_;
  scoped_ptr<CertVerifier::Request> cert_verifier_request_;

  // Certificate Transparency: Verifier and result holder.
  ct::CTVerifyResult ct_verify_result_;
  CTVerifier* cert_transparency_verifier_;

  // The service for retrieving Channel ID keys.  May be NULL.
  ChannelIDService* channel_id_service_;

  // ssl_session_cache_shard_ is an opaque string that partitions the SSL
  // session cache. i.e. sessions created with one value will not attempt to
  // resume on the socket with a different value.
  const std::string ssl_session_cache_shard_;

  // True if the SSL handshake has been completed.
  bool completed_handshake_;

  State next_handshake_state_;

  // The NSS SSL state machine. This is owned by |core_|.
  // TODO(rsleevi): http://crbug.com/130616 - Remove this member once
  // ExportKeyingMaterial is updated to be asynchronous.
  PRFileDesc* nss_fd_;

  BoundNetLog net_log_;

  base::TimeTicks start_cert_verification_time_;

  TransportSecurityState* transport_security_state_;

  CertPolicyEnforcer* const policy_enforcer_;

  // pinning_failure_log contains a message produced by
  // TransportSecurityState::CheckPublicKeyPins in the event of a
  // pinning failure. It is a (somewhat) human-readable string.
  std::string pinning_failure_log_;

  // The following two variables are added for debugging bug 65948. Will
  // remove this code after fixing bug 65948.
  // Added the following code Debugging in release mode.
  mutable base::Lock lock_;
  // This is mutable so that CalledOnValidThread can set it.
  // It's guarded by |lock_|.
  mutable base::PlatformThreadId valid_thread_id_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_NSS_H_
