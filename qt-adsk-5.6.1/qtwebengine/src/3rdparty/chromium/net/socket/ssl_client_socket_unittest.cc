// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/asn1_util.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(USE_OPENSSL)
#include <pk11pub.h>
#include "crypto/nss_util.h"

#if !defined(CKM_AES_GCM)
#define CKM_AES_GCM 0x00001087
#endif

#if !defined(CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256)
#define CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256 (CKM_NSS + 24)
#endif
#endif

//-----------------------------------------------------------------------------

using testing::_;
using testing::Return;
using testing::Truly;

namespace net {

namespace {

// WrappedStreamSocket is a base class that wraps an existing StreamSocket,
// forwarding the Socket and StreamSocket interfaces to the underlying
// transport.
// This is to provide a common base class for subclasses to override specific
// StreamSocket methods for testing, while still communicating with a 'real'
// StreamSocket.
class WrappedStreamSocket : public StreamSocket {
 public:
  explicit WrappedStreamSocket(scoped_ptr<StreamSocket> transport)
      : transport_(transport.Pass()) {}
  ~WrappedStreamSocket() override {}

  // StreamSocket implementation:
  int Connect(const CompletionCallback& callback) override {
    return transport_->Connect(callback);
  }
  void Disconnect() override { transport_->Disconnect(); }
  bool IsConnected() const override { return transport_->IsConnected(); }
  bool IsConnectedAndIdle() const override {
    return transport_->IsConnectedAndIdle();
  }
  int GetPeerAddress(IPEndPoint* address) const override {
    return transport_->GetPeerAddress(address);
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    return transport_->GetLocalAddress(address);
  }
  const BoundNetLog& NetLog() const override { return transport_->NetLog(); }
  void SetSubresourceSpeculation() override {
    transport_->SetSubresourceSpeculation();
  }
  void SetOmniboxSpeculation() override { transport_->SetOmniboxSpeculation(); }
  bool WasEverUsed() const override { return transport_->WasEverUsed(); }
  bool UsingTCPFastOpen() const override {
    return transport_->UsingTCPFastOpen();
  }
  bool WasNpnNegotiated() const override {
    return transport_->WasNpnNegotiated();
  }
  NextProto GetNegotiatedProtocol() const override {
    return transport_->GetNegotiatedProtocol();
  }
  bool GetSSLInfo(SSLInfo* ssl_info) override {
    return transport_->GetSSLInfo(ssl_info);
  }
  void GetConnectionAttempts(ConnectionAttempts* out) const override {
    transport_->GetConnectionAttempts(out);
  }
  void ClearConnectionAttempts() override {
    transport_->ClearConnectionAttempts();
  }
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {
    transport_->AddConnectionAttempts(attempts);
  }

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override {
    return transport_->Read(buf, buf_len, callback);
  }
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override {
    return transport_->Write(buf, buf_len, callback);
  }
  int SetReceiveBufferSize(int32 size) override {
    return transport_->SetReceiveBufferSize(size);
  }
  int SetSendBufferSize(int32 size) override {
    return transport_->SetSendBufferSize(size);
  }

 protected:
  scoped_ptr<StreamSocket> transport_;
};

// ReadBufferingStreamSocket is a wrapper for an existing StreamSocket that
// will ensure a certain amount of data is internally buffered before
// satisfying a Read() request. It exists to mimic OS-level internal
// buffering, but in a way to guarantee that X number of bytes will be
// returned to callers of Read(), regardless of how quickly the OS receives
// them from the TestServer.
class ReadBufferingStreamSocket : public WrappedStreamSocket {
 public:
  explicit ReadBufferingStreamSocket(scoped_ptr<StreamSocket> transport);
  ~ReadBufferingStreamSocket() override {}

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;

  // Sets the internal buffer to |size|. This must not be greater than
  // the largest value supplied to Read() - that is, it does not handle
  // having "leftovers" at the end of Read().
  // Each call to Read() will be prevented from completion until at least
  // |size| data has been read.
  // Set to 0 to turn off buffering, causing Read() to transparently
  // read via the underlying transport.
  void SetBufferSize(int size);

 private:
  enum State {
    STATE_NONE,
    STATE_READ,
    STATE_READ_COMPLETE,
  };

  int DoLoop(int result);
  int DoRead();
  int DoReadComplete(int result);
  void OnReadCompleted(int result);

  State state_;
  scoped_refptr<GrowableIOBuffer> read_buffer_;
  int buffer_size_;

  scoped_refptr<IOBuffer> user_read_buf_;
  CompletionCallback user_read_callback_;
};

ReadBufferingStreamSocket::ReadBufferingStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      read_buffer_(new GrowableIOBuffer()),
      buffer_size_(0) {}

void ReadBufferingStreamSocket::SetBufferSize(int size) {
  DCHECK(!user_read_buf_.get());
  buffer_size_ = size;
  read_buffer_->SetCapacity(size);
}

int ReadBufferingStreamSocket::Read(IOBuffer* buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  if (buffer_size_ == 0)
    return transport_->Read(buf, buf_len, callback);

  if (buf_len < buffer_size_)
    return ERR_UNEXPECTED;

  state_ = STATE_READ;
  user_read_buf_ = buf;
  int result = DoLoop(OK);
  if (result == ERR_IO_PENDING)
    user_read_callback_ = callback;
  else
    user_read_buf_ = NULL;
  return result;
}

int ReadBufferingStreamSocket::DoLoop(int result) {
  int rv = result;
  do {
    State current_state = state_;
    state_ = STATE_NONE;
    switch (current_state) {
      case STATE_READ:
        rv = DoRead();
        break;
      case STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case STATE_NONE:
      default:
        NOTREACHED() << "Unexpected state: " << current_state;
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && state_ != STATE_NONE);
  return rv;
}

int ReadBufferingStreamSocket::DoRead() {
  state_ = STATE_READ_COMPLETE;
  int rv =
      transport_->Read(read_buffer_.get(),
                       read_buffer_->RemainingCapacity(),
                       base::Bind(&ReadBufferingStreamSocket::OnReadCompleted,
                                  base::Unretained(this)));
  return rv;
}

int ReadBufferingStreamSocket::DoReadComplete(int result) {
  state_ = STATE_NONE;
  if (result <= 0)
    return result;

  read_buffer_->set_offset(read_buffer_->offset() + result);
  if (read_buffer_->RemainingCapacity() > 0) {
    state_ = STATE_READ;
    return OK;
  }

  memcpy(user_read_buf_->data(),
         read_buffer_->StartOfBuffer(),
         read_buffer_->capacity());
  read_buffer_->set_offset(0);
  return read_buffer_->capacity();
}

void ReadBufferingStreamSocket::OnReadCompleted(int result) {
  result = DoLoop(result);
  if (result == ERR_IO_PENDING)
    return;

  user_read_buf_ = NULL;
  base::ResetAndReturn(&user_read_callback_).Run(result);
}

// Simulates synchronously receiving an error during Read() or Write()
class SynchronousErrorStreamSocket : public WrappedStreamSocket {
 public:
  explicit SynchronousErrorStreamSocket(scoped_ptr<StreamSocket> transport);
  ~SynchronousErrorStreamSocket() override {}

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;

  // Sets the next Read() call and all future calls to return |error|.
  // If there is already a pending asynchronous read, the configured error
  // will not be returned until that asynchronous read has completed and Read()
  // is called again.
  void SetNextReadError(int error) {
    DCHECK_GE(0, error);
    have_read_error_ = true;
    pending_read_error_ = error;
  }

  // Sets the next Write() call and all future calls to return |error|.
  // If there is already a pending asynchronous write, the configured error
  // will not be returned until that asynchronous write has completed and
  // Write() is called again.
  void SetNextWriteError(int error) {
    DCHECK_GE(0, error);
    have_write_error_ = true;
    pending_write_error_ = error;
  }

 private:
  bool have_read_error_;
  int pending_read_error_;

  bool have_write_error_;
  int pending_write_error_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousErrorStreamSocket);
};

SynchronousErrorStreamSocket::SynchronousErrorStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      have_read_error_(false),
      pending_read_error_(OK),
      have_write_error_(false),
      pending_write_error_(OK) {}

int SynchronousErrorStreamSocket::Read(IOBuffer* buf,
                                       int buf_len,
                                       const CompletionCallback& callback) {
  if (have_read_error_)
    return pending_read_error_;
  return transport_->Read(buf, buf_len, callback);
}

int SynchronousErrorStreamSocket::Write(IOBuffer* buf,
                                        int buf_len,
                                        const CompletionCallback& callback) {
  if (have_write_error_)
    return pending_write_error_;
  return transport_->Write(buf, buf_len, callback);
}

// FakeBlockingStreamSocket wraps an existing StreamSocket and simulates the
// underlying transport needing to complete things asynchronously in a
// deterministic manner (e.g.: independent of the TestServer and the OS's
// semantics).
class FakeBlockingStreamSocket : public WrappedStreamSocket {
 public:
  explicit FakeBlockingStreamSocket(scoped_ptr<StreamSocket> transport);
  ~FakeBlockingStreamSocket() override {}

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;

  int pending_read_result() const { return pending_read_result_; }
  IOBuffer* pending_read_buf() const { return pending_read_buf_.get(); }

  // Blocks read results on the socket. Reads will not complete until
  // UnblockReadResult() has been called and a result is ready from the
  // underlying transport. Note: if BlockReadResult() is called while there is a
  // hanging asynchronous Read(), that Read is blocked.
  void BlockReadResult();
  void UnblockReadResult();

  // Waits for the blocked Read() call to be complete at the underlying
  // transport.
  void WaitForReadResult();

  // Causes the next call to Write() to return ERR_IO_PENDING, not beginning the
  // underlying transport until UnblockWrite() has been called. Note: if there
  // is a pending asynchronous write, it is NOT blocked. For purposes of
  // blocking writes, data is considered to have reached the underlying
  // transport as soon as Write() is called.
  void BlockWrite();
  void UnblockWrite();

  // Waits for the blocked Write() call to be scheduled.
  void WaitForWrite();

 private:
  // Handles completion from the underlying transport read.
  void OnReadCompleted(int result);

  // True if read callbacks are blocked.
  bool should_block_read_;

  // The buffer for the pending read, or NULL if not consumed.
  scoped_refptr<IOBuffer> pending_read_buf_;

  // The user callback for the pending read call.
  CompletionCallback pending_read_callback_;

  // The result for the blocked read callback, or ERR_IO_PENDING if not
  // completed.
  int pending_read_result_;

  // WaitForReadResult() wait loop.
  scoped_ptr<base::RunLoop> read_loop_;

  // True if write calls are blocked.
  bool should_block_write_;

  // The buffer for the pending write, or NULL if not scheduled.
  scoped_refptr<IOBuffer> pending_write_buf_;

  // The callback for the pending write call.
  CompletionCallback pending_write_callback_;

  // The length for the pending write, or -1 if not scheduled.
  int pending_write_len_;

  // WaitForWrite() wait loop.
  scoped_ptr<base::RunLoop> write_loop_;
};

FakeBlockingStreamSocket::FakeBlockingStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      should_block_read_(false),
      pending_read_result_(ERR_IO_PENDING),
      should_block_write_(false),
      pending_write_len_(-1) {}

int FakeBlockingStreamSocket::Read(IOBuffer* buf,
                                   int len,
                                   const CompletionCallback& callback) {
  DCHECK(!pending_read_buf_);
  DCHECK(pending_read_callback_.is_null());
  DCHECK_EQ(ERR_IO_PENDING, pending_read_result_);
  DCHECK(!callback.is_null());

  int rv = transport_->Read(buf, len, base::Bind(
      &FakeBlockingStreamSocket::OnReadCompleted, base::Unretained(this)));
  if (rv == ERR_IO_PENDING) {
    // Save the callback to be called later.
    pending_read_buf_ = buf;
    pending_read_callback_ = callback;
  } else if (should_block_read_) {
    // Save the callback and read result to be called later.
    pending_read_buf_ = buf;
    pending_read_callback_ = callback;
    OnReadCompleted(rv);
    rv = ERR_IO_PENDING;
  }
  return rv;
}

int FakeBlockingStreamSocket::Write(IOBuffer* buf,
                                    int len,
                                    const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_LE(0, len);

  if (!should_block_write_)
    return transport_->Write(buf, len, callback);

  // Schedule the write, but do nothing.
  DCHECK(!pending_write_buf_.get());
  DCHECK_EQ(-1, pending_write_len_);
  DCHECK(pending_write_callback_.is_null());
  DCHECK(!callback.is_null());
  pending_write_buf_ = buf;
  pending_write_len_ = len;
  pending_write_callback_ = callback;

  // Stop the write loop, if any.
  if (write_loop_)
    write_loop_->Quit();
  return ERR_IO_PENDING;
}

void FakeBlockingStreamSocket::BlockReadResult() {
  DCHECK(!should_block_read_);
  should_block_read_ = true;
}

void FakeBlockingStreamSocket::UnblockReadResult() {
  DCHECK(should_block_read_);
  should_block_read_ = false;

  // If the operation is still pending in the underlying transport, immediately
  // return - OnReadCompleted() will handle invoking the callback once the
  // transport has completed.
  if (pending_read_result_ == ERR_IO_PENDING)
    return;
  int result = pending_read_result_;
  pending_read_buf_ = nullptr;
  pending_read_result_ = ERR_IO_PENDING;
  base::ResetAndReturn(&pending_read_callback_).Run(result);
}

void FakeBlockingStreamSocket::WaitForReadResult() {
  DCHECK(should_block_read_);
  DCHECK(!read_loop_);

  if (pending_read_result_ != ERR_IO_PENDING)
    return;
  read_loop_.reset(new base::RunLoop);
  read_loop_->Run();
  read_loop_.reset();
  DCHECK_NE(ERR_IO_PENDING, pending_read_result_);
}

void FakeBlockingStreamSocket::BlockWrite() {
  DCHECK(!should_block_write_);
  should_block_write_ = true;
}

void FakeBlockingStreamSocket::UnblockWrite() {
  DCHECK(should_block_write_);
  should_block_write_ = false;

  // Do nothing if UnblockWrite() was called after BlockWrite(),
  // without a Write() in between.
  if (!pending_write_buf_.get())
    return;

  int rv = transport_->Write(
      pending_write_buf_.get(), pending_write_len_, pending_write_callback_);
  pending_write_buf_ = NULL;
  pending_write_len_ = -1;
  if (rv == ERR_IO_PENDING) {
    pending_write_callback_.Reset();
  } else {
    base::ResetAndReturn(&pending_write_callback_).Run(rv);
  }
}

void FakeBlockingStreamSocket::WaitForWrite() {
  DCHECK(should_block_write_);
  DCHECK(!write_loop_);

  if (pending_write_buf_.get())
    return;
  write_loop_.reset(new base::RunLoop);
  write_loop_->Run();
  write_loop_.reset();
  DCHECK(pending_write_buf_.get());
}

void FakeBlockingStreamSocket::OnReadCompleted(int result) {
  DCHECK_EQ(ERR_IO_PENDING, pending_read_result_);
  DCHECK(!pending_read_callback_.is_null());

  if (should_block_read_) {
    // Store the result so that the callback can be invoked once Unblock() is
    // called.
    pending_read_result_ = result;

    // Stop the WaitForReadResult() call if any.
    if (read_loop_)
      read_loop_->Quit();
  } else {
    // Either the Read() was never blocked or UnblockReadResult() was called
    // before the Read() completed. Either way, return the result to the caller.
    pending_read_buf_ = nullptr;
    base::ResetAndReturn(&pending_read_callback_).Run(result);
  }
}

// CountingStreamSocket wraps an existing StreamSocket and maintains a count of
// reads and writes on the socket.
class CountingStreamSocket : public WrappedStreamSocket {
 public:
  explicit CountingStreamSocket(scoped_ptr<StreamSocket> transport)
      : WrappedStreamSocket(transport.Pass()),
        read_count_(0),
        write_count_(0) {}
  ~CountingStreamSocket() override {}

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override {
    read_count_++;
    return transport_->Read(buf, buf_len, callback);
  }
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override {
    write_count_++;
    return transport_->Write(buf, buf_len, callback);
  }

  int read_count() const { return read_count_; }
  int write_count() const { return write_count_; }

 private:
  int read_count_;
  int write_count_;
};

// CompletionCallback that will delete the associated StreamSocket when
// the callback is invoked.
class DeleteSocketCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSocketCallback(StreamSocket* socket)
      : socket_(socket),
        callback_(base::Bind(&DeleteSocketCallback::OnComplete,
                             base::Unretained(this))) {}
  ~DeleteSocketCallback() override {}

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    if (socket_) {
      delete socket_;
      socket_ = NULL;
    } else {
      ADD_FAILURE() << "Deleting socket twice";
    }
    SetResult(result);
  }

  StreamSocket* socket_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSocketCallback);
};

// A ChannelIDStore that always returns an error when asked for a
// channel id.
class FailingChannelIDStore : public ChannelIDStore {
  int GetChannelID(const std::string& server_identifier,
                   scoped_ptr<crypto::ECPrivateKey>* key_result,
                   const GetChannelIDCallback& callback) override {
    return ERR_UNEXPECTED;
  }
  void SetChannelID(scoped_ptr<ChannelID> channel_id) override {}
  void DeleteChannelID(const std::string& server_identifier,
                       const base::Closure& completion_callback) override {}
  void DeleteAllCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& completion_callback) override {}
  void DeleteAll(const base::Closure& completion_callback) override {}
  void GetAllChannelIDs(const GetChannelIDListCallback& callback) override {}
  int GetChannelIDCount() override { return 0; }
  void SetForceKeepSessionState() override {}
};

// A ChannelIDStore that asynchronously returns an error when asked for a
// channel id.
class AsyncFailingChannelIDStore : public ChannelIDStore {
  int GetChannelID(const std::string& server_identifier,
                   scoped_ptr<crypto::ECPrivateKey>* key_result,
                   const GetChannelIDCallback& callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, ERR_UNEXPECTED, server_identifier, nullptr));
    return ERR_IO_PENDING;
  }
  void SetChannelID(scoped_ptr<ChannelID> channel_id) override {}
  void DeleteChannelID(const std::string& server_identifier,
                       const base::Closure& completion_callback) override {}
  void DeleteAllCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      const base::Closure& completion_callback) override {}
  void DeleteAll(const base::Closure& completion_callback) override {}
  void GetAllChannelIDs(const GetChannelIDListCallback& callback) override {}
  int GetChannelIDCount() override { return 0; }
  void SetForceKeepSessionState() override {}
};

// A mock CTVerifier that records every call to Verify but doesn't verify
// anything.
class MockCTVerifier : public CTVerifier {
 public:
  MOCK_METHOD5(Verify, int(X509Certificate*,
                           const std::string&,
                           const std::string&,
                           ct::CTVerifyResult*,
                           const BoundNetLog&));
  MOCK_METHOD1(SetObserver, void(CTVerifier::Observer*));
};

class SSLClientSocketTest : public PlatformTest {
 public:
  SSLClientSocketTest()
      : socket_factory_(ClientSocketFactory::GetDefaultFactory()),
        cert_verifier_(new MockCertVerifier),
        transport_security_state_(new TransportSecurityState) {
    cert_verifier_->set_default_result(OK);
    context_.cert_verifier = cert_verifier_.get();
    context_.transport_security_state = transport_security_state_.get();
  }

 protected:
  // The address of the spawned test server, after calling StartTestServer().
  const AddressList& addr() const { return addr_; }

  // The SpawnedTestServer object, after calling StartTestServer().
  const SpawnedTestServer* test_server() const { return test_server_.get(); }

  void SetCTVerifier(CTVerifier* ct_verifier) {
    context_.cert_transparency_verifier = ct_verifier;
  }

  // Starts the test server with SSL configuration |ssl_options|. Returns true
  // on success.
  bool StartTestServer(const SpawnedTestServer::SSLOptions& ssl_options) {
    test_server_.reset(new SpawnedTestServer(
        SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath()));
    if (!test_server_->Start()) {
      LOG(ERROR) << "Could not start SpawnedTestServer";
      return false;
    }

    if (!test_server_->GetAddressList(&addr_)) {
      LOG(ERROR) << "Could not get SpawnedTestServer address list";
      return false;
    }
    return true;
  }

  // Sets up a TCP connection to a HTTPS server. To actually do the SSL
  // handshake, follow up with call to CreateAndConnectSSLClientSocket() below.
  bool ConnectToTestServer(const SpawnedTestServer::SSLOptions& ssl_options) {
    if (!StartTestServer(ssl_options))
      return false;

    transport_.reset(new TCPClientSocket(addr_, &log_, NetLog::Source()));
    int rv = callback_.GetResult(transport_->Connect(callback_.callback()));
    if (rv != OK) {
      LOG(ERROR) << "Could not connect to SpawnedTestServer";
      return false;
    }
    return true;
  }

  scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<StreamSocket> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) {
    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    connection->SetSocket(transport_socket.Pass());
    return socket_factory_->CreateSSLClientSocket(
        connection.Pass(), host_and_port, ssl_config, context_);
  }

  // Create an SSLClientSocket object and use it to connect to a test
  // server, then wait for connection results. This must be called after
  // a successful ConnectToTestServer() call.
  // |ssl_config| the SSL configuration to use.
  // |result| will retrieve the ::Connect() result value.
  // Returns true on success, false otherwise. Success means that the socket
  // could be created and its Connect() was called, not that the connection
  // itself was a success.
  bool CreateAndConnectSSLClientSocket(SSLConfig& ssl_config, int* result) {
    sock_ = CreateSSLClientSocket(
        transport_.Pass(), test_server_->host_port_pair(), ssl_config);

    if (sock_->IsConnected()) {
      LOG(ERROR) << "SSL Socket prematurely connected";
      return false;
    }

    *result = callback_.GetResult(sock_->Connect(callback_.callback()));
    return true;
  }

  ClientSocketFactory* socket_factory_;
  scoped_ptr<MockCertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
  SSLClientSocketContext context_;
  scoped_ptr<SSLClientSocket> sock_;
  TestNetLog log_;

 private:
  scoped_ptr<StreamSocket> transport_;
  scoped_ptr<SpawnedTestServer> test_server_;
  TestCompletionCallback callback_;
  AddressList addr_;
};

// Verifies the correctness of GetSSLCertRequestInfo.
class SSLClientSocketCertRequestInfoTest : public SSLClientSocketTest {
 protected:
  // Creates a test server with the given SSLOptions, connects to it and returns
  // the SSLCertRequestInfo reported by the socket.
  scoped_refptr<SSLCertRequestInfo> GetCertRequest(
      SpawnedTestServer::SSLOptions ssl_options) {
    SpawnedTestServer test_server(
        SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
    if (!test_server.Start())
      return NULL;

    AddressList addr;
    if (!test_server.GetAddressList(&addr))
      return NULL;

    TestCompletionCallback callback;
    TestNetLog log;
    scoped_ptr<StreamSocket> transport(
        new TCPClientSocket(addr, &log, NetLog::Source()));
    int rv = transport->Connect(callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
        transport.Pass(), test_server.host_port_pair(), SSLConfig()));
    EXPECT_FALSE(sock->IsConnected());

    rv = sock->Connect(callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    scoped_refptr<SSLCertRequestInfo> request_info = new SSLCertRequestInfo();
    sock->GetSSLCertRequestInfo(request_info.get());
    sock->Disconnect();
    EXPECT_FALSE(sock->IsConnected());
    EXPECT_TRUE(
        test_server.host_port_pair().Equals(request_info->host_and_port));

    return request_info;
  }
};

class SSLClientSocketFalseStartTest : public SSLClientSocketTest {
 protected:
  // Creates an SSLClientSocket with |client_config| attached to a
  // FakeBlockingStreamSocket, returning both in |*out_raw_transport| and
  // |*out_sock|. The FakeBlockingStreamSocket is owned by the SSLClientSocket,
  // so |*out_raw_transport| is a raw pointer.
  //
  // The client socket will begin a connect using |callback| but stop before the
  // server's finished message is received. The finished message will be blocked
  // in |*out_raw_transport|. To complete the handshake and successfully read
  // data, the caller must unblock reads on |*out_raw_transport|. (Note that, if
  // the client successfully false started, |callback.WaitForResult()| will
  // return OK without unblocking transport reads. But Read() will still block.)
  //
  // Must be called after StartTestServer is called.
  void CreateAndConnectUntilServerFinishedReceived(
      const SSLConfig& client_config,
      TestCompletionCallback* callback,
      FakeBlockingStreamSocket** out_raw_transport,
      scoped_ptr<SSLClientSocket>* out_sock) {
    CHECK(test_server());

    scoped_ptr<StreamSocket> real_transport(
        new TCPClientSocket(addr(), NULL, NetLog::Source()));
    scoped_ptr<FakeBlockingStreamSocket> transport(
        new FakeBlockingStreamSocket(real_transport.Pass()));
    int rv = callback->GetResult(transport->Connect(callback->callback()));
    EXPECT_EQ(OK, rv);

    FakeBlockingStreamSocket* raw_transport = transport.get();
    scoped_ptr<SSLClientSocket> sock = CreateSSLClientSocket(
        transport.Pass(), test_server()->host_port_pair(), client_config);

    // Connect. Stop before the client processes the first server leg
    // (ServerHello, etc.)
    raw_transport->BlockReadResult();
    rv = sock->Connect(callback->callback());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    raw_transport->WaitForReadResult();

    // Release the ServerHello and wait for the client to write
    // ClientKeyExchange, etc. (A proxy for waiting for the entirety of the
    // server's leg to complete, since it may span multiple reads.)
    EXPECT_FALSE(callback->have_result());
    raw_transport->BlockWrite();
    raw_transport->UnblockReadResult();
    raw_transport->WaitForWrite();

    // And, finally, release that and block the next server leg
    // (ChangeCipherSpec, Finished).
    raw_transport->BlockReadResult();
    raw_transport->UnblockWrite();

    *out_raw_transport = raw_transport;
    *out_sock = sock.Pass();
  }

  void TestFalseStart(const SpawnedTestServer::SSLOptions& server_options,
                      const SSLConfig& client_config,
                      bool expect_false_start) {
    ASSERT_TRUE(StartTestServer(server_options));

    TestCompletionCallback callback;
    FakeBlockingStreamSocket* raw_transport = NULL;
    scoped_ptr<SSLClientSocket> sock;
    ASSERT_NO_FATAL_FAILURE(CreateAndConnectUntilServerFinishedReceived(
        client_config, &callback, &raw_transport, &sock));

    if (expect_false_start) {
      // When False Starting, the handshake should complete before receiving the
      // Change Cipher Spec and Finished messages.
      //
      // Note: callback.have_result() may not be true without waiting. The NSS
      // state machine sometimes lives on a separate thread, so this thread may
      // not yet have processed the signal that the handshake has completed.
      int rv = callback.WaitForResult();
      EXPECT_EQ(OK, rv);
      EXPECT_TRUE(sock->IsConnected());

      const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
      static const int kRequestTextSize =
          static_cast<int>(arraysize(request_text) - 1);
      scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
      memcpy(request_buffer->data(), request_text, kRequestTextSize);

      // Write the request.
      rv = callback.GetResult(sock->Write(request_buffer.get(),
                                          kRequestTextSize,
                                          callback.callback()));
      EXPECT_EQ(kRequestTextSize, rv);

      // The read will hang; it's waiting for the peer to complete the
      // handshake, and the handshake is still blocked.
      scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
      rv = sock->Read(buf.get(), 4096, callback.callback());

      // After releasing reads, the connection proceeds.
      raw_transport->UnblockReadResult();
      rv = callback.GetResult(rv);
      EXPECT_LT(0, rv);
    } else {
      // False Start is not enabled, so the handshake will not complete because
      // the server second leg is blocked.
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(callback.have_result());
    }
  }
};

class SSLClientSocketChannelIDTest : public SSLClientSocketTest {
 protected:
  void EnableChannelID() {
    channel_id_service_.reset(new ChannelIDService(
        new DefaultChannelIDStore(NULL), base::ThreadTaskRunnerHandle::Get()));
    context_.channel_id_service = channel_id_service_.get();
  }

  void EnableFailingChannelID() {
    channel_id_service_.reset(new ChannelIDService(
        new FailingChannelIDStore(), base::ThreadTaskRunnerHandle::Get()));
    context_.channel_id_service = channel_id_service_.get();
  }

  void EnableAsyncFailingChannelID() {
    channel_id_service_.reset(new ChannelIDService(
        new AsyncFailingChannelIDStore(), base::ThreadTaskRunnerHandle::Get()));
    context_.channel_id_service = channel_id_service_.get();
  }

 private:
  scoped_ptr<ChannelIDService> channel_id_service_;
};

//-----------------------------------------------------------------------------

bool SupportsAESGCM() {
#if defined(USE_OPENSSL)
  return true;
#else
  crypto::EnsureNSSInit();
  return PK11_TokenExists(CKM_AES_GCM) &&
         PK11_TokenExists(CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256);
#endif
}

}  // namespace

TEST_F(SSLClientSocketTest, Connect) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLog::TYPE_SSL_CONNECT));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, ConnectExpired) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_EXPIRED);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(ERR_CERT_DATE_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLog::TYPE_SSL_CONNECT));
}

TEST_F(SSLClientSocketTest, ConnectMismatched) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  cert_verifier_->set_default_result(ERR_CERT_COMMON_NAME_INVALID);

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLog::TYPE_SSL_CONNECT));
}

// Attempt to connect to a page which requests a client certificate. It should
// return an error code on connect.
TEST_F(SSLClientSocketTest, ConnectClientAuthCertRequested) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLog::TYPE_SSL_CONNECT));
  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);
  EXPECT_FALSE(sock->IsConnected());
}

// Connect to a server requesting optional client authentication. Send it a
// null certificate. It should allow the connection.
//
// TODO(davidben): Also test providing an actual certificate.
TEST_F(SSLClientSocketTest, ConnectClientAuthSendNullCert) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  ssl_config.send_client_cert = true;
  ssl_config.client_cert = NULL;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  // Our test server accepts certificate-less connections.
  // TODO(davidben): Add a test which requires them and verify the error.
  rv = sock->Connect(callback.callback());

  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLog::TYPE_SSL_CONNECT));

  // We responded to the server's certificate request with a Certificate
  // message with no client certificate in it.  ssl_info.client_cert_sent
  // should be false in this case.
  SSLInfo ssl_info;
  sock->GetSSLInfo(&ssl_info);
  EXPECT_FALSE(ssl_info.client_cert_sent);

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// TODO(wtc): Add unit tests for IsConnectedAndIdle:
//   - Server closes an SSL connection (with a close_notify alert message).
//   - Server closes the underlying TCP connection directly.
//   - Server sends data unexpectedly.

// Tests that the socket can be read from successfully. Also test that a peer's
// close_notify alert is successfully processed without error.
TEST_F(SSLClientSocketTest, Read) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  for (;;) {
    rv = sock->Read(buf.get(), 4096, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }

  // The peer should have cleanly closed the connection with a close_notify.
  EXPECT_EQ(0, rv);
}

// Tests that SSLClientSocket properly handles when the underlying transport
// synchronously fails a transport read in during the handshake. The error code
// should be preserved so SSLv3 fallback logic can condition on it.
TEST_F(SSLClientSocketTest, Connect_WithSynchronousError) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<SynchronousErrorStreamSocket> transport(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  SynchronousErrorStreamSocket* raw_transport = transport.get();
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  raw_transport->SetNextWriteError(ERR_CONNECTION_RESET);

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
  EXPECT_FALSE(sock->IsConnected());
}

// Tests that the SSLClientSocket properly handles when the underlying transport
// synchronously returns an error code - such as if an intermediary terminates
// the socket connection uncleanly.
// This is a regression test for http://crbug.com/238536
TEST_F(SSLClientSocketTest, Read_WithSynchronousError) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<SynchronousErrorStreamSocket> transport(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  SynchronousErrorStreamSocket* raw_transport = transport.get();
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  rv = callback.GetResult(
      sock->Write(request_buffer.get(), kRequestTextSize, callback.callback()));
  EXPECT_EQ(kRequestTextSize, rv);

  // Simulate an unclean/forcible shutdown.
  raw_transport->SetNextReadError(ERR_CONNECTION_RESET);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));

  // Note: This test will hang if this bug has regressed. Simply checking that
  // rv != ERR_IO_PENDING is insufficient, as ERR_IO_PENDING is a legitimate
  // result when using a dedicated task runner for NSS.
  rv = callback.GetResult(sock->Read(buf.get(), 4096, callback.callback()));
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
}

// Tests that the SSLClientSocket properly handles when the underlying transport
// asynchronously returns an error code while writing data - such as if an
// intermediary terminates the socket connection uncleanly.
// This is a regression test for http://crbug.com/249848
TEST_F(SSLClientSocketTest, Write_WithSynchronousError) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(error_socket.Pass()));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  // Simulate an unclean/forcible shutdown on the underlying socket.
  // However, simulate this error asynchronously.
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);
  raw_transport->BlockWrite();

  // This write should complete synchronously, because the TLS ciphertext
  // can be created and placed into the outgoing buffers independent of the
  // underlying transport.
  rv = callback.GetResult(
      sock->Write(request_buffer.get(), kRequestTextSize, callback.callback()));
  EXPECT_EQ(kRequestTextSize, rv);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));

  rv = sock->Read(buf.get(), 4096, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Now unblock the outgoing request, having it fail with the connection
  // being reset.
  raw_transport->UnblockWrite();

  // Note: This will cause an inifite loop if this bug has regressed. Simply
  // checking that rv != ERR_IO_PENDING is insufficient, as ERR_IO_PENDING
  // is a legitimate result when using a dedicated task runner for NSS.
  rv = callback.GetResult(rv);
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
}

// If there is a Write failure at the transport with no follow-up Read, although
// the write error will not be returned to the client until a future Read or
// Write operation, SSLClientSocket should not spin attempting to re-write on
// the socket. This is a regression test for part of https://crbug.com/381160.
TEST_F(SSLClientSocketTest, Write_WithSynchronousErrorNoRead) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  // Note: intermediate sockets' ownership are handed to |sock|, but a pointer
  // is retained in order to query them.
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<CountingStreamSocket> counting_socket(
      new CountingStreamSocket(error_socket.Pass()));
  CountingStreamSocket* raw_counting_socket = counting_socket.get();
  int rv = callback.GetResult(counting_socket->Connect(callback.callback()));
  ASSERT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      counting_socket.Pass(), test_server.host_port_pair(), ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  ASSERT_EQ(OK, rv);
  ASSERT_TRUE(sock->IsConnected());

  // Simulate an unclean/forcible shutdown on the underlying socket.
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  // This write should complete synchronously, because the TLS ciphertext
  // can be created and placed into the outgoing buffers independent of the
  // underlying transport.
  rv = callback.GetResult(
      sock->Write(request_buffer.get(), kRequestTextSize, callback.callback()));
  ASSERT_EQ(kRequestTextSize, rv);

  // Let the event loop spin for a little bit of time. Even on platforms where
  // pumping the state machine involve thread hops, there should be no further
  // writes on the transport socket.
  //
  // TODO(davidben): Avoid the arbitrary timeout?
  int old_write_count = raw_counting_socket->write_count();
  base::RunLoop loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::TimeDelta::FromMilliseconds(100));
  loop.Run();
  EXPECT_EQ(old_write_count, raw_counting_socket->write_count());
}

// Test the full duplex mode, with Read and Write pending at the same time.
// This test also serves as a regression test for http://crbug.com/29815.
TEST_F(SSLClientSocketTest, Read_FullDuplex) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;  // Used for everything except Write.

  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  // Issue a "hanging" Read first.
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  rv = sock->Read(buf.get(), 4096, callback.callback());
  // We haven't written the request, so there should be no response yet.
  ASSERT_EQ(ERR_IO_PENDING, rv);

  // Write the request.
  // The request is padded with a User-Agent header to a size that causes the
  // memio circular buffer (4k bytes) in SSLClientSocketNSS to wrap around.
  // This tests the fix for http://crbug.com/29815.
  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  for (int i = 0; i < 3770; ++i)
    request_text.push_back('*');
  request_text.append("\r\n\r\n");
  scoped_refptr<IOBuffer> request_buffer(new StringIOBuffer(request_text));

  TestCompletionCallback callback2;  // Used for Write only.
  rv = sock->Write(
      request_buffer.get(), request_text.size(), callback2.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback2.WaitForResult();
  EXPECT_EQ(static_cast<int>(request_text.size()), rv);

  // Now get the Read result.
  rv = callback.WaitForResult();
  EXPECT_GT(rv, 0);
}

// Attempts to Read() and Write() from an SSLClientSocketNSS in full duplex
// mode when the underlying transport is blocked on sending data. When the
// underlying transport completes due to an error, it should invoke both the
// Read() and Write() callbacks. If the socket is deleted by the Read()
// callback, the Write() callback should not be invoked.
// Regression test for http://crbug.com/232633
TEST_F(SSLClientSocketTest, Read_DeleteWhilePendingFullDuplex) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(error_socket.Pass()));
  FakeBlockingStreamSocket* raw_transport = transport.get();

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock = CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config);

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  request_text.append(20 * 1024, '*');
  request_text.append("\r\n\r\n");
  scoped_refptr<DrainableIOBuffer> request_buffer(new DrainableIOBuffer(
      new StringIOBuffer(request_text), request_text.size()));

  // Simulate errors being returned from the underlying Read() and Write() ...
  raw_error_socket->SetNextReadError(ERR_CONNECTION_RESET);
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);
  // ... but have those errors returned asynchronously. Because the Write() will
  // return first, this will trigger the error.
  raw_transport->BlockReadResult();
  raw_transport->BlockWrite();

  // Enqueue a Read() before calling Write(), which should "hang" due to
  // the ERR_IO_PENDING caused by SetReadShouldBlock() and thus return.
  SSLClientSocket* raw_sock = sock.get();
  DeleteSocketCallback read_callback(sock.release());
  scoped_refptr<IOBuffer> read_buf(new IOBuffer(4096));
  rv = raw_sock->Read(read_buf.get(), 4096, read_callback.callback());

  // Ensure things didn't complete synchronously, otherwise |sock| is invalid.
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_FALSE(read_callback.have_result());

#if !defined(USE_OPENSSL)
  // NSS follows a pattern where a call to PR_Write will only consume as
  // much data as it can encode into application data records before the
  // internal memio buffer is full, which should only fill if writing a large
  // amount of data and the underlying transport is blocked. Once this happens,
  // NSS will return (total size of all application data records it wrote) - 1,
  // with the caller expected to resume with the remaining unsent data.
  //
  // This causes SSLClientSocketNSS::Write to return that it wrote some data
  // before it will return ERR_IO_PENDING, so make an extra call to Write() to
  // get the socket in the state needed for the test below.
  //
  // This is not needed for OpenSSL, because for OpenSSL,
  // SSL_MODE_ENABLE_PARTIAL_WRITE is not specified - thus
  // SSLClientSocketOpenSSL::Write() will not return until all of
  // |request_buffer| has been written to the underlying BIO (although not
  // necessarily the underlying transport).
  rv = callback.GetResult(raw_sock->Write(request_buffer.get(),
                                          request_buffer->BytesRemaining(),
                                          callback.callback()));
  ASSERT_LT(0, rv);
  request_buffer->DidConsume(rv);

  // Guard to ensure that |request_buffer| was larger than all of the internal
  // buffers (transport, memio, NSS) along the way - otherwise the next call
  // to Write() will crash with an invalid buffer.
  ASSERT_LT(0, request_buffer->BytesRemaining());
#endif

  // Attempt to write the remaining data. NSS will not be able to consume the
  // application data because the internal buffers are full, while OpenSSL will
  // return that its blocked because the underlying transport is blocked.
  rv = raw_sock->Write(request_buffer.get(),
                       request_buffer->BytesRemaining(),
                       callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_FALSE(callback.have_result());

  // Now unblock Write(), which will invoke OnSendComplete and (eventually)
  // call the Read() callback, deleting the socket and thus aborting calling
  // the Write() callback.
  raw_transport->UnblockWrite();

  rv = read_callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  // The Write callback should not have been called.
  EXPECT_FALSE(callback.have_result());
}

// Tests that the SSLClientSocket does not crash if data is received on the
// transport socket after a failing write. This can occur if we have a Write
// error in a SPDY socket.
// Regression test for http://crbug.com/335557
TEST_F(SSLClientSocketTest, Read_WithWriteError) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(error_socket.Pass()));
  FakeBlockingStreamSocket* raw_transport = transport.get();

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  // Send a request so there is something to read from the socket.
  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  rv = callback.GetResult(
      sock->Write(request_buffer.get(), kRequestTextSize, callback.callback()));
  EXPECT_EQ(kRequestTextSize, rv);

  // Start a hanging read.
  TestCompletionCallback read_callback;
  raw_transport->BlockReadResult();
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  rv = sock->Read(buf.get(), 4096, read_callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Perform another write, but have it fail. Write a request larger than the
  // internal socket buffers so that the request hits the underlying transport
  // socket and detects the error.
  std::string long_request_text =
      "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  long_request_text.append(20 * 1024, '*');
  long_request_text.append("\r\n\r\n");
  scoped_refptr<DrainableIOBuffer> long_request_buffer(new DrainableIOBuffer(
      new StringIOBuffer(long_request_text), long_request_text.size()));

  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);

  // Write as much data as possible until hitting an error. This is necessary
  // for NSS. PR_Write will only consume as much data as it can encode into
  // application data records before the internal memio buffer is full, which
  // should only fill if writing a large amount of data and the underlying
  // transport is blocked. Once this happens, NSS will return (total size of all
  // application data records it wrote) - 1, with the caller expected to resume
  // with the remaining unsent data.
  do {
    rv = callback.GetResult(sock->Write(long_request_buffer.get(),
                                        long_request_buffer->BytesRemaining(),
                                        callback.callback()));
    if (rv > 0) {
      long_request_buffer->DidConsume(rv);
      // Abort if the entire buffer is ever consumed.
      ASSERT_LT(0, long_request_buffer->BytesRemaining());
    }
  } while (rv > 0);

  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  // Release the read.
  raw_transport->UnblockReadResult();
  rv = read_callback.WaitForResult();

#if defined(USE_OPENSSL)
  // Should still read bytes despite the write error.
  EXPECT_LT(0, rv);
#else
  // NSS attempts to flush the write buffer in PR_Read on an SSL socket before
  // pumping the read state machine, unless configured with SSL_ENABLE_FDX, so
  // the write error stops future reads.
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
#endif
}

// Tests that SSLClientSocket fails the handshake if the underlying
// transport is cleanly closed.
TEST_F(SSLClientSocketTest, Connect_WithZeroReturn) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<SynchronousErrorStreamSocket> transport(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  SynchronousErrorStreamSocket* raw_transport = transport.get();
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  raw_transport->SetNextReadError(0);

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
  EXPECT_FALSE(sock->IsConnected());
}

// Tests that SSLClientSocket returns a Read of size 0 if the underlying socket
// is cleanly closed, but the peer does not send close_notify.
// This is a regression test for https://crbug.com/422246
TEST_F(SSLClientSocketTest, Read_WithZeroReturn) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<SynchronousErrorStreamSocket> transport(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to ensure the handshake has completed.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  SynchronousErrorStreamSocket* raw_transport = transport.get();
  scoped_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(transport.Pass(),
                            test_server.host_port_pair(),
                            ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  raw_transport->SetNextReadError(0);
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  rv = callback.GetResult(sock->Read(buf.get(), 4096, callback.callback()));
  EXPECT_EQ(0, rv);
}

// Tests that SSLClientSocket cleanly returns a Read of size 0 if the
// underlying socket is cleanly closed asynchronously.
// This is a regression test for https://crbug.com/422246
TEST_F(SSLClientSocketTest, Read_WithAsyncZeroReturn) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(error_socket.Pass()));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to ensure the handshake has completed.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(transport.Pass(),
                            test_server.host_port_pair(),
                            ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  raw_error_socket->SetNextReadError(0);
  raw_transport->BlockReadResult();
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  rv = sock->Read(buf.get(), 4096, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  raw_transport->UnblockReadResult();
  rv = callback.GetResult(rv);
  EXPECT_EQ(0, rv);
}

// Tests that fatal alerts from the peer are processed. This is a regression
// test for https://crbug.com/466303.
TEST_F(SSLClientSocketTest, Read_WithFatalAlert) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.alert_after_handshake = true;
  ASSERT_TRUE(StartTestServer(ssl_options));

  SSLConfig ssl_config;
  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), ssl_config));
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));

  // Receive the fatal alert.
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, callback.GetResult(sock->Read(
                                        buf.get(), 4096, callback.callback())));
}

TEST_F(SSLClientSocketTest, Read_SmallChunks) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<IOBuffer> buf(new IOBuffer(1));
  for (;;) {
    rv = sock->Read(buf.get(), 1, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(SSLClientSocketTest, Read_ManySmallRecords) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;

  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<ReadBufferingStreamSocket> transport(
      new ReadBufferingStreamSocket(real_transport.Pass()));
  ReadBufferingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  ASSERT_EQ(OK, rv);
  ASSERT_TRUE(sock->IsConnected());

  const char request_text[] = "GET /ssl-many-small-records HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = callback.GetResult(sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback()));
  ASSERT_GT(rv, 0);
  ASSERT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  // Note: This relies on SSLClientSocketNSS attempting to read up to 17K of
  // data (the max SSL record size) at a time. Ensure that at least 15K worth
  // of SSL data is buffered first. The 15K of buffered data is made up of
  // many smaller SSL records (the TestServer writes along 1350 byte
  // plaintext boundaries), although there may also be a few records that are
  // smaller or larger, due to timing and SSL False Start.
  // 15K was chosen because 15K is smaller than the 17K (max) read issued by
  // the SSLClientSocket implementation, and larger than the minimum amount
  // of ciphertext necessary to contain the 8K of plaintext requested below.
  raw_transport->SetBufferSize(15000);

  scoped_refptr<IOBuffer> buffer(new IOBuffer(8192));
  rv = callback.GetResult(sock->Read(buffer.get(), 8192, callback.callback()));
  ASSERT_EQ(rv, 8192);
}

TEST_F(SSLClientSocketTest, Read_Interrupted) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<IOBuffer> buf(new IOBuffer(512));
  rv = sock->Read(buf.get(), 512, callback.callback());
  EXPECT_TRUE(rv > 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_GT(rv, 0);
}

TEST_F(SSLClientSocketTest, Read_FullLogging) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  log.SetCaptureMode(NetLogCaptureMode::IncludeSocketBytes());
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  size_t last_index = ExpectLogContainsSomewhereAfter(
      entries, 5, NetLog::TYPE_SSL_SOCKET_BYTES_SENT, NetLog::PHASE_NONE);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  for (;;) {
    rv = sock->Read(buf.get(), 4096, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;

    log.GetEntries(&entries);
    last_index =
        ExpectLogContainsSomewhereAfter(entries,
                                        last_index + 1,
                                        NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED,
                                        NetLog::PHASE_NONE);
  }
}

// Regression test for http://crbug.com/42538
TEST_F(SSLClientSocketTest, PrematureApplicationData) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  TestCompletionCallback callback;

  static const unsigned char application_data[] = {
      0x17, 0x03, 0x01, 0x00, 0x4a, 0x02, 0x00, 0x00, 0x46, 0x03, 0x01, 0x4b,
      0xc2, 0xf8, 0xb2, 0xc1, 0x56, 0x42, 0xb9, 0x57, 0x7f, 0xde, 0x87, 0x46,
      0xf7, 0xa3, 0x52, 0x42, 0x21, 0xf0, 0x13, 0x1c, 0x9c, 0x83, 0x88, 0xd6,
      0x93, 0x0c, 0xf6, 0x36, 0x30, 0x05, 0x7e, 0x20, 0xb5, 0xb5, 0x73, 0x36,
      0x53, 0x83, 0x0a, 0xfc, 0x17, 0x63, 0xbf, 0xa0, 0xe4, 0x42, 0x90, 0x0d,
      0x2f, 0x18, 0x6d, 0x20, 0xd8, 0x36, 0x3f, 0xfc, 0xe6, 0x01, 0xfa, 0x0f,
      0xa5, 0x75, 0x7f, 0x09, 0x00, 0x04, 0x00, 0x16, 0x03, 0x01, 0x11, 0x57,
      0x0b, 0x00, 0x11, 0x53, 0x00, 0x11, 0x50, 0x00, 0x06, 0x22, 0x30, 0x82,
      0x06, 0x1e, 0x30, 0x82, 0x05, 0x06, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02,
      0x0a};

  // All reads and writes complete synchronously (async=false).
  MockRead data_reads[] = {
      MockRead(SYNCHRONOUS,
               reinterpret_cast<const char*>(application_data),
               arraysize(application_data)),
      MockRead(SYNCHRONOUS, OK), };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);

  scoped_ptr<StreamSocket> transport(
      new MockTCPClientSocket(addr, NULL, &data));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, rv);
}

TEST_F(SSLClientSocketTest, CipherSuiteDisables) {
  // Rather than exhaustively disabling every AES_128_CBC ciphersuite defined at
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml, only
  // disabling those cipher suites that the test server actually implements.
  const uint16 kCiphersToDisable[] = {
      0x002f,  // TLS_RSA_WITH_AES_128_CBC_SHA
      0x0033,  // TLS_DHE_RSA_WITH_AES_128_CBC_SHA
      0xc013,  // TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
  };

  SpawnedTestServer::SSLOptions ssl_options;
  // Enable only AES_128_CBC on the test server.
  ssl_options.bulk_ciphers = SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  for (size_t i = 0; i < arraysize(kCiphersToDisable); ++i)
    ssl_config.disabled_cipher_suites.push_back(kCiphersToDisable[i]);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());
  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(ERR_SSL_VERSION_OR_CIPHER_MISMATCH, rv);
  // The exact ordering depends no whether an extra read is issued. Just check
  // the error is somewhere in the log.
  log.GetEntries(&entries);
  ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_SSL_HANDSHAKE_ERROR, NetLog::PHASE_NONE);

  // We cannot test sock->IsConnected(), as the NSS implementation disconnects
  // the socket when it encounters an error, whereas other implementations
  // leave it connected.
  // Because this an error that the test server is mutually aware of, as opposed
  // to being an error such as a certificate name mismatch, which is
  // client-only, the exact index of the SSL connect end depends on how
  // quickly the test server closes the underlying socket. If the test server
  // closes before the IO message loop pumps messages, there may be a 0-byte
  // Read event in the NetLog due to TCPClientSocket picking up the EOF. As a
  // result, the SSL connect end event will be the second-to-last entry,
  // rather than the last entry.
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLog::TYPE_SSL_CONNECT) ||
              LogContainsEndEvent(entries, -2, NetLog::TYPE_SSL_CONNECT));
}

// When creating an SSLClientSocket, it is allowed to pass in a
// ClientSocketHandle that is not obtained from a client socket pool.
// Here we verify that such a simple ClientSocketHandle, not associated with any
// client socket pool, can be destroyed safely.
TEST_F(SSLClientSocketTest, ClientSocketHandleNotFromPool) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle());
  socket_handle->SetSocket(transport.Pass());

  scoped_ptr<SSLClientSocket> sock(socket_factory_->CreateSSLClientSocket(
      socket_handle.Pass(), test_server.host_port_pair(), SSLConfig(),
      context_));

  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

// Verifies that SSLClientSocket::ExportKeyingMaterial return a success
// code and different keying label results in different keying material.
TEST_F(SSLClientSocketTest, ExportKeyingMaterial) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;

  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const int kKeyingMaterialSize = 32;
  const char kKeyingLabel1[] = "client-socket-test-1";
  const char kKeyingContext1[] = "";
  unsigned char client_out1[kKeyingMaterialSize];
  memset(client_out1, 0, sizeof(client_out1));
  rv = sock->ExportKeyingMaterial(kKeyingLabel1, false, kKeyingContext1,
                                  client_out1, sizeof(client_out1));
  EXPECT_EQ(rv, OK);

  const char kKeyingLabel2[] = "client-socket-test-2";
  unsigned char client_out2[kKeyingMaterialSize];
  memset(client_out2, 0, sizeof(client_out2));
  rv = sock->ExportKeyingMaterial(kKeyingLabel2, false, kKeyingContext1,
                                  client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, OK);
  EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);

  const char kKeyingContext2[] = "context";
  rv = sock->ExportKeyingMaterial(kKeyingLabel1, true, kKeyingContext2,
                                  client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, OK);
  EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);

  // Using an empty context should give different key material from not using a
  // context at all.
  memset(client_out2, 0, sizeof(client_out2));
  rv = sock->ExportKeyingMaterial(kKeyingLabel1, true, kKeyingContext1,
                                  client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, OK);
  EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);
}

// Verifies that SSLClientSocket::ClearSessionCache can be called without
// explicit NSS initialization.
TEST(SSLClientSocket, ClearSessionCache) {
  SSLClientSocket::ClearSessionCache();
}

TEST(SSLClientSocket, SerializeNextProtos) {
  NextProtoVector next_protos;
  next_protos.push_back(kProtoHTTP11);
  next_protos.push_back(kProtoSPDY31);
  static std::vector<uint8_t> serialized =
      SSLClientSocket::SerializeNextProtos(next_protos, true);
  ASSERT_EQ(18u, serialized.size());
  EXPECT_EQ(8, serialized[0]);  // length("http/1.1")
  EXPECT_EQ('h', serialized[1]);
  EXPECT_EQ('t', serialized[2]);
  EXPECT_EQ('t', serialized[3]);
  EXPECT_EQ('p', serialized[4]);
  EXPECT_EQ('/', serialized[5]);
  EXPECT_EQ('1', serialized[6]);
  EXPECT_EQ('.', serialized[7]);
  EXPECT_EQ('1', serialized[8]);
  EXPECT_EQ(8, serialized[9]);  // length("spdy/3.1")
  EXPECT_EQ('s', serialized[10]);
  EXPECT_EQ('p', serialized[11]);
  EXPECT_EQ('d', serialized[12]);
  EXPECT_EQ('y', serialized[13]);
  EXPECT_EQ('/', serialized[14]);
  EXPECT_EQ('3', serialized[15]);
  EXPECT_EQ('.', serialized[16]);
  EXPECT_EQ('1', serialized[17]);
}

// Test that the server certificates are properly retrieved from the underlying
// SSL stack.
TEST_F(SSLClientSocketTest, VerifyServerChainProperlyOrdered) {
  // The connection does not have to be successful.
  cert_verifier_->set_default_result(ERR_CERT_INVALID);

  // Set up a test server with CERT_CHAIN_WRONG_ROOT.
  // This makes the server present redundant-server-chain.pem, which contains
  // intermediate certificates.
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_CHAIN_WRONG_ROOT);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  rv = callback.GetResult(rv);
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));
  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());
  rv = callback.GetResult(rv);

  EXPECT_EQ(ERR_CERT_INVALID, rv);
  EXPECT_TRUE(sock->IsConnected());

  // When given option CERT_CHAIN_WRONG_ROOT, SpawnedTestServer will present
  // certs from redundant-server-chain.pem.
  CertificateList server_certs =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "redundant-server-chain.pem",
                                    X509Certificate::FORMAT_AUTO);

  // Get the server certificate as received client side.
  SSLInfo ssl_info;
  ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
  scoped_refptr<X509Certificate> server_certificate = ssl_info.unverified_cert;

  // Get the intermediates as received  client side.
  const X509Certificate::OSCertHandles& server_intermediates =
      server_certificate->GetIntermediateCertificates();

  // Check that the unverified server certificate chain is properly retrieved
  // from the underlying ssl stack.
  ASSERT_EQ(4U, server_certs.size());

  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      server_certificate->os_cert_handle(), server_certs[0]->os_cert_handle()));

  ASSERT_EQ(3U, server_intermediates.size());

  EXPECT_TRUE(X509Certificate::IsSameOSCert(server_intermediates[0],
                                            server_certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(server_intermediates[1],
                                            server_certs[2]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(server_intermediates[2],
                                            server_certs[3]->os_cert_handle()));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// This tests that SSLInfo contains a properly re-constructed certificate
// chain. That, in turn, verifies that GetSSLInfo is giving us the chain as
// verified, not the chain as served by the server. (They may be different.)
//
// CERT_CHAIN_WRONG_ROOT is redundant-server-chain.pem. It contains A
// (end-entity) -> B -> C, and C is signed by D. redundant-validated-chain.pem
// contains a chain of A -> B -> C2, where C2 is the same public key as C, but
// a self-signed root. Such a situation can occur when a new root (C2) is
// cross-certified by an old root (D) and has two different versions of its
// floating around. Servers may supply C2 as an intermediate, but the
// SSLClientSocket should return the chain that was verified, from
// verify_result, instead.
TEST_F(SSLClientSocketTest, VerifyReturnChainProperlyOrdered) {
  // By default, cause the CertVerifier to treat all certificates as
  // expired.
  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  CertificateList unverified_certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "redundant-server-chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(4u, unverified_certs.size());

  // We will expect SSLInfo to ultimately contain this chain.
  CertificateList certs =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "redundant-validated-chain.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles temp_intermediates;
  temp_intermediates.push_back(certs[1]->os_cert_handle());
  temp_intermediates.push_back(certs[2]->os_cert_handle());

  CertVerifyResult verify_result;
  verify_result.verified_cert = X509Certificate::CreateFromHandle(
      certs[0]->os_cert_handle(), temp_intermediates);

  // Add a rule that maps the server cert (A) to the chain of A->B->C2
  // rather than A->B->C.
  cert_verifier_->AddResultForCert(certs[0].get(), verify_result, OK);

  // Load and install the root for the validated chain.
  scoped_refptr<X509Certificate> root_cert = ImportCertFromFile(
      GetTestCertsDirectory(), "redundant-validated-chain-root.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert.get());
  ScopedTestRoot scoped_root(root_cert.get());

  // Set up a test server with CERT_CHAIN_WRONG_ROOT.
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_CHAIN_WRONG_ROOT);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));
  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());

  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLog::TYPE_SSL_CONNECT));

  SSLInfo ssl_info;
  sock->GetSSLInfo(&ssl_info);

  // Verify that SSLInfo contains the corrected re-constructed chain A -> B
  // -> C2.
  const X509Certificate::OSCertHandles& intermediates =
      ssl_info.cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(ssl_info.cert->os_cert_handle(),
                                            certs[0]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(intermediates[1],
                                            certs[2]->os_cert_handle()));

  // Verify that SSLInfo also contains the chain as received from the server.
  const X509Certificate::OSCertHandles& served_intermediates =
      ssl_info.unverified_cert->GetIntermediateCertificates();
  ASSERT_EQ(3U, served_intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      ssl_info.cert->os_cert_handle(), unverified_certs[0]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      served_intermediates[0], unverified_certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      served_intermediates[1], unverified_certs[2]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      served_intermediates[2], unverified_certs[3]->os_cert_handle()));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketCertRequestInfoTest, NoAuthorities) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest(ssl_options);
  ASSERT_TRUE(request_info.get());
  EXPECT_EQ(0u, request_info->cert_authorities.size());
}

TEST_F(SSLClientSocketCertRequestInfoTest, TwoAuthorities) {
  const base::FilePath::CharType kThawteFile[] =
      FILE_PATH_LITERAL("thawte.single.pem");
  const unsigned char kThawteDN[] = {
      0x30, 0x4c, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x5a, 0x41, 0x31, 0x25, 0x30, 0x23, 0x06, 0x03, 0x55, 0x04, 0x0a,
      0x13, 0x1c, 0x54, 0x68, 0x61, 0x77, 0x74, 0x65, 0x20, 0x43, 0x6f, 0x6e,
      0x73, 0x75, 0x6c, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x28, 0x50, 0x74, 0x79,
      0x29, 0x20, 0x4c, 0x74, 0x64, 0x2e, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03,
      0x55, 0x04, 0x03, 0x13, 0x0d, 0x54, 0x68, 0x61, 0x77, 0x74, 0x65, 0x20,
      0x53, 0x47, 0x43, 0x20, 0x43, 0x41};
  const size_t kThawteLen = sizeof(kThawteDN);

  const base::FilePath::CharType kDiginotarFile[] =
      FILE_PATH_LITERAL("diginotar_root_ca.pem");
  const unsigned char kDiginotarDN[] = {
      0x30, 0x5f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x4e, 0x4c, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0a,
      0x13, 0x09, 0x44, 0x69, 0x67, 0x69, 0x4e, 0x6f, 0x74, 0x61, 0x72, 0x31,
      0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x11, 0x44, 0x69,
      0x67, 0x69, 0x4e, 0x6f, 0x74, 0x61, 0x72, 0x20, 0x52, 0x6f, 0x6f, 0x74,
      0x20, 0x43, 0x41, 0x31, 0x20, 0x30, 0x1e, 0x06, 0x09, 0x2a, 0x86, 0x48,
      0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01, 0x16, 0x11, 0x69, 0x6e, 0x66, 0x6f,
      0x40, 0x64, 0x69, 0x67, 0x69, 0x6e, 0x6f, 0x74, 0x61, 0x72, 0x2e, 0x6e,
      0x6c};
  const size_t kDiginotarLen = sizeof(kDiginotarDN);

  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ssl_options.client_authorities.push_back(
      GetTestClientCertsDirectory().Append(kThawteFile));
  ssl_options.client_authorities.push_back(
      GetTestClientCertsDirectory().Append(kDiginotarFile));
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest(ssl_options);
  ASSERT_TRUE(request_info.get());
  ASSERT_EQ(2u, request_info->cert_authorities.size());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kThawteDN), kThawteLen),
            request_info->cert_authorities[0]);
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(kDiginotarDN), kDiginotarLen),
      request_info->cert_authorities[1]);
}

// cert_key_types is currently only populated on OpenSSL.
#if defined(USE_OPENSSL)
TEST_F(SSLClientSocketCertRequestInfoTest, CertKeyTypes) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ssl_options.client_cert_types.push_back(CLIENT_CERT_RSA_SIGN);
  ssl_options.client_cert_types.push_back(CLIENT_CERT_ECDSA_SIGN);
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest(ssl_options);
  ASSERT_TRUE(request_info.get());
  ASSERT_EQ(2u, request_info->cert_key_types.size());
  EXPECT_EQ(CLIENT_CERT_RSA_SIGN, request_info->cert_key_types[0]);
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, request_info->cert_key_types[1]);
}
#endif  // defined(USE_OPENSSL)

TEST_F(SSLClientSocketTest, ConnectSignedCertTimestampsEnabledTLSExtension) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.signed_cert_timestamps_tls_ext = "test";

  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                ssl_options,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log_, NetLog::Source()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  ssl_config.signed_cert_timestamps_enabled = true;

  MockCTVerifier ct_verifier;
  SetCTVerifier(&ct_verifier);

  // Check that the SCT list is extracted as expected.
  EXPECT_CALL(ct_verifier, Verify(_, "", "test", _, _)).WillRepeatedly(
      Return(ERR_CT_NO_SCTS_VERIFIED_OK));

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));
  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(sock->signed_cert_timestamps_received_);
}

namespace {

bool IsValidOCSPResponse(const base::StringPiece& input) {
  base::StringPiece ocsp_response = input;
  base::StringPiece sequence, response_status, response_bytes;
  return asn1::GetElement(&ocsp_response, asn1::kSEQUENCE, &sequence) &&
      ocsp_response.empty() &&
      asn1::GetElement(&sequence, asn1::kENUMERATED, &response_status) &&
      asn1::GetElement(&sequence,
                       asn1::kContextSpecific | asn1::kConstructed | 0,
                       &response_status) &&
      sequence.empty();
}

}  // namespace

// Test that enabling Signed Certificate Timestamps enables OCSP stapling.
TEST_F(SSLClientSocketTest, ConnectSignedCertTimestampsEnabledOCSP) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.staple_ocsp_response = true;
  // The test server currently only knows how to generate OCSP responses
  // for a freshly minted certificate.
  ssl_options.server_certificate = SpawnedTestServer::SSLOptions::CERT_AUTO;

  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                ssl_options,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log_, NetLog::Source()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  // Enabling Signed Cert Timestamps ensures we request OCSP stapling for
  // Certificate Transparency verification regardless of whether the platform
  // is able to process the OCSP status itself.
  ssl_config.signed_cert_timestamps_enabled = true;

  MockCTVerifier ct_verifier;
  SetCTVerifier(&ct_verifier);

  // Check that the OCSP response is extracted and well-formed. It should be the
  // DER encoding of an OCSPResponse (RFC 2560), so check that it consists of a
  // SEQUENCE of an ENUMERATED type and an element tagged with [0] EXPLICIT. In
  // particular, it should not include the overall two-byte length prefix from
  // TLS.
  EXPECT_CALL(ct_verifier,
              Verify(_, Truly(IsValidOCSPResponse), "", _, _)).WillRepeatedly(
                  Return(ERR_CT_NO_SCTS_VERIFIED_OK));

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));
  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(sock->stapled_ocsp_response_received_);
}

TEST_F(SSLClientSocketTest, ConnectSignedCertTimestampsDisabled) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.signed_cert_timestamps_tls_ext = "test";

  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                ssl_options,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log_, NetLog::Source()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  ssl_config.signed_cert_timestamps_enabled = false;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));
  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  EXPECT_FALSE(sock->signed_cert_timestamps_received_);
}

// Tests that IsConnectedAndIdle and WasEverUsed behave as expected.
TEST_F(SSLClientSocketTest, ReuseStates) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // The socket was just connected. It should be idle because it is speaking
  // HTTP. Although the transport has been used for the handshake, WasEverUsed()
  // returns false.
  EXPECT_TRUE(sock->IsConnected());
  EXPECT_TRUE(sock->IsConnectedAndIdle());
  EXPECT_FALSE(sock->WasEverUsed());

  const char kRequestText[] = "GET / HTTP/1.0\r\n\r\n";
  const size_t kRequestLen = arraysize(kRequestText) - 1;
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestLen));
  memcpy(request_buffer->data(), kRequestText, kRequestLen);

  rv = sock->Write(request_buffer.get(), kRequestLen, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(kRequestLen), rv);

  // The socket has now been used.
  EXPECT_TRUE(sock->WasEverUsed());

  // TODO(davidben): Read one byte to ensure the test server has responded and
  // then assert IsConnectedAndIdle is false. This currently doesn't work
  // because neither SSLClientSocketNSS nor SSLClientSocketOpenSSL check their
  // SSL implementation's internal buffers. Either call PR_Available and
  // SSL_pending, although the former isn't actually implemented or perhaps
  // attempt to read one byte extra.
}

// Tests that IsConnectedAndIdle treats a socket as idle even if a Write hasn't
// been flushed completely out of SSLClientSocket's internal buffers. This is a
// regression test for https://crbug.com/466147.
TEST_F(SSLClientSocketTest, ReusableAfterWrite) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(real_transport.Pass()));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), SSLConfig()));
  ASSERT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));

  // Block any application data from reaching the network.
  raw_transport->BlockWrite();

  // Write a partial HTTP request.
  const char kRequestText[] = "GET / HTTP/1.0";
  const size_t kRequestLen = arraysize(kRequestText) - 1;
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestLen));
  memcpy(request_buffer->data(), kRequestText, kRequestLen);

  // Although transport writes are blocked, both SSLClientSocketOpenSSL and
  // SSLClientSocketNSS complete the outer Write operation.
  EXPECT_EQ(static_cast<int>(kRequestLen),
            callback.GetResult(sock->Write(request_buffer.get(), kRequestLen,
                                           callback.callback())));

  // The Write operation is complete, so the socket should be treated as
  // reusable, in case the server returns an HTTP response before completely
  // consuming the request body. In this case, we assume the server will
  // properly drain the request body before trying to read the next request.
  EXPECT_TRUE(sock->IsConnectedAndIdle());
}

// Tests that basic session resumption works.
TEST_F(SSLClientSocketTest, SessionResumption) {
  SpawnedTestServer::SSLOptions ssl_options;
  ASSERT_TRUE(StartTestServer(ssl_options));

  // First, perform a full handshake.
  SSLConfig ssl_config;
  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), ssl_config));
  ASSERT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // The next connection should resume.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  ASSERT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);

  // Using a different HostPortPair uses a different session cache key.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               HostPortPair("example.com", 443), ssl_config);
  ASSERT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  SSLClientSocket::ClearSessionCache();

  // After clearing the session cache, the next handshake doesn't resume.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  ASSERT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

// Tests that connections with certificate errors do not add entries to the
// session cache.
TEST_F(SSLClientSocketTest, CertificateErrorNoResume) {
  SpawnedTestServer::SSLOptions ssl_options;
  ASSERT_TRUE(StartTestServer(ssl_options));

  cert_verifier_->set_default_result(ERR_CERT_COMMON_NAME_INVALID);

  SSLConfig ssl_config;
  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), ssl_config));
  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID,
            callback.GetResult(sock->Connect(callback.callback())));

  cert_verifier_->set_default_result(OK);

  // The next connection should perform a full handshake.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  ASSERT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

// Tests that session caches are sharded by max_version.
TEST_F(SSLClientSocketTest, FallbackShardSessionCache) {
  SpawnedTestServer::SSLOptions ssl_options;
  ASSERT_TRUE(StartTestServer(ssl_options));

  // Prepare a normal and fallback SSL config.
  SSLConfig ssl_config;
  SSLConfig fallback_ssl_config;
  fallback_ssl_config.version_max = SSL_PROTOCOL_VERSION_TLS1;
  fallback_ssl_config.version_fallback_min = SSL_PROTOCOL_VERSION_TLS1;
  fallback_ssl_config.version_fallback = true;

  // Connect with a fallback config from the test server to add an entry to the
  // session cache.
  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), fallback_ssl_config));
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  SSLInfo ssl_info;
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
  EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1,
            SSLConnectionStatusToVersion(ssl_info.connection_status));

  // A non-fallback connection needs a full handshake.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
  // This does not check for equality because TLS 1.2 support is conditional on
  // system NSS features.
  EXPECT_LT(SSL_CONNECTION_VERSION_TLS1,
            SSLConnectionStatusToVersion(ssl_info.connection_status));

  // Note: if the server (correctly) declines to resume a TLS 1.0 session at TLS
  // 1.2, the above test would not be sufficient to prove the session caches are
  // sharded. Implementations vary here, so, to avoid being sensitive to this,
  // attempt to resume with two more connections.

  // The non-fallback connection added a > TLS 1.0 entry to the session cache.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  // This does not check for equality because TLS 1.2 support is conditional on
  // system NSS features.
  EXPECT_LT(SSL_CONNECTION_VERSION_TLS1,
            SSLConnectionStatusToVersion(ssl_info.connection_status));

  // The fallback connection still resumes from its session cache. It cannot
  // offer the > TLS 1.0 session, so this must have been the session from the
  // first fallback connection.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), fallback_ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1,
            SSLConnectionStatusToVersion(ssl_info.connection_status));
}

// Test that RC4 is only enabled if enable_deprecated_cipher_suites is set.
TEST_F(SSLClientSocketTest, DeprecatedRC4) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.bulk_ciphers = SpawnedTestServer::SSLOptions::BULK_CIPHER_RC4;
  ASSERT_TRUE(StartTestServer(ssl_options));

  // Normal handshakes with RC4 do not work.
  SSLConfig ssl_config;
  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), ssl_config));
  ASSERT_EQ(ERR_SSL_VERSION_OR_CIPHER_MISMATCH,
            callback.GetResult(sock->Connect(callback.callback())));

  // Enabling deprecated ciphers works fine.
  ssl_config.enable_deprecated_cipher_suites = true;
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  ASSERT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  ASSERT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
}

// Tests that enabling deprecated ciphers shards the session cache.
TEST_F(SSLClientSocketTest, DeprecatedShardSessionCache) {
  SpawnedTestServer::SSLOptions ssl_options;
  ASSERT_TRUE(StartTestServer(ssl_options));

  // Prepare a normal and deprecated SSL config.
  SSLConfig ssl_config;
  SSLConfig deprecated_ssl_config;
  deprecated_ssl_config.enable_deprecated_cipher_suites = true;

  // Connect with deprecated ciphers enabled to warm the session cache cache.
  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(transport.Pass(), test_server()->host_port_pair(),
                            deprecated_ssl_config));
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  SSLInfo ssl_info;
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // Test that re-connecting with deprecated ciphers enabled still resumes.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), deprecated_ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);

  // However, a normal connection needs a full handshake.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // Clear the session cache for the inverse test.
  SSLClientSocket::ClearSessionCache();

  // Now make a normal connection to prime the session cache.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // A normal connection should be able to resume.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(transport.Pass(),
                               test_server()->host_port_pair(), ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);

  // However, enabling deprecated ciphers connects fresh.
  transport.reset(new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport->Connect(callback.callback())));
  sock = CreateSSLClientSocket(
      transport.Pass(), test_server()->host_port_pair(), deprecated_ssl_config);
  EXPECT_EQ(OK, callback.GetResult(sock->Connect(callback.callback())));
  EXPECT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

TEST_F(SSLClientSocketTest, RequireECDHE) {
  // Run test server without ECDHE.
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.key_exchanges = SpawnedTestServer::SSLOptions::KEY_EXCHANGE_RSA;
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS, ssl_options,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  TestNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  rv = callback.GetResult(rv);
  EXPECT_EQ(OK, rv);

  SSLConfig config;
  config.require_ecdhe = true;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), config));

  rv = sock->Connect(callback.callback());
  rv = callback.GetResult(rv);

  EXPECT_EQ(ERR_SSL_VERSION_OR_CIPHER_MISMATCH, rv);
}

TEST_F(SSLClientSocketFalseStartTest, FalseStartEnabled) {
  if (!SupportsAESGCM()) {
    LOG(WARNING) << "Skipping test because AES-GCM is not supported.";
    return;
  }

  // False Start requires NPN/ALPN, ECDHE, and an AEAD.
  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_ECDHE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128GCM;
  server_options.enable_npn = true;
  SSLConfig client_config;
  client_config.next_protos.push_back(kProtoHTTP11);
  ASSERT_NO_FATAL_FAILURE(
      TestFalseStart(server_options, client_config, true));
}

// Test that False Start is disabled without NPN.
TEST_F(SSLClientSocketFalseStartTest, NoNPN) {
  if (!SupportsAESGCM()) {
    LOG(WARNING) << "Skipping test because AES-GCM is not supported.";
    return;
  }

  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_ECDHE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128GCM;
  SSLConfig client_config;
  client_config.next_protos.clear();
  ASSERT_NO_FATAL_FAILURE(
      TestFalseStart(server_options, client_config, false));
}

// Test that False Start is disabled with plain RSA ciphers.
TEST_F(SSLClientSocketFalseStartTest, RSA) {
  if (!SupportsAESGCM()) {
    LOG(WARNING) << "Skipping test because AES-GCM is not supported.";
    return;
  }

  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128GCM;
  server_options.enable_npn = true;
  SSLConfig client_config;
  client_config.next_protos.push_back(kProtoHTTP11);
  ASSERT_NO_FATAL_FAILURE(
      TestFalseStart(server_options, client_config, false));
}

// Test that False Start is disabled with DHE_RSA ciphers.
TEST_F(SSLClientSocketFalseStartTest, DHE_RSA) {
  if (!SupportsAESGCM()) {
    LOG(WARNING) << "Skipping test because AES-GCM is not supported.";
    return;
  }

  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_DHE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128GCM;
  server_options.enable_npn = true;
  SSLConfig client_config;
  client_config.next_protos.push_back(kProtoHTTP11);
  ASSERT_NO_FATAL_FAILURE(TestFalseStart(server_options, client_config, false));
}

// Test that False Start is disabled without an AEAD.
TEST_F(SSLClientSocketFalseStartTest, NoAEAD) {
  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_ECDHE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128;
  server_options.enable_npn = true;
  SSLConfig client_config;
  client_config.next_protos.push_back(kProtoHTTP11);
  ASSERT_NO_FATAL_FAILURE(TestFalseStart(server_options, client_config, false));
}

// Test that sessions are resumable after receiving the server Finished message.
TEST_F(SSLClientSocketFalseStartTest, SessionResumption) {
  if (!SupportsAESGCM()) {
    LOG(WARNING) << "Skipping test because AES-GCM is not supported.";
    return;
  }

  // Start a server.
  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_ECDHE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128GCM;
  server_options.enable_npn = true;
  SSLConfig client_config;
  client_config.next_protos.push_back(kProtoHTTP11);

  // Let a full handshake complete with False Start.
  ASSERT_NO_FATAL_FAILURE(
      TestFalseStart(server_options, client_config, true));

  // Make a second connection.
  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport2(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport2->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock2 = CreateSSLClientSocket(
      transport2.Pass(), test_server()->host_port_pair(), client_config);
  ASSERT_TRUE(sock2.get());
  EXPECT_EQ(OK, callback.GetResult(sock2->Connect(callback.callback())));

  // It should resume the session.
  SSLInfo ssl_info;
  EXPECT_TRUE(sock2->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// Test that False Started sessions are not resumable before receiving the
// server Finished message.
TEST_F(SSLClientSocketFalseStartTest, NoSessionResumptionBeforeFinished) {
  if (!SupportsAESGCM()) {
    LOG(WARNING) << "Skipping test because AES-GCM is not supported.";
    return;
  }

  // Start a server.
  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_ECDHE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128GCM;
  server_options.enable_npn = true;
  ASSERT_TRUE(StartTestServer(server_options));

  SSLConfig client_config;
  client_config.next_protos.push_back(kProtoHTTP11);

  // Start a handshake up to the server Finished message.
  TestCompletionCallback callback;
  FakeBlockingStreamSocket* raw_transport1 = NULL;
  scoped_ptr<SSLClientSocket> sock1;
  ASSERT_NO_FATAL_FAILURE(CreateAndConnectUntilServerFinishedReceived(
      client_config, &callback, &raw_transport1, &sock1));
  // Although raw_transport1 has the server Finished blocked, the handshake
  // still completes.
  EXPECT_EQ(OK, callback.WaitForResult());

  // Continue to block the client (|sock1|) from processing the Finished
  // message, but allow it to arrive on the socket. This ensures that, from the
  // server's point of view, it has completed the handshake and added the
  // session to its session cache.
  //
  // The actual read on |sock1| will not complete until the Finished message is
  // processed; however, pump the underlying transport so that it is read from
  // the socket. NOTE: This may flakily pass if the server's final flight
  // doesn't come in one Read.
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  int rv = sock1->Read(buf.get(), 4096, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  raw_transport1->WaitForReadResult();

  // Drop the old socket. This is needed because the Python test server can't
  // service two sockets in parallel.
  sock1.reset();

  // Start a second connection.
  scoped_ptr<StreamSocket> transport2(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport2->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock2 = CreateSSLClientSocket(
      transport2.Pass(), test_server()->host_port_pair(), client_config);
  EXPECT_EQ(OK, callback.GetResult(sock2->Connect(callback.callback())));

  // No session resumption because the first connection never received a server
  // Finished message.
  SSLInfo ssl_info;
  EXPECT_TRUE(sock2->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

// Test that False Started sessions are not resumable if the server Finished
// message was bad.
TEST_F(SSLClientSocketFalseStartTest, NoSessionResumptionBadFinished) {
  if (!SupportsAESGCM()) {
    LOG(WARNING) << "Skipping test because AES-GCM is not supported.";
    return;
  }

  // Start a server.
  SpawnedTestServer::SSLOptions server_options;
  server_options.key_exchanges =
      SpawnedTestServer::SSLOptions::KEY_EXCHANGE_ECDHE_RSA;
  server_options.bulk_ciphers =
      SpawnedTestServer::SSLOptions::BULK_CIPHER_AES128GCM;
  server_options.enable_npn = true;
  ASSERT_TRUE(StartTestServer(server_options));

  SSLConfig client_config;
  client_config.next_protos.push_back(kProtoHTTP11);

  // Start a handshake up to the server Finished message.
  TestCompletionCallback callback;
  FakeBlockingStreamSocket* raw_transport1 = NULL;
  scoped_ptr<SSLClientSocket> sock1;
  ASSERT_NO_FATAL_FAILURE(CreateAndConnectUntilServerFinishedReceived(
      client_config, &callback, &raw_transport1, &sock1));
  // Although raw_transport1 has the server Finished blocked, the handshake
  // still completes.
  EXPECT_EQ(OK, callback.WaitForResult());

  // Continue to block the client (|sock1|) from processing the Finished
  // message, but allow it to arrive on the socket. This ensures that, from the
  // server's point of view, it has completed the handshake and added the
  // session to its session cache.
  //
  // The actual read on |sock1| will not complete until the Finished message is
  // processed; however, pump the underlying transport so that it is read from
  // the socket.
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  int rv = sock1->Read(buf.get(), 4096, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  raw_transport1->WaitForReadResult();

  // The server's second leg, or part of it, is now received but not yet sent to
  // |sock1|. Before doing so, break the server's second leg.
  int bytes_read = raw_transport1->pending_read_result();
  ASSERT_LT(0, bytes_read);
  raw_transport1->pending_read_buf()->data()[bytes_read - 1]++;

  // Unblock the Finished message. |sock1->Read| should now fail.
  raw_transport1->UnblockReadResult();
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, callback.GetResult(rv));

  // Drop the old socket. This is needed because the Python test server can't
  // service two sockets in parallel.
  sock1.reset();

  // Start a second connection.
  scoped_ptr<StreamSocket> transport2(
      new TCPClientSocket(addr(), &log_, NetLog::Source()));
  EXPECT_EQ(OK, callback.GetResult(transport2->Connect(callback.callback())));
  scoped_ptr<SSLClientSocket> sock2 = CreateSSLClientSocket(
      transport2.Pass(), test_server()->host_port_pair(), client_config);
  EXPECT_EQ(OK, callback.GetResult(sock2->Connect(callback.callback())));

  // No session resumption because the first connection never received a server
  // Finished message.
  SSLInfo ssl_info;
  EXPECT_TRUE(sock2->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

// Connect to a server using channel id. It should allow the connection.
TEST_F(SSLClientSocketChannelIDTest, SendChannelID) {
  SpawnedTestServer::SSLOptions ssl_options;

  ASSERT_TRUE(ConnectToTestServer(ssl_options));

  EnableChannelID();
  SSLConfig ssl_config;
  ssl_config.channel_id_enabled = true;

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock_->IsConnected());
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.channel_id_sent);

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}

// Connect to a server using Channel ID but failing to look up the Channel
// ID. It should fail.
TEST_F(SSLClientSocketChannelIDTest, FailingChannelID) {
  SpawnedTestServer::SSLOptions ssl_options;

  ASSERT_TRUE(ConnectToTestServer(ssl_options));

  EnableFailingChannelID();
  SSLConfig ssl_config;
  ssl_config.channel_id_enabled = true;

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));

  // TODO(haavardm@opera.com): Due to differences in threading, Linux returns
  // ERR_UNEXPECTED while Mac and Windows return ERR_PROTOCOL_ERROR. Accept all
  // error codes for now.
  // http://crbug.com/373670
  EXPECT_NE(OK, rv);
  EXPECT_FALSE(sock_->IsConnected());
}

// Connect to a server using Channel ID but asynchronously failing to look up
// the Channel ID. It should fail.
TEST_F(SSLClientSocketChannelIDTest, FailingChannelIDAsync) {
  SpawnedTestServer::SSLOptions ssl_options;

  ASSERT_TRUE(ConnectToTestServer(ssl_options));

  EnableAsyncFailingChannelID();
  SSLConfig ssl_config;
  ssl_config.channel_id_enabled = true;

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));

  EXPECT_EQ(ERR_UNEXPECTED, rv);
  EXPECT_FALSE(sock_->IsConnected());
}

}  // namespace net
