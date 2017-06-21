// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test suite uses SSLClientSocket to test the implementation of
// SSLServerSocket. In order to establish connections between the sockets
// we need two additional classes:
// 1. FakeSocket
//    Connects SSL socket to FakeDataChannel. This class is just a stub.
//
// 2. FakeDataChannel
//    Implements the actual exchange of data between two FakeSockets.
//
// Implementations of these two classes are included in this file.

#include "net/socket/ssl_server_socket.h"

#include <stdlib.h>

#include <queue>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "crypto/nss_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(USE_OPENSSL)
#include <pk11pub.h>

#if !defined(CKM_AES_GCM)
#define CKM_AES_GCM 0x00001087
#endif
#if !defined(CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256)
#define CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256 (CKM_NSS + 24)
#endif
#endif

namespace net {

namespace {

class FakeDataChannel {
 public:
  FakeDataChannel()
      : read_buf_len_(0),
        closed_(false),
        write_called_after_close_(false),
        weak_factory_(this) {
  }

  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
    DCHECK(read_callback_.is_null());
    DCHECK(!read_buf_.get());
    if (closed_)
      return 0;
    if (data_.empty()) {
      read_callback_ = callback;
      read_buf_ = buf;
      read_buf_len_ = buf_len;
      return ERR_IO_PENDING;
    }
    return PropogateData(buf, buf_len);
  }

  int Write(IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
    DCHECK(write_callback_.is_null());
    if (closed_) {
      if (write_called_after_close_)
        return ERR_CONNECTION_RESET;
      write_called_after_close_ = true;
      write_callback_ = callback;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&FakeDataChannel::DoWriteCallback,
                                weak_factory_.GetWeakPtr()));
      return ERR_IO_PENDING;
    }
    // This function returns synchronously, so make a copy of the buffer.
    data_.push(new DrainableIOBuffer(
        new StringIOBuffer(std::string(buf->data(), buf_len)),
        buf_len));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&FakeDataChannel::DoReadCallback,
                              weak_factory_.GetWeakPtr()));
    return buf_len;
  }

  // Closes the FakeDataChannel. After Close() is called, Read() returns 0,
  // indicating EOF, and Write() fails with ERR_CONNECTION_RESET. Note that
  // after the FakeDataChannel is closed, the first Write() call completes
  // asynchronously, which is necessary to reproduce bug 127822.
  void Close() {
    closed_ = true;
  }

 private:
  void DoReadCallback() {
    if (read_callback_.is_null() || data_.empty())
      return;

    int copied = PropogateData(read_buf_, read_buf_len_);
    CompletionCallback callback = read_callback_;
    read_callback_.Reset();
    read_buf_ = NULL;
    read_buf_len_ = 0;
    callback.Run(copied);
  }

  void DoWriteCallback() {
    if (write_callback_.is_null())
      return;

    CompletionCallback callback = write_callback_;
    write_callback_.Reset();
    callback.Run(ERR_CONNECTION_RESET);
  }

  int PropogateData(scoped_refptr<IOBuffer> read_buf, int read_buf_len) {
    scoped_refptr<DrainableIOBuffer> buf = data_.front();
    int copied = std::min(buf->BytesRemaining(), read_buf_len);
    memcpy(read_buf->data(), buf->data(), copied);
    buf->DidConsume(copied);

    if (!buf->BytesRemaining())
      data_.pop();
    return copied;
  }

  CompletionCallback read_callback_;
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;

  CompletionCallback write_callback_;

  std::queue<scoped_refptr<DrainableIOBuffer> > data_;

  // True if Close() has been called.
  bool closed_;

  // Controls the completion of Write() after the FakeDataChannel is closed.
  // After the FakeDataChannel is closed, the first Write() call completes
  // asynchronously.
  bool write_called_after_close_;

  base::WeakPtrFactory<FakeDataChannel> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDataChannel);
};

class FakeSocket : public StreamSocket {
 public:
  FakeSocket(FakeDataChannel* incoming_channel,
             FakeDataChannel* outgoing_channel)
      : incoming_(incoming_channel),
        outgoing_(outgoing_channel) {
  }

  ~FakeSocket() override {}

  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override {
    // Read random number of bytes.
    buf_len = rand() % buf_len + 1;
    return incoming_->Read(buf, buf_len, callback);
  }

  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override {
    // Write random number of bytes.
    buf_len = rand() % buf_len + 1;
    return outgoing_->Write(buf, buf_len, callback);
  }

  int SetReceiveBufferSize(int32 size) override { return OK; }

  int SetSendBufferSize(int32 size) override { return OK; }

  int Connect(const CompletionCallback& callback) override { return OK; }

  void Disconnect() override {
    incoming_->Close();
    outgoing_->Close();
  }

  bool IsConnected() const override { return true; }

  bool IsConnectedAndIdle() const override { return true; }

  int GetPeerAddress(IPEndPoint* address) const override {
    IPAddressNumber ip_address(kIPv4AddressSize);
    *address = IPEndPoint(ip_address, 0 /*port*/);
    return OK;
  }

  int GetLocalAddress(IPEndPoint* address) const override {
    IPAddressNumber ip_address(4);
    *address = IPEndPoint(ip_address, 0);
    return OK;
  }

  const BoundNetLog& NetLog() const override { return net_log_; }

  void SetSubresourceSpeculation() override {}
  void SetOmniboxSpeculation() override {}

  bool WasEverUsed() const override { return true; }

  bool UsingTCPFastOpen() const override { return false; }

  bool WasNpnNegotiated() const override { return false; }

  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }

  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }

  void GetConnectionAttempts(ConnectionAttempts* out) const override {
    out->clear();
  }

  void ClearConnectionAttempts() override {}

  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}

 private:
  BoundNetLog net_log_;
  FakeDataChannel* incoming_;
  FakeDataChannel* outgoing_;

  DISALLOW_COPY_AND_ASSIGN(FakeSocket);
};

}  // namespace

// Verify the correctness of the test helper classes first.
TEST(FakeSocketTest, DataTransfer) {
  // Establish channels between two sockets.
  FakeDataChannel channel_1;
  FakeDataChannel channel_2;
  FakeSocket client(&channel_1, &channel_2);
  FakeSocket server(&channel_2, &channel_1);

  const char kTestData[] = "testing123";
  const int kTestDataSize = strlen(kTestData);
  const int kReadBufSize = 1024;
  scoped_refptr<IOBuffer> write_buf = new StringIOBuffer(kTestData);
  scoped_refptr<IOBuffer> read_buf = new IOBuffer(kReadBufSize);

  // Write then read.
  int written =
      server.Write(write_buf.get(), kTestDataSize, CompletionCallback());
  EXPECT_GT(written, 0);
  EXPECT_LE(written, kTestDataSize);

  int read = client.Read(read_buf.get(), kReadBufSize, CompletionCallback());
  EXPECT_GT(read, 0);
  EXPECT_LE(read, written);
  EXPECT_EQ(0, memcmp(kTestData, read_buf->data(), read));

  // Read then write.
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server.Read(read_buf.get(), kReadBufSize, callback.callback()));

  written = client.Write(write_buf.get(), kTestDataSize, CompletionCallback());
  EXPECT_GT(written, 0);
  EXPECT_LE(written, kTestDataSize);

  read = callback.WaitForResult();
  EXPECT_GT(read, 0);
  EXPECT_LE(read, written);
  EXPECT_EQ(0, memcmp(kTestData, read_buf->data(), read));
}

class SSLServerSocketTest : public PlatformTest {
 public:
  SSLServerSocketTest()
      : socket_factory_(ClientSocketFactory::GetDefaultFactory()),
        cert_verifier_(new MockCertVerifier()),
        transport_security_state_(new TransportSecurityState) {
    cert_verifier_->set_default_result(CERT_STATUS_AUTHORITY_INVALID);
  }

 protected:
  void Initialize() {
    scoped_ptr<ClientSocketHandle> client_connection(new ClientSocketHandle);
    client_connection->SetSocket(
        scoped_ptr<StreamSocket>(new FakeSocket(&channel_1_, &channel_2_)));
    scoped_ptr<StreamSocket> server_socket(
        new FakeSocket(&channel_2_, &channel_1_));

    base::FilePath certs_dir(GetTestCertsDirectory());

    base::FilePath cert_path = certs_dir.AppendASCII("unittest.selfsigned.der");
    std::string cert_der;
    ASSERT_TRUE(base::ReadFileToString(cert_path, &cert_der));

    scoped_refptr<X509Certificate> cert =
        X509Certificate::CreateFromBytes(cert_der.data(), cert_der.size());

    base::FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
    std::string key_string;
    ASSERT_TRUE(base::ReadFileToString(key_path, &key_string));
    std::vector<uint8> key_vector(
        reinterpret_cast<const uint8*>(key_string.data()),
        reinterpret_cast<const uint8*>(key_string.data() +
                                       key_string.length()));

    scoped_ptr<crypto::RSAPrivateKey> private_key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));

    client_ssl_config_.false_start_enabled = false;
    client_ssl_config_.channel_id_enabled = false;

    // Certificate provided by the host doesn't need authority.
    SSLConfig::CertAndStatus cert_and_status;
    cert_and_status.cert_status = CERT_STATUS_AUTHORITY_INVALID;
    cert_and_status.der_cert = cert_der;
    client_ssl_config_.allowed_bad_certs.push_back(cert_and_status);

    HostPortPair host_and_pair("unittest", 0);
    SSLClientSocketContext context;
    context.cert_verifier = cert_verifier_.get();
    context.transport_security_state = transport_security_state_.get();
    client_socket_ = socket_factory_->CreateSSLClientSocket(
        client_connection.Pass(), host_and_pair, client_ssl_config_, context);
    server_socket_ =
        CreateSSLServerSocket(server_socket.Pass(), cert.get(),
                              private_key.get(), server_ssl_config_);
  }

  FakeDataChannel channel_1_;
  FakeDataChannel channel_2_;
  SSLConfig client_ssl_config_;
  SSLConfig server_ssl_config_;
  scoped_ptr<SSLClientSocket> client_socket_;
  scoped_ptr<SSLServerSocket> server_socket_;
  ClientSocketFactory* socket_factory_;
  scoped_ptr<MockCertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
};

// This test only executes creation of client and server sockets. This is to
// test that creation of sockets doesn't crash and have minimal code to run
// under valgrind in order to help debugging memory problems.
TEST_F(SSLServerSocketTest, Initialize) {
  Initialize();
}

// This test executes Connect() on SSLClientSocket and Handshake() on
// SSLServerSocket to make sure handshaking between the two sockets is
// completed successfully.
TEST_F(SSLServerSocketTest, Handshake) {
  Initialize();

  TestCompletionCallback connect_callback;
  TestCompletionCallback handshake_callback;

  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  EXPECT_TRUE(server_ret == OK || server_ret == ERR_IO_PENDING);

  int client_ret = client_socket_->Connect(connect_callback.callback());
  EXPECT_TRUE(client_ret == OK || client_ret == ERR_IO_PENDING);

  if (client_ret == ERR_IO_PENDING) {
    EXPECT_EQ(OK, connect_callback.WaitForResult());
  }
  if (server_ret == ERR_IO_PENDING) {
    EXPECT_EQ(OK, handshake_callback.WaitForResult());
  }

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, ssl_info.cert_status);

  // The default cipher suite should be ECDHE and, unless on NSS and the
  // platform doesn't support it, an AEAD.
  uint16_t cipher_suite =
      SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead, cipher_suite);
  EXPECT_STREQ("ECDHE_RSA", key_exchange);
#if defined(USE_OPENSSL)
  bool supports_aead = true;
#else
  bool supports_aead =
      PK11_TokenExists(CKM_AES_GCM) &&
      PK11_TokenExists(CKM_NSS_TLS_MASTER_KEY_DERIVE_DH_SHA256);
#endif
  EXPECT_TRUE(!supports_aead || is_aead);
}

TEST_F(SSLServerSocketTest, DataTransfer) {
  Initialize();

  TestCompletionCallback connect_callback;
  TestCompletionCallback handshake_callback;

  // Establish connection.
  int client_ret = client_socket_->Connect(connect_callback.callback());
  ASSERT_TRUE(client_ret == OK || client_ret == ERR_IO_PENDING);

  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_TRUE(server_ret == OK || server_ret == ERR_IO_PENDING);

  client_ret = connect_callback.GetResult(client_ret);
  ASSERT_EQ(OK, client_ret);
  server_ret = handshake_callback.GetResult(server_ret);
  ASSERT_EQ(OK, server_ret);

  const int kReadBufSize = 1024;
  scoped_refptr<StringIOBuffer> write_buf =
      new StringIOBuffer("testing123");
  scoped_refptr<DrainableIOBuffer> read_buf =
      new DrainableIOBuffer(new IOBuffer(kReadBufSize), kReadBufSize);

  // Write then read.
  TestCompletionCallback write_callback;
  TestCompletionCallback read_callback;
  server_ret = server_socket_->Write(
      write_buf.get(), write_buf->size(), write_callback.callback());
  EXPECT_TRUE(server_ret > 0 || server_ret == ERR_IO_PENDING);
  client_ret = client_socket_->Read(
      read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
  EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);

  server_ret = write_callback.GetResult(server_ret);
  EXPECT_GT(server_ret, 0);
  client_ret = read_callback.GetResult(client_ret);
  ASSERT_GT(client_ret, 0);

  read_buf->DidConsume(client_ret);
  while (read_buf->BytesConsumed() < write_buf->size()) {
    client_ret = client_socket_->Read(
        read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
    EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);
    client_ret = read_callback.GetResult(client_ret);
    ASSERT_GT(client_ret, 0);
    read_buf->DidConsume(client_ret);
  }
  EXPECT_EQ(write_buf->size(), read_buf->BytesConsumed());
  read_buf->SetOffset(0);
  EXPECT_EQ(0, memcmp(write_buf->data(), read_buf->data(), write_buf->size()));

  // Read then write.
  write_buf = new StringIOBuffer("hello123");
  server_ret = server_socket_->Read(
      read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
  EXPECT_TRUE(server_ret > 0 || server_ret == ERR_IO_PENDING);
  client_ret = client_socket_->Write(
      write_buf.get(), write_buf->size(), write_callback.callback());
  EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);

  server_ret = read_callback.GetResult(server_ret);
  ASSERT_GT(server_ret, 0);
  client_ret = write_callback.GetResult(client_ret);
  EXPECT_GT(client_ret, 0);

  read_buf->DidConsume(server_ret);
  while (read_buf->BytesConsumed() < write_buf->size()) {
    server_ret = server_socket_->Read(
        read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
    EXPECT_TRUE(server_ret > 0 || server_ret == ERR_IO_PENDING);
    server_ret = read_callback.GetResult(server_ret);
    ASSERT_GT(server_ret, 0);
    read_buf->DidConsume(server_ret);
  }
  EXPECT_EQ(write_buf->size(), read_buf->BytesConsumed());
  read_buf->SetOffset(0);
  EXPECT_EQ(0, memcmp(write_buf->data(), read_buf->data(), write_buf->size()));
}

// A regression test for bug 127822 (http://crbug.com/127822).
// If the server closes the connection after the handshake is finished,
// the client's Write() call should not cause an infinite loop.
// NOTE: this is a test for SSLClientSocket rather than SSLServerSocket.
TEST_F(SSLServerSocketTest, ClientWriteAfterServerClose) {
  Initialize();

  TestCompletionCallback connect_callback;
  TestCompletionCallback handshake_callback;

  // Establish connection.
  int client_ret = client_socket_->Connect(connect_callback.callback());
  ASSERT_TRUE(client_ret == OK || client_ret == ERR_IO_PENDING);

  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_TRUE(server_ret == OK || server_ret == ERR_IO_PENDING);

  client_ret = connect_callback.GetResult(client_ret);
  ASSERT_EQ(OK, client_ret);
  server_ret = handshake_callback.GetResult(server_ret);
  ASSERT_EQ(OK, server_ret);

  scoped_refptr<StringIOBuffer> write_buf = new StringIOBuffer("testing123");

  // The server closes the connection. The server needs to write some
  // data first so that the client's Read() calls from the transport
  // socket won't return ERR_IO_PENDING.  This ensures that the client
  // will call Read() on the transport socket again.
  TestCompletionCallback write_callback;

  server_ret = server_socket_->Write(
      write_buf.get(), write_buf->size(), write_callback.callback());
  EXPECT_TRUE(server_ret > 0 || server_ret == ERR_IO_PENDING);

  server_ret = write_callback.GetResult(server_ret);
  EXPECT_GT(server_ret, 0);

  server_socket_->Disconnect();

  // The client writes some data. This should not cause an infinite loop.
  client_ret = client_socket_->Write(
      write_buf.get(), write_buf->size(), write_callback.callback());
  EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);

  client_ret = write_callback.GetResult(client_ret);
  EXPECT_GT(client_ret, 0);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(10));
  base::MessageLoop::current()->Run();
}

// This test executes ExportKeyingMaterial() on the client and server sockets,
// after connecting them, and verifies that the results match.
// This test will fail if False Start is enabled (see crbug.com/90208).
TEST_F(SSLServerSocketTest, ExportKeyingMaterial) {
  Initialize();

  TestCompletionCallback connect_callback;
  TestCompletionCallback handshake_callback;

  int client_ret = client_socket_->Connect(connect_callback.callback());
  ASSERT_TRUE(client_ret == OK || client_ret == ERR_IO_PENDING);

  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_TRUE(server_ret == OK || server_ret == ERR_IO_PENDING);

  if (client_ret == ERR_IO_PENDING) {
    ASSERT_EQ(OK, connect_callback.WaitForResult());
  }
  if (server_ret == ERR_IO_PENDING) {
    ASSERT_EQ(OK, handshake_callback.WaitForResult());
  }

  const int kKeyingMaterialSize = 32;
  const char kKeyingLabel[] = "EXPERIMENTAL-server-socket-test";
  const char kKeyingContext[] = "";
  unsigned char server_out[kKeyingMaterialSize];
  int rv = server_socket_->ExportKeyingMaterial(kKeyingLabel,
                                                false, kKeyingContext,
                                                server_out, sizeof(server_out));
  ASSERT_EQ(OK, rv);

  unsigned char client_out[kKeyingMaterialSize];
  rv = client_socket_->ExportKeyingMaterial(kKeyingLabel,
                                            false, kKeyingContext,
                                            client_out, sizeof(client_out));
  ASSERT_EQ(OK, rv);
  EXPECT_EQ(0, memcmp(server_out, client_out, sizeof(server_out)));

  const char kKeyingLabelBad[] = "EXPERIMENTAL-server-socket-test-bad";
  unsigned char client_bad[kKeyingMaterialSize];
  rv = client_socket_->ExportKeyingMaterial(kKeyingLabelBad,
                                            false, kKeyingContext,
                                            client_bad, sizeof(client_bad));
  ASSERT_EQ(rv, OK);
  EXPECT_NE(0, memcmp(server_out, client_bad, sizeof(server_out)));
}

// Verifies that SSLConfig::require_ecdhe flags works properly.
TEST_F(SSLServerSocketTest, RequireEcdheFlag) {
  // Disable all ECDHE suites on the client side.
  uint16_t kEcdheCiphers[] = {
      0xc007,  // ECDHE_ECDSA_WITH_RC4_128_SHA
      0xc009,  // ECDHE_ECDSA_WITH_AES_128_CBC_SHA
      0xc00a,  // ECDHE_ECDSA_WITH_AES_256_CBC_SHA
      0xc011,  // ECDHE_RSA_WITH_RC4_128_SHA
      0xc013,  // ECDHE_RSA_WITH_AES_128_CBC_SHA
      0xc014,  // ECDHE_RSA_WITH_AES_256_CBC_SHA
      0xc02b,  // ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
      0xc02f,  // ECDHE_RSA_WITH_AES_128_GCM_SHA256
      0xcc13,  // ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
      0xcc14,  // ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
  };
  client_ssl_config_.disabled_cipher_suites.assign(
      kEcdheCiphers, kEcdheCiphers + arraysize(kEcdheCiphers));

  // Require ECDHE on the server.
  server_ssl_config_.require_ecdhe = true;

  Initialize();

  TestCompletionCallback connect_callback;
  TestCompletionCallback handshake_callback;

  int client_ret = client_socket_->Connect(connect_callback.callback());
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_EQ(ERR_SSL_VERSION_OR_CIPHER_MISMATCH, client_ret);
  ASSERT_EQ(ERR_SSL_VERSION_OR_CIPHER_MISMATCH, server_ret);
}

}  // namespace net
