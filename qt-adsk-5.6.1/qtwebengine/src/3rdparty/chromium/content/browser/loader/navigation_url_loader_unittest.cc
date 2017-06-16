// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_url_request_job.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class StreamProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  StreamProtocolHandler(StreamRegistry* registry) : registry_(registry) {}

  // net::URLRequestJobFactory::ProtocolHandler implementation.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    scoped_refptr<Stream> stream = registry_->GetStream(request->url());
    if (stream.get())
      return new StreamURLRequestJob(request, network_delegate, stream);
    return nullptr;
  }

 private:
  StreamRegistry* registry_;

  DISALLOW_COPY_AND_ASSIGN(StreamProtocolHandler);
};

class TestNavigationURLLoaderDelegate : public NavigationURLLoaderDelegate {
 public:
  TestNavigationURLLoaderDelegate()
      : net_error_(0), on_request_handled_counter_(0) {}

  const net::RedirectInfo& redirect_info() const { return redirect_info_; }
  ResourceResponse* redirect_response() const {
    return redirect_response_.get();
  }
  ResourceResponse* response() const { return response_.get(); }
  StreamHandle* body() const { return body_.get(); }
  int net_error() const { return net_error_; }
  int on_request_handled_counter() const { return on_request_handled_counter_; }

  void WaitForRequestRedirected() {
    request_redirected_.reset(new base::RunLoop);
    request_redirected_->Run();
    request_redirected_.reset();
  }

  void WaitForResponseStarted() {
    response_started_.reset(new base::RunLoop);
    response_started_->Run();
    response_started_.reset();
  }

  void WaitForRequestFailed() {
    request_failed_.reset(new base::RunLoop);
    request_failed_->Run();
    request_failed_.reset();
  }

  void ReleaseBody() {
    body_.reset();
  }

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<ResourceResponse>& response) override {
    redirect_info_ = redirect_info;
    redirect_response_ = response;
    ASSERT_TRUE(request_redirected_);
    request_redirected_->Quit();
  }

  void OnResponseStarted(const scoped_refptr<ResourceResponse>& response,
                         scoped_ptr<StreamHandle> body) override {
    response_ = response;
    body_ = body.Pass();
    ASSERT_TRUE(response_started_);
    response_started_->Quit();
  }

  void OnRequestFailed(bool in_cache, int net_error) override {
    net_error_ = net_error;
    ASSERT_TRUE(request_failed_);
    request_failed_->Quit();
  }

  void OnRequestStarted(base::TimeTicks timestamp) override {
    ASSERT_FALSE(timestamp.is_null());
    ++on_request_handled_counter_;
  }

 private:
  net::RedirectInfo redirect_info_;
  scoped_refptr<ResourceResponse> redirect_response_;
  scoped_refptr<ResourceResponse> response_;
  scoped_ptr<StreamHandle> body_;
  int net_error_;
  int on_request_handled_counter_;

  scoped_ptr<base::RunLoop> request_redirected_;
  scoped_ptr<base::RunLoop> response_started_;
  scoped_ptr<base::RunLoop> request_failed_;
};

class RequestBlockingResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  // ResourceDispatcherHostDelegate implementation:
  bool ShouldBeginRequest(const std::string& method,
                          const GURL& url,
                          ResourceType resource_type,
                          ResourceContext* resource_context) override {
    return false;
  }
};

}  // namespace

class NavigationURLLoaderTest : public testing::Test {
 public:
  NavigationURLLoaderTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        browser_context_(new TestBrowserContext) {
    BrowserContext::EnsureResourceContextInitialized(browser_context_.get());
    base::RunLoop().RunUntilIdle();
    net::URLRequestContext* request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
    // Attach URLRequestTestJob and make streams work.
    job_factory_.SetProtocolHandler(
        "test", net::URLRequestTestJob::CreateProtocolHandler());
    job_factory_.SetProtocolHandler(
        "blob", new StreamProtocolHandler(
            StreamContext::GetFor(browser_context_.get())->registry()));
    request_context->set_job_factory(&job_factory_);

    // NavigationURLLoader is only used for browser-side navigations.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBrowserSideNavigation);
  }

  scoped_ptr<NavigationURLLoader> MakeTestLoader(
      const GURL& url,
      NavigationURLLoaderDelegate* delegate) {
    BeginNavigationParams begin_params(
        "GET", std::string(), net::LOAD_NORMAL, false);
    CommonNavigationParams common_params;
    common_params.url = url;
    scoped_ptr<NavigationRequestInfo> request_info(
        new NavigationRequestInfo(common_params, begin_params, url, true, false,
                                  -1, scoped_refptr<ResourceRequestBody>()));

    return NavigationURLLoader::Create(
        browser_context_.get(), 0, request_info.Pass(), delegate);
  }

  // Helper function for fetching the body of a URL to a string.
  std::string FetchURL(const GURL& url) {
    net::TestDelegate delegate;
    net::URLRequestContext* request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
    scoped_ptr<net::URLRequest> request(request_context->CreateRequest(
        url, net::DEFAULT_PRIORITY, &delegate));
    request->Start();
    base::RunLoop().Run();

    EXPECT_TRUE(request->status().is_success());
    EXPECT_EQ(200, request->response_headers()->response_code());
    return delegate.data_received();
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;
  net::URLRequestJobFactoryImpl job_factory_;
  scoped_ptr<TestBrowserContext> browser_context_;
  ResourceDispatcherHostImpl host_;
};

// Tests that a basic request works.
TEST_F(NavigationURLLoaderTest, Basic) {
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_1(), &delegate);

  // Wait for the response to come back.
  delegate.WaitForResponseStarted();

  // Check the response is correct.
  EXPECT_EQ("text/html", delegate.response()->head.mime_type);
  EXPECT_EQ(200, delegate.response()->head.headers->response_code());

  // Check the body is correct.
  EXPECT_EQ(net::URLRequestTestJob::test_data_1(),
            FetchURL(delegate.body()->GetURL()));

  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that request failures are propagated correctly.
TEST_F(NavigationURLLoaderTest, RequestFailed) {
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(GURL("bogus:bogus"), &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_UNKNOWN_URL_SCHEME, delegate.net_error());
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Test that redirects are sent to the delegate.
TEST_F(NavigationURLLoaderTest, RequestRedirected) {
  // Fake a top-level request. Choose a URL which redirects so the request can
  // be paused before the response comes in.
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_redirect_to_url_2(),
                     &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();
  EXPECT_EQ(net::URLRequestTestJob::test_url_2(),
            delegate.redirect_info().new_url);
  EXPECT_EQ("GET", delegate.redirect_info().new_method);
  EXPECT_EQ(net::URLRequestTestJob::test_url_2(),
            delegate.redirect_info().new_first_party_for_cookies);
  EXPECT_EQ(302, delegate.redirect_response()->head.headers->response_code());
  EXPECT_EQ(1, delegate.on_request_handled_counter());

  // Wait for the response to complete.
  loader->FollowRedirect();
  delegate.WaitForResponseStarted();

  // Check the response is correct.
  EXPECT_EQ("text/html", delegate.response()->head.mime_type);
  EXPECT_EQ(200, delegate.response()->head.headers->response_code());

  // Release the body and check it is correct.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());
  EXPECT_EQ(net::URLRequestTestJob::test_data_2(),
            FetchURL(delegate.body()->GetURL()));

  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that the destroying the loader cancels the request.
TEST_F(NavigationURLLoaderTest, CancelOnDestruct) {
  // Fake a top-level request. Choose a URL which redirects so the request can
  // be paused before the response comes in.
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_redirect_to_url_2(),
                     &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();

  // Destroy the loader and verify that URLRequestTestJob no longer has anything
  // paused.
  loader.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(net::URLRequestTestJob::ProcessOnePendingMessage());
}

// Test that the delegate is not called if OnResponseStarted and destroying the
// loader race.
TEST_F(NavigationURLLoaderTest, CancelResponseRace) {
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_redirect_to_url_2(),
                     &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();

  // In the same event loop iteration, follow the redirect (allowing the
  // response to go through) and destroy the loader.
  loader->FollowRedirect();
  loader.reset();

  // Verify the URLRequestTestJob no longer has anything paused and that no
  // response body was received.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(net::URLRequestTestJob::ProcessOnePendingMessage());
  EXPECT_FALSE(delegate.body());
}

// Tests that the loader may be canceled by context.
TEST_F(NavigationURLLoaderTest, CancelByContext) {
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_redirect_to_url_2(),
                     &delegate);

  // Wait for the request to redirect.
  delegate.WaitForRequestRedirected();

  // Cancel all requests.
  host_.CancelRequestsForContext(browser_context_->GetResourceContext());

  // Wait for the request to now be aborted.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_ABORTED, delegate.net_error());
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that, if the request is blocked by the ResourceDispatcherHostDelegate,
// the caller is informed appropriately.
TEST_F(NavigationURLLoaderTest, RequestBlocked) {
  RequestBlockingResourceDispatcherHostDelegate rdh_delegate;
  host_.SetDelegate(&rdh_delegate);

  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_1(), &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_ABORTED, delegate.net_error());
  EXPECT_EQ(1, delegate.on_request_handled_counter());

  host_.SetDelegate(nullptr);
}

// Tests that ownership leaves the loader once the response is received.
TEST_F(NavigationURLLoaderTest, LoaderDetached) {
  // Fake a top-level request to a URL whose body does not load immediately.
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_2(), &delegate);

  // Wait for the response to come back.
  delegate.WaitForResponseStarted();

  // Check the response is correct.
  EXPECT_EQ("text/html", delegate.response()->head.mime_type);
  EXPECT_EQ(200, delegate.response()->head.headers->response_code());

  // Destroy the loader.
  loader.reset();
  base::RunLoop().RunUntilIdle();

  // Check the body can still be fetched through the StreamHandle.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());
  EXPECT_EQ(net::URLRequestTestJob::test_data_2(),
            FetchURL(delegate.body()->GetURL()));
}

// Tests that the request is owned by the body StreamHandle.
TEST_F(NavigationURLLoaderTest, OwnedByHandle) {
  // Fake a top-level request to a URL whose body does not load immediately.
  TestNavigationURLLoaderDelegate delegate;
  scoped_ptr<NavigationURLLoader> loader =
      MakeTestLoader(net::URLRequestTestJob::test_url_2(), &delegate);

  // Wait for the response to come back.
  delegate.WaitForResponseStarted();

  // Release the body.
  delegate.ReleaseBody();
  base::RunLoop().RunUntilIdle();

  // Verify that URLRequestTestJob no longer has anything paused.
  EXPECT_FALSE(net::URLRequestTestJob::ProcessOnePendingMessage());
}

}  // namespace content
