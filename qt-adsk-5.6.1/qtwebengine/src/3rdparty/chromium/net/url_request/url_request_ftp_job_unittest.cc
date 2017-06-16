// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_ftp_job.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/ftp/ftp_auth_cache.h"
#include "net/http/http_transaction_test_util.h"
#include "net/proxy/mock_proxy_resolver.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace net {
namespace {

class MockProxyResolverFactory : public ProxyResolverFactory {
 public:
  MockProxyResolverFactory() : ProxyResolverFactory(false) {}
  int CreateProxyResolver(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      scoped_ptr<ProxyResolver>* resolver,
      const CompletionCallback& callback,
      scoped_ptr<Request>* request) override {
    resolver->reset(new MockAsyncProxyResolver());
    return OK;
  }
};

}  // namespace

class FtpTestURLRequestContext : public TestURLRequestContext {
 public:
  FtpTestURLRequestContext(ClientSocketFactory* socket_factory,
                           ProxyService* proxy_service,
                           NetworkDelegate* network_delegate,
                           FtpTransactionFactory* ftp_transaction_factory)
      : TestURLRequestContext(true),
        ftp_protocol_handler_(new FtpProtocolHandler(ftp_transaction_factory)) {
    set_client_socket_factory(socket_factory);
    context_storage_.set_proxy_service(proxy_service);
    set_network_delegate(network_delegate);
    URLRequestJobFactoryImpl* job_factory = new URLRequestJobFactoryImpl;
    job_factory->SetProtocolHandler("ftp", ftp_protocol_handler_);
    context_storage_.set_job_factory(job_factory);
    Init();
  }

  FtpAuthCache* GetFtpAuthCache() {
    return ftp_protocol_handler_->ftp_auth_cache_.get();
  }

  void set_proxy_service(ProxyService* proxy_service) {
    context_storage_.set_proxy_service(proxy_service);
  }

 private:
  FtpProtocolHandler* ftp_protocol_handler_;
};

namespace {

class SimpleProxyConfigService : public ProxyConfigService {
 public:
  SimpleProxyConfigService() {
    // Any FTP requests that ever go through HTTP paths are proxied requests.
    config_.proxy_rules().ParseFromString("ftp=localhost");
  }

  void AddObserver(Observer* observer) override { observer_ = observer; }

  void RemoveObserver(Observer* observer) override {
    if (observer_ == observer) {
      observer_ = NULL;
    }
  }

  ConfigAvailability GetLatestProxyConfig(ProxyConfig* config) override {
    *config = config_;
    return CONFIG_VALID;
  }

  void IncrementConfigId() {
    config_.set_id(config_.id() + 1);
    observer_->OnProxyConfigChanged(config_, ProxyConfigService::CONFIG_VALID);
  }

 private:
  ProxyConfig config_;
  Observer* observer_;
};

// Inherit from URLRequestFtpJob to expose the priority and some
// other hidden functions.
class TestURLRequestFtpJob : public URLRequestFtpJob {
 public:
  TestURLRequestFtpJob(URLRequest* request,
                       FtpTransactionFactory* ftp_factory,
                       FtpAuthCache* ftp_auth_cache)
      : URLRequestFtpJob(request, NULL, ftp_factory, ftp_auth_cache) {}

  using URLRequestFtpJob::SetPriority;
  using URLRequestFtpJob::Start;
  using URLRequestFtpJob::Kill;
  using URLRequestFtpJob::priority;

 protected:
  ~TestURLRequestFtpJob() override {}
};

class MockFtpTransactionFactory : public FtpTransactionFactory {
 public:
  FtpTransaction* CreateTransaction() override { return NULL; }

  void Suspend(bool suspend) override {}
};

// Fixture for priority-related tests. Priority matters when there is
// an HTTP proxy.
class URLRequestFtpJobPriorityTest : public testing::Test {
 protected:
  URLRequestFtpJobPriorityTest()
      : proxy_service_(new SimpleProxyConfigService, NULL, NULL),
        req_(context_.CreateRequest(GURL("ftp://ftp.example.com"),
                                    DEFAULT_PRIORITY,
                                    &delegate_)) {
    context_.set_proxy_service(&proxy_service_);
    context_.set_http_transaction_factory(&network_layer_);
  }

  ProxyService proxy_service_;
  MockNetworkLayer network_layer_;
  MockFtpTransactionFactory ftp_factory_;
  FtpAuthCache ftp_auth_cache_;
  TestURLRequestContext context_;
  TestDelegate delegate_;
  scoped_ptr<URLRequest> req_;
};

// Make sure that SetPriority actually sets the URLRequestFtpJob's
// priority, both before and after start.
TEST_F(URLRequestFtpJobPriorityTest, SetPriorityBasic) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(
      req_.get(), &ftp_factory_, &ftp_auth_cache_));
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  job->SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, job->priority());

  job->SetPriority(LOW);
  EXPECT_EQ(LOW, job->priority());

  job->Start();
  EXPECT_EQ(LOW, job->priority());

  job->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job->priority());
}

// Make sure that URLRequestFtpJob passes on its priority to its
// transaction on start.
TEST_F(URLRequestFtpJobPriorityTest, SetTransactionPriorityOnStart) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(
      req_.get(), &ftp_factory_, &ftp_auth_cache_));
  job->SetPriority(LOW);

  EXPECT_FALSE(network_layer_.last_transaction());

  job->Start();

  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestFtpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestFtpJobPriorityTest, SetTransactionPriority) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(
      req_.get(), &ftp_factory_, &ftp_auth_cache_));
  job->SetPriority(LOW);
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestFtpJob passes on its priority updates to
// newly-created transactions after the first one.
TEST_F(URLRequestFtpJobPriorityTest, SetSubsequentTransactionPriority) {
  scoped_refptr<TestURLRequestFtpJob> job(new TestURLRequestFtpJob(
      req_.get(), &ftp_factory_, &ftp_auth_cache_));
  job->Start();

  job->SetPriority(LOW);
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->Kill();
  network_layer_.ClearLastTransaction();

  // Creates a second transaction.
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

class URLRequestFtpJobTest : public testing::Test {
 public:
  URLRequestFtpJobTest()
      : request_context_(&socket_factory_,
                         new ProxyService(
                             new SimpleProxyConfigService, NULL, NULL),
                         &network_delegate_,
                         &ftp_transaction_factory_) {
  }

  ~URLRequestFtpJobTest() override {
    // Clean up any remaining tasks that mess up unrelated tests.
    base::RunLoop().RunUntilIdle();
  }

  void AddSocket(MockRead* reads, size_t reads_size,
                 MockWrite* writes, size_t writes_size) {
    SequencedSocketData* socket_data =
        new SequencedSocketData(reads, reads_size, writes, writes_size);
    socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    socket_factory_.AddSocketDataProvider(socket_data);

    socket_data_.push_back(socket_data);
  }

  FtpTestURLRequestContext* request_context() { return &request_context_; }
  TestNetworkDelegate* network_delegate() { return &network_delegate_; }

 private:
  ScopedVector<SequencedSocketData> socket_data_;
  MockClientSocketFactory socket_factory_;
  TestNetworkDelegate network_delegate_;
  MockFtpTransactionFactory ftp_transaction_factory_;

  FtpTestURLRequestContext request_context_;
};

TEST_F(URLRequestFtpJobTest, FtpProxyRequest) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 3, "test.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  ASSERT_TRUE(url_request->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_TRUE(url_request->proxy_server().Equals(
      HostPortPair::FromString("localhost:80")));
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate.auth_required_called());
  EXPECT_EQ("test.html", request_delegate.data_received());
}

// Regression test for http://crbug.com/237526 .
TEST_F(URLRequestFtpJobTest, FtpProxyRequestOrphanJob) {
  // Use a PAC URL so that URLRequestFtpJob's |pac_request_| field is non-NULL.
  request_context()->set_proxy_service(new ProxyService(
      new ProxyConfigServiceFixed(
          ProxyConfig::CreateFromCustomPacURL(GURL("http://foo"))),
      make_scoped_ptr(new MockProxyResolverFactory), NULL));

  TestDelegate request_delegate;
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();

  // Now |url_request| will be deleted before its completion,
  // resulting in it being orphaned. It should not crash.
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestNeedProxyAuthNoCredentials) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead(ASYNC, 2, "Proxy-Authenticate: Basic "
             "realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  ASSERT_TRUE(url_request->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_TRUE(url_request->proxy_server().Equals(
      HostPortPair::FromString("localhost:80")));
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_TRUE(request_delegate.auth_required_called());
  EXPECT_EQ("test.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestNeedProxyAuthWithCredentials) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 5, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic bXl1c2VyOm15cGFzcw==\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead(ASYNC, 2, "Proxy-Authenticate: Basic "
             "realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),

    // Second response.
    MockRead(ASYNC, 6, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 7, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 8, "test2.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  request_delegate.set_credentials(
      AuthCredentials(ASCIIToUTF16("myuser"), ASCIIToUTF16("mypass")));
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  ASSERT_TRUE(url_request->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_TRUE(request_delegate.auth_required_called());
  EXPECT_EQ("test2.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestNeedServerAuthNoCredentials) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 401 Unauthorized\r\n"),
    MockRead(ASYNC, 2, "WWW-Authenticate: Basic "
             "realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  ASSERT_TRUE(url_request->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_TRUE(request_delegate.auth_required_called());
  EXPECT_EQ("test.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestNeedServerAuthWithCredentials) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 5, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Authorization: Basic bXl1c2VyOm15cGFzcw==\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 401 Unauthorized\r\n"),
    MockRead(ASYNC, 2, "WWW-Authenticate: Basic "
             "realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),

    // Second response.
    MockRead(ASYNC, 6, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 7, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 8, "test2.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  request_delegate.set_credentials(
      AuthCredentials(ASCIIToUTF16("myuser"), ASCIIToUTF16("mypass")));
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  ASSERT_TRUE(url_request->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_TRUE(request_delegate.auth_required_called());
  EXPECT_EQ("test2.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestNeedProxyAndServerAuth) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 5, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic "
              "cHJveHl1c2VyOnByb3h5cGFzcw==\r\n\r\n"),
    MockWrite(ASYNC, 10, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic "
              "cHJveHl1c2VyOnByb3h5cGFzcw==\r\n"
              "Authorization: Basic bXl1c2VyOm15cGFzcw==\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(ASYNC, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead(ASYNC, 2, "Proxy-Authenticate: Basic "
             "realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 3, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),

    // Second response.
    MockRead(ASYNC, 6, "HTTP/1.1 401 Unauthorized\r\n"),
    MockRead(ASYNC, 7, "WWW-Authenticate: Basic "
             "realm=\"MyRealm1\"\r\n"),
    MockRead(ASYNC, 8, "Content-Length: 9\r\n\r\n"),
    MockRead(ASYNC, 9, "test.html"),

    // Third response.
    MockRead(ASYNC, 11, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 12, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 13, "test2.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  GURL url("ftp://ftp.example.com");

  // Make sure cached FTP credentials are not used for proxy authentication.
  request_context()->GetFtpAuthCache()->Add(
      url.GetOrigin(),
      AuthCredentials(ASCIIToUTF16("userdonotuse"),
                      ASCIIToUTF16("passworddonotuse")));

  TestDelegate request_delegate;
  request_delegate.set_quit_on_auth_required(true);
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      url, DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  ASSERT_TRUE(url_request->is_pending());

  // Run until proxy auth is requested.
  base::RunLoop().Run();

  ASSERT_TRUE(request_delegate.auth_required_called());
  EXPECT_EQ(0, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  url_request->SetAuth(
      AuthCredentials(ASCIIToUTF16("proxyuser"), ASCIIToUTF16("proxypass")));

  // Run until server auth is requested.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_EQ(0, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  url_request->SetAuth(
      AuthCredentials(ASCIIToUTF16("myuser"), ASCIIToUTF16("mypass")));

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_TRUE(request_delegate.auth_required_called());
  EXPECT_EQ("test2.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestDoNotSaveCookies) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 9\r\n"),
    MockRead(ASYNC, 3, "Set-Cookie: name=value\r\n\r\n"),
    MockRead(ASYNC, 4, "test.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  ASSERT_TRUE(url_request->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request->status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());

  // Make sure we do not accept cookies.
  EXPECT_EQ(0, network_delegate()->set_cookie_count());

  EXPECT_FALSE(request_delegate.auth_required_called());
  EXPECT_EQ("test.html", request_delegate.data_received());
}

TEST_F(URLRequestFtpJobTest, FtpProxyRequestDoNotFollowRedirects) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "GET ftp://ftp.example.com/ HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 1, "HTTP/1.1 302 Found\r\n"),
    MockRead(ASYNC, 2, "Location: http://other.example.com/\r\n\r\n"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate;
  scoped_ptr<URLRequest> url_request(request_context()->CreateRequest(
      GURL("ftp://ftp.example.com/"), DEFAULT_PRIORITY, &request_delegate));
  url_request->Start();
  EXPECT_TRUE(url_request->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->error_count());
  EXPECT_FALSE(url_request->status().is_success());
  EXPECT_EQ(ERR_UNSAFE_REDIRECT, url_request->status().error());
}

// We should re-use socket for requests using the same scheme, host, and port.
TEST_F(URLRequestFtpJobTest, FtpProxyRequestReuseSocket) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/first HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
    MockWrite(ASYNC, 4, "GET ftp://ftp.example.com/second HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 3, "test1.html"),
    MockRead(ASYNC, 5, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 6, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 7, "test2.html"),
  };

  AddSocket(reads, arraysize(reads), writes, arraysize(writes));

  TestDelegate request_delegate1;

  scoped_ptr<URLRequest> url_request1(
      request_context()->CreateRequest(GURL("ftp://ftp.example.com/first"),
                                       DEFAULT_PRIORITY, &request_delegate1));
  url_request1->Start();
  ASSERT_TRUE(url_request1->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request1->status().is_success());
  EXPECT_TRUE(url_request1->proxy_server().Equals(
      HostPortPair::FromString("localhost:80")));
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate1.auth_required_called());
  EXPECT_EQ("test1.html", request_delegate1.data_received());

  TestDelegate request_delegate2;
  scoped_ptr<URLRequest> url_request2(
      request_context()->CreateRequest(GURL("ftp://ftp.example.com/second"),
                                       DEFAULT_PRIORITY, &request_delegate2));
  url_request2->Start();
  ASSERT_TRUE(url_request2->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request2->status().is_success());
  EXPECT_EQ(2, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate2.auth_required_called());
  EXPECT_EQ("test2.html", request_delegate2.data_received());
}

// We should not re-use socket when there are two requests to the same host,
// but one is FTP and the other is HTTP.
TEST_F(URLRequestFtpJobTest, FtpProxyRequestDoNotReuseSocket) {
  MockWrite writes1[] = {
    MockWrite(ASYNC, 0, "GET ftp://ftp.example.com/first HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockWrite writes2[] = {
    MockWrite(ASYNC, 0, "GET /second HTTP/1.1\r\n"
              "Host: ftp.example.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent:\r\n"
              "Accept-Encoding: gzip, deflate\r\n"
              "Accept-Language: en-us,fr\r\n\r\n"),
  };
  MockRead reads1[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 3, "test1.html"),
  };
  MockRead reads2[] = {
    MockRead(ASYNC, 1, "HTTP/1.1 200 OK\r\n"),
    MockRead(ASYNC, 2, "Content-Length: 10\r\n\r\n"),
    MockRead(ASYNC, 3, "test2.html"),
  };

  AddSocket(reads1, arraysize(reads1), writes1, arraysize(writes1));
  AddSocket(reads2, arraysize(reads2), writes2, arraysize(writes2));

  TestDelegate request_delegate1;
  scoped_ptr<URLRequest> url_request1(
      request_context()->CreateRequest(GURL("ftp://ftp.example.com/first"),
                                       DEFAULT_PRIORITY, &request_delegate1));
  url_request1->Start();
  ASSERT_TRUE(url_request1->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request1->status().is_success());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate1.auth_required_called());
  EXPECT_EQ("test1.html", request_delegate1.data_received());

  TestDelegate request_delegate2;
  scoped_ptr<URLRequest> url_request2(
      request_context()->CreateRequest(GURL("http://ftp.example.com/second"),
                                       DEFAULT_PRIORITY, &request_delegate2));
  url_request2->Start();
  ASSERT_TRUE(url_request2->is_pending());

  // The TestDelegate will by default quit the message loop on completion.
  base::RunLoop().Run();

  EXPECT_TRUE(url_request2->status().is_success());
  EXPECT_EQ(2, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
  EXPECT_FALSE(request_delegate2.auth_required_called());
  EXPECT_EQ("test2.html", request_delegate2.data_received());
}

}  // namespace

}  // namespace net
