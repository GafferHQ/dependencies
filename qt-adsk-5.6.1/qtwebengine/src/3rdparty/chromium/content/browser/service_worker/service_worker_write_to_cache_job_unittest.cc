// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/resource_request_body.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kHeaders[] =
    "HTTP/1.1 200 OK\0"
    "Content-Type: text/javascript\0"
    "Expires: Thu, 1 Jan 2100 20:00:00 GMT\0"
    "\0";
const char kScriptCode[] = "// no script code\n";

void EmptyCallback() {
}

// The blocksize that ServiceWorkerWriteToCacheJob reads/writes at a time.
const int kBlockSize = 16 * 1024;
const int kNumBlocks = 8;
const int kMiddleBlock = 5;

std::string GenerateLongResponse() {
  return std::string(kNumBlocks * kBlockSize, 'a');
}
net::URLRequestJob* CreateNormalURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new net::URLRequestTestJob(request,
                                    network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    kScriptCode,
                                    true);
}

net::URLRequestJob* CreateResponseJob(const std::string& response_data,
                                      net::URLRequest* request,
                                      net::NetworkDelegate* network_delegate) {
  return new net::URLRequestTestJob(request, network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    response_data, true);
}

net::URLRequestJob* CreateInvalidMimeTypeJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  const char kPlainTextHeaders[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: text/plain\0"
      "Expires: Thu, 1 Jan 2100 20:00:00 GMT\0"
      "\0";
  return new net::URLRequestTestJob(
      request,
      network_delegate,
      std::string(kPlainTextHeaders, arraysize(kPlainTextHeaders)),
      kScriptCode,
      true);
}

class SSLCertificateErrorJob : public net::URLRequestTestJob {
 public:
  SSLCertificateErrorJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance),
        weak_factory_(this) {}
  void Start() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SSLCertificateErrorJob::NotifyError,
                              weak_factory_.GetWeakPtr()));
  }
  void NotifyError() {
    net::SSLInfo info;
    info.cert_status = net::CERT_STATUS_DATE_INVALID;
    NotifySSLCertificateError(info, true);
  }

 protected:
  ~SSLCertificateErrorJob() override {}
  base::WeakPtrFactory<SSLCertificateErrorJob> weak_factory_;
};

net::URLRequestJob* CreateSSLCertificateErrorJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new SSLCertificateErrorJob(request,
                                    network_delegate,
                                    std::string(kHeaders, arraysize(kHeaders)),
                                    kScriptCode,
                                    true);
}

class CertStatusErrorJob : public net::URLRequestTestJob {
 public:
  CertStatusErrorJob(net::URLRequest* request,
                     net::NetworkDelegate* network_delegate,
                     const std::string& response_headers,
                     const std::string& response_data,
                     bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}
  void GetResponseInfo(net::HttpResponseInfo* info) override {
    URLRequestTestJob::GetResponseInfo(info);
    info->ssl_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  }

 protected:
  ~CertStatusErrorJob() override {}
};

net::URLRequestJob* CreateCertStatusErrorJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new CertStatusErrorJob(request,
                                network_delegate,
                                std::string(kHeaders, arraysize(kHeaders)),
                                kScriptCode,
                                true);
}

class MockHttpProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  typedef base::Callback<
      net::URLRequestJob*(net::URLRequest*, net::NetworkDelegate*)> JobCallback;

  explicit MockHttpProtocolHandler(ResourceContext* resource_context)
      : resource_context_(resource_context) {}
  ~MockHttpProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    ServiceWorkerRequestHandler* handler =
        ServiceWorkerRequestHandler::GetHandler(request);
    if (handler) {
      return handler->MaybeCreateJob(
          request, network_delegate, resource_context_);
    }
    return create_job_callback_.Run(request, network_delegate);
  }
  void SetCreateJobCallback(const JobCallback& callback) {
    create_job_callback_ = callback;
  }

 private:
  ResourceContext* resource_context_;
  JobCallback create_job_callback_;
};

class ResponseVerifier : public base::RefCounted<ResponseVerifier> {
 public:
  ResponseVerifier(scoped_ptr<ServiceWorkerResponseReader> reader,
                   const std::string& expected,
                   const base::Callback<void(bool)> callback)
      : reader_(reader.release()), expected_(expected), callback_(callback) {}

  void Start() {
    info_buffer_ = new HttpResponseInfoIOBuffer();
    io_buffer_ = new net::IOBuffer(kBlockSize);
    reader_->ReadInfo(info_buffer_.get(),
                      base::Bind(&ResponseVerifier::OnReadInfoComplete, this));
    bytes_read_ = 0;
  }

  void OnReadInfoComplete(int result) {
    if (result < 0) {
      callback_.Run(false);
      return;
    }
    if (info_buffer_->response_data_size !=
        static_cast<int>(expected_.size())) {
      callback_.Run(false);
      return;
    }
    ReadSomeData();
  }

  void ReadSomeData() {
    reader_->ReadData(io_buffer_.get(), kBlockSize,
                      base::Bind(&ResponseVerifier::OnReadDataComplete, this));
  }

  void OnReadDataComplete(int result) {
    if (result < 0) {
      callback_.Run(false);
      return;
    }
    if (result == 0) {
      callback_.Run(true);
      return;
    }
    std::string str(io_buffer_->data(), result);
    std::string expect = expected_.substr(bytes_read_, result);
    if (str != expect) {
      callback_.Run(false);
      return;
    }
    bytes_read_ += result;
    ReadSomeData();
  }

 private:
  friend class base::RefCounted<ResponseVerifier>;
  ~ResponseVerifier() {}

  scoped_ptr<ServiceWorkerResponseReader> reader_;
  const std::string expected_;
  base::Callback<void(bool)> callback_;
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t bytes_read_;
};

}  // namespace

class ServiceWorkerWriteToCacheJobTest : public testing::Test {
 public:
  ServiceWorkerWriteToCacheJobTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        mock_protocol_handler_(nullptr) {}
  ~ServiceWorkerWriteToCacheJobTest() override {}

  void CreateHostForVersion(
      int process_id,
      int provider_id,
      const scoped_refptr<ServiceWorkerVersion>& version) {
    scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
        process_id, MSG_ROUTING_NONE, provider_id,
        SERVICE_WORKER_PROVIDER_FOR_WORKER, context()->AsWeakPtr(), nullptr));
    base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
    context()->AddProviderHost(host.Pass());
    provider_host->running_hosted_version_ = version;
  }

  void SetUpScriptRequest(int process_id, int provider_id) {
    request_.reset();
    url_request_context_.reset();
    url_request_job_factory_.reset();
    mock_protocol_handler_ = nullptr;
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();

    url_request_context_.reset(new net::URLRequestContext);
    mock_protocol_handler_ = new MockHttpProtocolHandler(&resource_context_);
    url_request_job_factory_.reset(new net::URLRequestJobFactoryImpl);
    url_request_job_factory_->SetProtocolHandler("https",
                                                 mock_protocol_handler_);
    url_request_context_->set_job_factory(url_request_job_factory_.get());

    request_ = url_request_context_->CreateRequest(
        script_url_, net::DEFAULT_PRIORITY, &url_request_delegate_);
    ServiceWorkerRequestHandler::InitializeHandler(
        request_.get(), context_wrapper(), &blob_storage_context_, process_id,
        provider_id, false, FETCH_REQUEST_MODE_NO_CORS,
        FETCH_CREDENTIALS_MODE_OMIT, RESOURCE_TYPE_SERVICE_WORKER,
        REQUEST_CONTEXT_TYPE_SERVICE_WORKER, REQUEST_CONTEXT_FRAME_TYPE_NONE,
        scoped_refptr<ResourceRequestBody>());
  }

  int NextRenderProcessId() { return next_render_process_id_++; }
  int NextProviderId() { return next_provider_id_++; }
  int NextVersionId() { return next_version_id_++; }

  void SetUp() override {
    int render_process_id = NextRenderProcessId();
    int provider_id = NextProviderId();
    helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath(), render_process_id));

    // A new unstored registration/version.
    scope_ = GURL("https://host/scope/");
    script_url_ = GURL("https://host/script.js");
    registration_ =
        new ServiceWorkerRegistration(scope_, 1L, context()->AsWeakPtr());
    version_ =
        new ServiceWorkerVersion(registration_.get(), script_url_,
                                 NextVersionId(), context()->AsWeakPtr());
    CreateHostForVersion(render_process_id, provider_id, version_);
    SetUpScriptRequest(render_process_id, provider_id);

    context()->storage()->LazyInitialize(base::Bind(&EmptyCallback));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    request_.reset();
    url_request_context_.reset();
    url_request_job_factory_.reset();
    mock_protocol_handler_ = nullptr;
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  int CreateIncumbent(const std::string& response) {
    mock_protocol_handler_->SetCreateJobCallback(
        base::Bind(&CreateResponseJob, response));
    request_->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, request_->status().status());
    int incumbent_resource_id =
        version_->script_cache_map()->LookupResourceId(script_url_);
    EXPECT_NE(kInvalidServiceWorkerResponseId, incumbent_resource_id);

    registration_->SetActiveVersion(version_);

    // Teardown the request.
    request_.reset();
    url_request_context_.reset();
    url_request_job_factory_.reset();
    mock_protocol_handler_ = nullptr;
    base::RunLoop().RunUntilIdle();

    return incumbent_resource_id;
  }

  int GetResourceId(ServiceWorkerVersion* version) {
    return version->script_cache_map()->LookupResourceId(script_url_);
  }

  // Performs the net request for an update of |registration_|'s incumbent
  // to the script |response|. Returns the new version.
  scoped_refptr<ServiceWorkerVersion> UpdateScript(
      const std::string& response) {
    int render_process_id = NextRenderProcessId();
    int provider_id = NextProviderId();
    scoped_refptr<ServiceWorkerVersion> new_version =
        new ServiceWorkerVersion(registration_.get(), script_url_,
                                 NextVersionId(), context()->AsWeakPtr());
    CreateHostForVersion(render_process_id, provider_id, new_version);

    SetUpScriptRequest(render_process_id, provider_id);
    mock_protocol_handler_->SetCreateJobCallback(
        base::Bind(&CreateResponseJob, response));
    request_->Start();
    base::RunLoop().RunUntilIdle();
    return new_version;
  }

  void VerifyResource(int id, const std::string& expected) {
    ASSERT_NE(kInvalidServiceWorkerResourceId, id);
    bool is_equal = false;
    scoped_ptr<ServiceWorkerResponseReader> reader =
        context()->storage()->CreateResponseReader(id);
    scoped_refptr<ResponseVerifier> verifier = new ResponseVerifier(
        reader.Pass(), expected, CreateReceiverOnCurrentThread(&is_equal));
    verifier->Start();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(is_equal);
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::URLRequestJobFactoryImpl> url_request_job_factory_;
  scoped_ptr<net::URLRequest> request_;
  MockHttpProtocolHandler* mock_protocol_handler_;

  storage::BlobStorageContext blob_storage_context_;
  content::MockResourceContext resource_context_;

  MockURLRequestDelegate url_request_delegate_;
  GURL scope_;
  GURL script_url_;

  int next_render_process_id_ = 1224;  // dummy value
  int next_provider_id_ = 1;
  int64 next_version_id_ = 1L;
};

TEST_F(ServiceWorkerWriteToCacheJobTest, Normal) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateNormalURLRequestJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::SUCCESS, request_->status().status());
  EXPECT_NE(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, InvalidMimeType) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateInvalidMimeTypeJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, SSLCertificateError) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateSSLCertificateErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, CertStatusError) {
  mock_protocol_handler_->SetCreateJobCallback(
      base::Bind(&CreateCertStatusErrorJob));
  request_->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::URLRequestStatus::FAILED, request_->status().status());
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, request_->status().error());
  EXPECT_EQ(kInvalidServiceWorkerResponseId,
            version_->script_cache_map()->LookupResourceId(script_url_));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_SameScript) {
  std::string response = GenerateLongResponse();
  CreateIncumbent(response);
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  EXPECT_EQ(kInvalidServiceWorkerResponseId, GetResourceId(version.get()));
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_SameSizeScript) {
  std::string response = GenerateLongResponse();
  CreateIncumbent(response);

  // Change the first byte.
  response[0] = 'x';
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change something within the first block.
  response[5555] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change something in a middle block.
  response[kMiddleBlock * kBlockSize + 111] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change something within the last block.
  response[(kNumBlocks - 1) * kBlockSize] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Change the last byte.
  response[(kNumBlocks * kBlockSize) - 1] = 'x';
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_TruncatedScript) {
  std::string response = GenerateLongResponse();
  CreateIncumbent(response);

  // Truncate a single byte.
  response.resize(response.size() - 1);
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Truncate to a middle block.
  response.resize((kMiddleBlock + 1) * kBlockSize + 111);
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Truncate to a block boundary.
  response.resize((kMiddleBlock - 1) * kBlockSize);
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Truncate to a single byte.
  response.resize(1);
  version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_ElongatedScript) {
  std::string original_response = GenerateLongResponse();
  CreateIncumbent(original_response);

  // Extend a single byte.
  std::string new_response = original_response + 'a';
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(new_response);
  VerifyResource(GetResourceId(version.get()), new_response);
  registration_->SetWaitingVersion(version);

  // Extend multiple blocks.
  new_response = original_response + std::string(3 * kBlockSize, 'a');
  version = UpdateScript(new_response);
  VerifyResource(GetResourceId(version.get()), new_response);
  registration_->SetWaitingVersion(version);

  // Extend multiple blocks and bytes.
  new_response = original_response + std::string(7 * kBlockSize + 777, 'a');
  version = UpdateScript(new_response);
  VerifyResource(GetResourceId(version.get()), new_response);
  registration_->SetWaitingVersion(version);
}

TEST_F(ServiceWorkerWriteToCacheJobTest, Update_EmptyScript) {
  // Create empty incumbent.
  CreateIncumbent(std::string());

  // Update from empty to non-empty.
  std::string response = GenerateLongResponse();
  scoped_refptr<ServiceWorkerVersion> version = UpdateScript(response);
  VerifyResource(GetResourceId(version.get()), response);
  registration_->SetWaitingVersion(version);

  // Update from non-empty to empty.
  version = UpdateScript(std::string());
  VerifyResource(GetResourceId(version.get()), std::string());
  registration_->SetWaitingVersion(version);

  // Update from empty to empty.
  version = UpdateScript(std::string());
  EXPECT_EQ(kInvalidServiceWorkerResponseId, GetResourceId(version.get()));
}

}  // namespace content
