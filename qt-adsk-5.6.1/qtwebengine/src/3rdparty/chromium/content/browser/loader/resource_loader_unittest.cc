// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_loader.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/loader/redirect_to_file_resource_handler.h"
#include "content/browser/loader/resource_loader_delegate.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/mock_file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::ShareableFileReference;

namespace content {
namespace {

// Stub client certificate store that returns a preset list of certificates for
// each request and records the arguments of the most recent request for later
// inspection.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  // Creates a new ClientCertStoreStub that returns |response| on query. It
  // saves the number of requests and most recently certificate authorities list
  // in |requested_authorities| and |request_count|, respectively. The caller is
  // responsible for ensuring those pointers outlive the ClientCertStoreStub.
  //
  // TODO(ppi): Make the stub independent from the internal representation of
  // SSLCertRequestInfo. For now it seems that we can neither save the
  // scoped_refptr<> (since it is never passed to us) nor copy the entire
  // CertificateRequestInfo (since there is no copy constructor).
  ClientCertStoreStub(const net::CertificateList& response,
                      int* request_count,
                      std::vector<std::string>* requested_authorities)
      : response_(response),
        requested_authorities_(requested_authorities),
        request_count_(request_count) {
    requested_authorities_->clear();
    *request_count_ = 0;
  }

  ~ClientCertStoreStub() override {}

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      net::CertificateList* selected_certs,
                      const base::Closure& callback) override {
    *requested_authorities_ = cert_request_info.cert_authorities;
    ++(*request_count_);

    *selected_certs = response_;
    callback.Run();
  }

 private:
  const net::CertificateList response_;
  std::vector<std::string>* requested_authorities_;
  int* request_count_;
};

// Client certificate store which destroys its resource loader before the
// asynchronous GetClientCerts callback is called.
class LoaderDestroyingCertStore : public net::ClientCertStore {
 public:
  // Creates a client certificate store which, when looked up, posts a task to
  // reset |loader| and then call the callback. The caller is responsible for
  // ensuring the pointers remain valid until the process is complete.
  explicit LoaderDestroyingCertStore(scoped_ptr<ResourceLoader>* loader)
      : loader_(loader) {}

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      net::CertificateList* selected_certs,
                      const base::Closure& callback) override {
    // Don't destroy |loader_| while it's on the stack.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&LoaderDestroyingCertStore::DoCallback,
                              base::Unretained(loader_), callback));
  }

 private:
  static void DoCallback(scoped_ptr<ResourceLoader>* loader,
                         const base::Closure& callback) {
    loader->reset();
    callback.Run();
  }

  scoped_ptr<ResourceLoader>* loader_;
};

// A mock URLRequestJob which simulates an SSL client auth request.
class MockClientCertURLRequestJob : public net::URLRequestTestJob {
 public:
  MockClientCertURLRequestJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate) {}

  static std::vector<std::string> test_authorities() {
    return std::vector<std::string>(1, "dummy");
  }

  // net::URLRequestTestJob:
  void Start() override {
    scoped_refptr<net::SSLCertRequestInfo> cert_request_info(
        new net::SSLCertRequestInfo);
    cert_request_info->cert_authorities = test_authorities();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&MockClientCertURLRequestJob::NotifyCertificateRequested,
                   this, cert_request_info));
  }

  void ContinueWithCertificate(net::X509Certificate* cert) override {
    net::URLRequestTestJob::Start();
  }

 private:
  ~MockClientCertURLRequestJob() override {}

  DISALLOW_COPY_AND_ASSIGN(MockClientCertURLRequestJob);
};

class MockClientCertJobProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // URLRequestJobFactory::ProtocolHandler implementation:
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new MockClientCertURLRequestJob(request, network_delegate);
  }
};

// Arbitrary read buffer size.
const int kReadBufSize = 1024;

// Dummy implementation of ResourceHandler, instance of which is needed to
// initialize ResourceLoader.
class ResourceHandlerStub : public ResourceHandler {
 public:
  explicit ResourceHandlerStub(net::URLRequest* request)
      : ResourceHandler(request),
        read_buffer_(new net::IOBuffer(kReadBufSize)),
        defer_request_on_will_start_(false),
        expect_reads_(true),
        cancel_on_read_completed_(false),
        defer_eof_(false),
        received_on_will_read_(false),
        received_eof_(false),
        received_response_completed_(false),
        total_bytes_downloaded_(0),
        upload_position_(0) {
  }

  // If true, defers the resource load in OnWillStart.
  void set_defer_request_on_will_start(bool defer_request_on_will_start) {
    defer_request_on_will_start_ = defer_request_on_will_start;
  }

  // If true, expect OnWillRead / OnReadCompleted pairs for handling
  // data. Otherwise, expect OnDataDownloaded.
  void set_expect_reads(bool expect_reads) { expect_reads_ = expect_reads; }

  // If true, cancel the request in OnReadCompleted by returning false.
  void set_cancel_on_read_completed(bool cancel_on_read_completed) {
    cancel_on_read_completed_ = cancel_on_read_completed;
  }

  // If true, cancel the request in OnReadCompleted by returning false.
  void set_defer_eof(bool defer_eof) { defer_eof_ = defer_eof; }

  const GURL& start_url() const { return start_url_; }
  ResourceResponse* response() const { return response_.get(); }
  bool received_response_completed() const {
    return received_response_completed_;
  }
  const net::URLRequestStatus& status() const { return status_; }
  int total_bytes_downloaded() const { return total_bytes_downloaded_; }

  void Resume() {
    controller()->Resume();
  }

  // Waits until OnUploadProgress is called and returns the upload position.
  uint64 WaitForUploadProgress() {
    wait_for_progress_loop_.reset(new base::RunLoop());
    wait_for_progress_loop_->Run();
    wait_for_progress_loop_.reset();
    return upload_position_;
  }

  // ResourceHandler implementation:
  bool OnUploadProgress(uint64 position, uint64 size) override {
    EXPECT_LE(position, size);
    EXPECT_GT(position, upload_position_);
    upload_position_ = position;
    if (wait_for_progress_loop_)
      wait_for_progress_loop_->Quit();
    return true;
  }

  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override {
    NOTREACHED();
    return true;
  }

  bool OnResponseStarted(ResourceResponse* response, bool* defer) override {
    EXPECT_FALSE(response_.get());
    response_ = response;
    return true;
  }

  bool OnWillStart(const GURL& url, bool* defer) override {
    EXPECT_TRUE(start_url_.is_empty());
    start_url_ = url;
    *defer = defer_request_on_will_start_;
    return true;
  }

  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override {
    return true;
  }

  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override {
    EXPECT_TRUE(expect_reads_);
    EXPECT_FALSE(received_on_will_read_);
    EXPECT_FALSE(received_eof_);
    EXPECT_FALSE(received_response_completed_);

    *buf = read_buffer_;
    *buf_size = kReadBufSize;
    received_on_will_read_ = true;
    return true;
  }

  bool OnReadCompleted(int bytes_read, bool* defer) override {
    EXPECT_TRUE(received_on_will_read_);
    EXPECT_TRUE(expect_reads_);
    EXPECT_FALSE(received_response_completed_);

    if (bytes_read == 0) {
      received_eof_ = true;
      if (defer_eof_) {
        defer_eof_ = false;
        *defer = true;
      }
    }

    // Need another OnWillRead() call before seeing an OnReadCompleted().
    received_on_will_read_ = false;

    return !cancel_on_read_completed_;
  }

  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override {
    EXPECT_FALSE(received_response_completed_);
    if (status.is_success() && expect_reads_)
      EXPECT_TRUE(received_eof_);

    received_response_completed_ = true;
    status_ = status;
  }

  void OnDataDownloaded(int bytes_downloaded) override {
    EXPECT_FALSE(expect_reads_);
    total_bytes_downloaded_ += bytes_downloaded;
  }

 private:
  scoped_refptr<net::IOBuffer> read_buffer_;

  bool defer_request_on_will_start_;
  bool expect_reads_;
  bool cancel_on_read_completed_;
  bool defer_eof_;

  GURL start_url_;
  scoped_refptr<ResourceResponse> response_;
  bool received_on_will_read_;
  bool received_eof_;
  bool received_response_completed_;
  net::URLRequestStatus status_;
  int total_bytes_downloaded_;
  scoped_ptr<base::RunLoop> wait_for_progress_loop_;
  uint64 upload_position_;
};

// Test browser client that captures calls to SelectClientCertificates and
// records the arguments of the most recent call for later inspection.
class SelectCertificateBrowserClient : public TestContentBrowserClient {
 public:
  SelectCertificateBrowserClient() : call_count_(0) {}

  void SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      scoped_ptr<ClientCertificateDelegate> delegate) override {
    ++call_count_;
    passed_certs_ = cert_request_info->client_certs;
    delegate_ = delegate.Pass();
  }

  int call_count() { return call_count_; }
  net::CertificateList passed_certs() { return passed_certs_; }

  void ContinueWithCertificate(net::X509Certificate* cert) {
    delegate_->ContinueWithCertificate(cert);
    delegate_.reset();
  }

  void CancelCertificateSelection() { delegate_.reset(); }

 private:
  net::CertificateList passed_certs_;
  int call_count_;
  scoped_ptr<ClientCertificateDelegate> delegate_;
};

class ResourceContextStub : public MockResourceContext {
 public:
  explicit ResourceContextStub(net::URLRequestContext* test_request_context)
      : MockResourceContext(test_request_context) {}

  scoped_ptr<net::ClientCertStore> CreateClientCertStore() override {
    return dummy_cert_store_.Pass();
  }

  void SetClientCertStore(scoped_ptr<net::ClientCertStore> store) {
    dummy_cert_store_ = store.Pass();
  }

 private:
  scoped_ptr<net::ClientCertStore> dummy_cert_store_;
};

// Wraps a ChunkedUploadDataStream to behave as non-chunked to enable upload
// progress reporting.
class NonChunkedUploadDataStream : public net::UploadDataStream {
 public:
  explicit NonChunkedUploadDataStream(uint64 size)
      : net::UploadDataStream(false, 0), stream_(0), size_(size) {}

  void AppendData(const char* data) {
    stream_.AppendData(data, strlen(data), false);
  }

 private:
  int InitInternal() override {
    SetSize(size_);
    stream_.Init(base::Bind(&NonChunkedUploadDataStream::OnInitCompleted,
                            base::Unretained(this)));
    return net::OK;
  }

  int ReadInternal(net::IOBuffer* buf, int buf_len) override {
    return stream_.Read(buf, buf_len,
                        base::Bind(&NonChunkedUploadDataStream::OnReadCompleted,
                                   base::Unretained(this)));
  }

  void ResetInternal() override { stream_.Reset(); }

  net::ChunkedUploadDataStream stream_;
  uint64 size_;

  DISALLOW_COPY_AND_ASSIGN(NonChunkedUploadDataStream);
};

// Fails to create a temporary file with the given error.
void CreateTemporaryError(
    base::File::Error error,
    const CreateTemporaryFileStreamCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(scoped_ptr<net::FileStream>()),
                 scoped_refptr<ShareableFileReference>()));
}

}  // namespace

class ResourceLoaderTest : public testing::Test,
                           public ResourceLoaderDelegate {
 protected:
  ResourceLoaderTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      resource_context_(&test_url_request_context_),
      raw_ptr_resource_handler_(NULL),
      raw_ptr_to_request_(NULL) {
    test_url_request_context_.set_job_factory(&job_factory_);
  }

  GURL test_url() const { return net::URLRequestTestJob::test_url_1(); }

  std::string test_data() const {
    return net::URLRequestTestJob::test_data_1();
  }

  // Waits until upload progress reaches |target_position|
  void WaitForUploadProgress(uint64 target_position) {
    while (true) {
      uint64 position = raw_ptr_resource_handler_->WaitForUploadProgress();
      EXPECT_LE(position, target_position);
      loader_->OnUploadProgressACK();
      if (position == target_position)
        break;
    }
  }

  virtual net::URLRequestJobFactory::ProtocolHandler* CreateProtocolHandler() {
    return net::URLRequestTestJob::CreateProtocolHandler();
  }

  virtual scoped_ptr<ResourceHandler> WrapResourceHandler(
      scoped_ptr<ResourceHandlerStub> leaf_handler,
      net::URLRequest* request) {
    return leaf_handler.Pass();
  }

  // Replaces loader_ with a new one for |request|.
  void SetUpResourceLoader(scoped_ptr<net::URLRequest> request) {
    raw_ptr_to_request_ = request.get();

    RenderFrameHost* rfh = web_contents_->GetMainFrame();
    ResourceRequestInfo::AllocateForTesting(
        request.get(), RESOURCE_TYPE_MAIN_FRAME, &resource_context_,
        rfh->GetProcess()->GetID(), rfh->GetRenderViewHost()->GetRoutingID(),
        rfh->GetRoutingID(), true /* is_main_frame */,
        false /* parent_is_main_frame */, true /* allow_download */,
        false /* is_async */);
    scoped_ptr<ResourceHandlerStub> resource_handler(
        new ResourceHandlerStub(request.get()));
    raw_ptr_resource_handler_ = resource_handler.get();
    loader_.reset(new ResourceLoader(
        request.Pass(),
        WrapResourceHandler(resource_handler.Pass(), raw_ptr_to_request_),
        this));
  }

  void SetUp() override {
    job_factory_.SetProtocolHandler("test", CreateProtocolHandler());

    browser_context_.reset(new TestBrowserContext());
    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    web_contents_.reset(
        TestWebContents::Create(browser_context_.get(), site_instance.get()));

    scoped_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            test_url(),
            net::DEFAULT_PRIORITY,
            nullptr /* delegate */));
    SetUpResourceLoader(request.Pass());
  }

  void TearDown() override {
    // Destroy the WebContents and pump the event loop before destroying
    // |rvh_test_enabler_| and |thread_bundle_|. This lets asynchronous cleanup
    // tasks complete.
    web_contents_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // ResourceLoaderDelegate:
  ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(
      ResourceLoader* loader,
      net::AuthChallengeInfo* auth_info) override {
    return NULL;
  }
  bool HandleExternalProtocol(ResourceLoader* loader,
                              const GURL& url) override {
    return false;
  }
  void DidStartRequest(ResourceLoader* loader) override {}
  void DidReceiveRedirect(ResourceLoader* loader,
                          const GURL& new_url) override {}
  void DidReceiveResponse(ResourceLoader* loader) override {}
  void DidFinishLoading(ResourceLoader* loader) override {}

  TestBrowserThreadBundle thread_bundle_;
  RenderViewHostTestEnabler rvh_test_enabler_;

  net::URLRequestJobFactoryImpl job_factory_;
  net::TestURLRequestContext test_url_request_context_;
  ResourceContextStub resource_context_;
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<TestWebContents> web_contents_;

  // The ResourceLoader owns the URLRequest and the ResourceHandler.
  ResourceHandlerStub* raw_ptr_resource_handler_;
  net::URLRequest* raw_ptr_to_request_;
  scoped_ptr<ResourceLoader> loader_;
};

class ClientCertResourceLoaderTest : public ResourceLoaderTest {
 protected:
  net::URLRequestJobFactory::ProtocolHandler* CreateProtocolHandler() override {
    return new MockClientCertJobProtocolHandler;
  }
};

// Tests that client certificates are requested with ClientCertStore lookup.
TEST_F(ClientCertResourceLoaderTest, WithStoreLookup) {
  // Set up the test client cert store.
  int store_request_count;
  std::vector<std::string> store_requested_authorities;
  net::CertificateList dummy_certs(1, scoped_refptr<net::X509Certificate>(
      new net::X509Certificate("test", "test", base::Time(), base::Time())));
  scoped_ptr<ClientCertStoreStub> test_store(new ClientCertStoreStub(
      dummy_certs, &store_request_count, &store_requested_authorities));
  resource_context_.SetClientCertStore(test_store.Pass());

  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to pause.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());

  // Check if the test store was queried against correct |cert_authorities|.
  EXPECT_EQ(1, store_request_count);
  EXPECT_EQ(MockClientCertURLRequestJob::test_authorities(),
            store_requested_authorities);

  // Check if the retrieved certificates were passed to the content browser
  // client.
  EXPECT_EQ(1, test_client.call_count());
  EXPECT_EQ(dummy_certs, test_client.passed_certs());

  // Continue the request.
  test_client.ContinueWithCertificate(dummy_certs[0].get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::OK, raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Tests that client certificates are requested on a platform with NULL
// ClientCertStore.
TEST_F(ClientCertResourceLoaderTest, WithNullStore) {
  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to pause.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // Check if the SelectClientCertificate was called on the content browser
  // client.
  EXPECT_EQ(1, test_client.call_count());
  EXPECT_EQ(net::CertificateList(), test_client.passed_certs());

  // Continue the request.
  scoped_refptr<net::X509Certificate> cert(
      new net::X509Certificate("test", "test", base::Time(), base::Time()));
  test_client.ContinueWithCertificate(cert.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::OK, raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Tests that the ContentBrowserClient may cancel a certificate request.
TEST_F(ClientCertResourceLoaderTest, CancelSelection) {
  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to pause.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // Check if the SelectClientCertificate was called on the content browser
  // client.
  EXPECT_EQ(1, test_client.call_count());
  EXPECT_EQ(net::CertificateList(), test_client.passed_certs());

  // Cancel the request.
  test_client.CancelCertificateSelection();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Verifies that requests without WebContents attached abort.
TEST_F(ClientCertResourceLoaderTest, NoWebContents) {
  // Destroy the WebContents before starting the request.
  web_contents_.reset();

  // Plug in test content browser client.
  SelectCertificateBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  // Start the request and wait for it to pause.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // Check that SelectClientCertificate wasn't called and the request aborted.
  EXPECT_EQ(0, test_client.call_count());
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            raw_ptr_resource_handler_->status().error());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

// Verifies that ClientCertStore's callback doesn't crash if called after the
// loader is destroyed.
TEST_F(ClientCertResourceLoaderTest, StoreAsyncCancel) {
  scoped_ptr<LoaderDestroyingCertStore> test_store(
      new LoaderDestroyingCertStore(&loader_));
  resource_context_.SetClientCertStore(test_store.Pass());

  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(loader_);

  // Pump the event loop to ensure nothing asynchronous crashes either.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ResourceLoaderTest, ResumeCancelledRequest) {
  raw_ptr_resource_handler_->set_defer_request_on_will_start(true);

  loader_->StartRequest();
  loader_->CancelRequest(true);
  static_cast<ResourceController*>(loader_.get())->Resume();
}

// Tests that no invariants are broken if a ResourceHandler cancels during
// OnReadCompleted.
TEST_F(ResourceLoaderTest, CancelOnReadCompleted) {
  raw_ptr_resource_handler_->set_cancel_on_read_completed(true);

  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
}

// Tests that no invariants are broken if a ResourceHandler defers EOF.
TEST_F(ResourceLoaderTest, DeferEOF) {
  raw_ptr_resource_handler_->set_defer_eof(true);

  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());

  raw_ptr_resource_handler_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_resource_handler_->status().status());
}

// Tests that progress is reported correctly while uploading.
// TODO(andresantoso): Add test for the redirect case.
TEST_F(ResourceLoaderTest, UploadProgress) {
  // Set up a test server.
  net::test_server::EmbeddedTestServer server;
  ASSERT_TRUE(server.InitializeAndWaitUntilReady());
  base::FilePath path;
  PathService::Get(content::DIR_TEST_DATA, &path);
  server.ServeFilesFromDirectory(path);

  scoped_ptr<net::URLRequest> request(
      resource_context_.GetRequestContext()->CreateRequest(
          server.GetURL("/title1.html"),
          net::DEFAULT_PRIORITY,
          nullptr /* delegate */));

  // Start an upload.
  auto stream = new NonChunkedUploadDataStream(10);
  request->set_upload(make_scoped_ptr(stream));

  SetUpResourceLoader(request.Pass());
  loader_->StartRequest();

  stream->AppendData("xx");
  WaitForUploadProgress(2);

  stream->AppendData("yyy");
  WaitForUploadProgress(5);

  stream->AppendData("zzzzz");
  WaitForUploadProgress(10);
}

class ResourceLoaderRedirectToFileTest : public ResourceLoaderTest {
 public:
  ResourceLoaderRedirectToFileTest()
      : file_stream_(NULL),
        redirect_to_file_resource_handler_(NULL) {
  }

  base::FilePath temp_path() const { return temp_path_; }
  ShareableFileReference* deletable_file() const {
    return deletable_file_.get();
  }
  net::testing::MockFileStream* file_stream() const { return file_stream_; }
  RedirectToFileResourceHandler* redirect_to_file_resource_handler() const {
    return redirect_to_file_resource_handler_;
  }

  void ReleaseLoader() {
    file_stream_ = NULL;
    deletable_file_ = NULL;
    loader_.reset();
  }

  scoped_ptr<ResourceHandler> WrapResourceHandler(
      scoped_ptr<ResourceHandlerStub> leaf_handler,
      net::URLRequest* request) override {
    leaf_handler->set_expect_reads(false);

    // Make a temporary file.
    CHECK(base::CreateTemporaryFile(&temp_path_));
    int flags = base::File::FLAG_WRITE | base::File::FLAG_TEMPORARY |
                base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_ASYNC;
    base::File file(temp_path_, flags);
    CHECK(file.IsValid());

    // Create mock file streams and a ShareableFileReference.
    scoped_ptr<net::testing::MockFileStream> file_stream(
        new net::testing::MockFileStream(file.Pass(),
                                         base::ThreadTaskRunnerHandle::Get()));
    file_stream_ = file_stream.get();
    deletable_file_ = ShareableFileReference::GetOrCreate(
        temp_path_,
        ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        BrowserThread::GetMessageLoopProxyForThread(
            BrowserThread::FILE).get());

    // Inject them into the handler.
    scoped_ptr<RedirectToFileResourceHandler> handler(
        new RedirectToFileResourceHandler(leaf_handler.Pass(), request));
    redirect_to_file_resource_handler_ = handler.get();
    handler->SetCreateTemporaryFileStreamFunctionForTesting(
        base::Bind(&ResourceLoaderRedirectToFileTest::PostCallback,
                   base::Unretained(this),
                   base::Passed(&file_stream)));
    return handler.Pass();
  }

 private:
  void PostCallback(
      scoped_ptr<net::FileStream> file_stream,
      const CreateTemporaryFileStreamCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, base::File::FILE_OK,
                              base::Passed(&file_stream), deletable_file_));
  }

  base::FilePath temp_path_;
  scoped_refptr<ShareableFileReference> deletable_file_;
  // These are owned by the ResourceLoader.
  net::testing::MockFileStream* file_stream_;
  RedirectToFileResourceHandler* redirect_to_file_resource_handler_;
};

// Tests that a RedirectToFileResourceHandler works and forwards everything
// downstream.
TEST_F(ResourceLoaderRedirectToFileTest, Basic) {
  // Run it to completion.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // Check that the handler forwarded all information to the downstream handler.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(test_data().size(), static_cast<size_t>(
      raw_ptr_resource_handler_->total_bytes_downloaded()));

  // Check that the data was written to the file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(temp_path(), &contents));
  EXPECT_EQ(test_data(), contents);

  // Release the loader and the saved reference to file. The file should be gone
  // now.
  ReleaseLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(temp_path()));
}

// Tests that RedirectToFileResourceHandler handles errors in creating the
// temporary file.
TEST_F(ResourceLoaderRedirectToFileTest, CreateTemporaryError) {
  // Swap out the create temporary function.
  redirect_to_file_resource_handler()->
      SetCreateTemporaryFileStreamFunctionForTesting(
          base::Bind(&CreateTemporaryError, base::File::FILE_ERROR_FAILED));

  // Run it to completion.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // To downstream, the request was canceled.
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());
}

// Tests that RedirectToFileResourceHandler handles synchronous write errors.
TEST_F(ResourceLoaderRedirectToFileTest, WriteError) {
  file_stream()->set_forced_error(net::ERR_FAILED);

  // Run it to completion.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // To downstream, the request was canceled sometime after it started, but
  // before any data was written.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());

  // Release the loader. The file should be gone now.
  ReleaseLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(temp_path()));
}

// Tests that RedirectToFileResourceHandler handles asynchronous write errors.
TEST_F(ResourceLoaderRedirectToFileTest, WriteErrorAsync) {
  file_stream()->set_forced_error_async(net::ERR_FAILED);

  // Run it to completion.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // To downstream, the request was canceled sometime after it started, but
  // before any data was written.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());

  // Release the loader. The file should be gone now.
  ReleaseLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(temp_path()));
}

// Tests that RedirectToFileHandler defers completion if there are outstanding
// writes and accounts for errors which occur in that time.
TEST_F(ResourceLoaderRedirectToFileTest, DeferCompletion) {
  // Program the MockFileStream to error asynchronously, but throttle the
  // callback.
  file_stream()->set_forced_error_async(net::ERR_FAILED);
  file_stream()->ThrottleCallbacks();

  // Run it as far as it will go.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // At this point, the request should have completed.
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_to_request_->status().status());

  // However, the resource loader stack is stuck somewhere after receiving the
  // response.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());

  // Now, release the floodgates.
  file_stream()->ReleaseCallbacks();
  base::RunLoop().RunUntilIdle();

  // Although the URLRequest was successful, the leaf handler sees a failure
  // because the write never completed.
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::CANCELED,
            raw_ptr_resource_handler_->status().status());

  // Release the loader. The file should be gone now.
  ReleaseLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(temp_path()));
}

// Tests that a RedirectToFileResourceHandler behaves properly when the
// downstream handler defers OnWillStart.
TEST_F(ResourceLoaderRedirectToFileTest, DownstreamDeferStart) {
  // Defer OnWillStart.
  raw_ptr_resource_handler_->set_defer_request_on_will_start(true);

  // Run as far as we'll go.
  loader_->StartRequest();
  base::RunLoop().RunUntilIdle();

  // The request should have stopped at OnWillStart.
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_FALSE(raw_ptr_resource_handler_->response());
  EXPECT_FALSE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(0, raw_ptr_resource_handler_->total_bytes_downloaded());

  // Now resume the request. Now we complete.
  raw_ptr_resource_handler_->Resume();
  base::RunLoop().RunUntilIdle();

  // Check that the handler forwarded all information to the downstream handler.
  EXPECT_EQ(temp_path(),
            raw_ptr_resource_handler_->response()->head.download_file_path);
  EXPECT_EQ(test_url(), raw_ptr_resource_handler_->start_url());
  EXPECT_TRUE(raw_ptr_resource_handler_->received_response_completed());
  EXPECT_EQ(net::URLRequestStatus::SUCCESS,
            raw_ptr_resource_handler_->status().status());
  EXPECT_EQ(test_data().size(), static_cast<size_t>(
      raw_ptr_resource_handler_->total_bytes_downloaded()));

  // Check that the data was written to the file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(temp_path(), &contents));
  EXPECT_EQ(test_data(), contents);

  // Release the loader. The file should be gone now.
  ReleaseLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(temp_path()));
}

}  // namespace content
