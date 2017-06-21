// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/browser/renderer_host/media/webrtc_identity_service_host.h"
#include "content/common/media/webrtc_identity_messages.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char FAKE_URL[] = "http://fake.com";
const char FAKE_FIRST_PARTY_URL[] = "http://fake.firstparty.com";
const char FAKE_IDENTITY_NAME[] = "fake identity";
const char FAKE_COMMON_NAME[] = "fake common name";
const char FAKE_CERTIFICATE[] = "fake cert";
const char FAKE_PRIVATE_KEY[] = "fake private key";
const int FAKE_RENDERER_ID = 10;
const int FAKE_REQUEST_ID = 1;

class WebRTCIdentityServiceHostTestBrowserClient
    : public TestContentBrowserClient {
 public:
  WebRTCIdentityServiceHostTestBrowserClient() : allow_cache_(true) {}

  void set_allow_cache(bool allow) { allow_cache_ = allow; }

  bool AllowWebRTCIdentityCache(const GURL& url,
                                const GURL& first_party_url,
                                ResourceContext* context) override {
    url_ = url;
    first_party_url_ = first_party_url;
    return allow_cache_;
  }

  GURL url() const { return url_; }
  GURL first_party_url() const { return first_party_url_; }

 private:
  bool allow_cache_;
  GURL url_;
  GURL first_party_url_;
};

class MockWebRTCIdentityStore : public WebRTCIdentityStore {
 public:
  MockWebRTCIdentityStore()
      : WebRTCIdentityStore(base::FilePath(), NULL), enable_cache_(true) {}

  base::Closure RequestIdentity(const GURL& origin,
                                const std::string& identity_name,
                                const std::string& common_name,
                                const CompletionCallback& callback,
                                bool enable_cache) override {
    EXPECT_TRUE(callback_.is_null());

    callback_ = callback;
    enable_cache_ = enable_cache;
    return base::Bind(&MockWebRTCIdentityStore::OnCancel,
                      base::Unretained(this));
  }

  bool HasPendingRequest() const { return !callback_.is_null(); }

  void RunCompletionCallback(int error,
                             const std::string& cert,
                             const std::string& key) {
    callback_.Run(error, cert, key);
    callback_.Reset();
  }

  bool enable_cache() const { return enable_cache_; }

 private:
  ~MockWebRTCIdentityStore() override {}

  void OnCancel() { callback_.Reset(); }

  CompletionCallback callback_;
  bool enable_cache_;
};

class WebRTCIdentityServiceHostForTest : public WebRTCIdentityServiceHost {
 public:
  WebRTCIdentityServiceHostForTest(WebRTCIdentityStore* identity_store,
                                   ResourceContext* resource_context)
      : WebRTCIdentityServiceHost(FAKE_RENDERER_ID,
                                  identity_store,
                                  resource_context) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    policy->Add(FAKE_RENDERER_ID);
  }

  bool Send(IPC::Message* message) override {
    messages_.push_back(*message);
    delete message;
    return true;
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    return WebRTCIdentityServiceHost::OnMessageReceived(message);
  }

  IPC::Message GetLastMessage() { return messages_.back(); }

  int GetNumberOfMessages() { return messages_.size(); }

  void ClearMessages() { messages_.clear(); }

 private:
  ~WebRTCIdentityServiceHostForTest() override {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    policy->Remove(FAKE_RENDERER_ID);
  }

  std::deque<IPC::Message> messages_;
};

class WebRTCIdentityServiceHostTest : public ::testing::Test {
 public:
  WebRTCIdentityServiceHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        mock_resource_context_(new MockResourceContext()),
        store_(new MockWebRTCIdentityStore()),
        host_(new WebRTCIdentityServiceHostForTest(
            store_.get(),
            mock_resource_context_.get())) {}

  void SendRequestToHost() {
    WebRTCIdentityMsg_RequestIdentity_Params params;
    params.request_id = FAKE_REQUEST_ID;
    params.url = GURL(FAKE_URL);
    params.first_party_for_cookies = GURL(FAKE_FIRST_PARTY_URL);
    params.identity_name = FAKE_IDENTITY_NAME;
    params.common_name = FAKE_COMMON_NAME;
    host_->OnMessageReceived(WebRTCIdentityMsg_RequestIdentity(params));
  }

  void SendCancelRequestToHost() {
    host_->OnMessageReceived(WebRTCIdentityMsg_CancelRequest());
  }

  void VerifyRequestFailedMessage(int error) {
    EXPECT_EQ(1, host_->GetNumberOfMessages());
    IPC::Message ipc = host_->GetLastMessage();
    EXPECT_EQ(ipc.type(), WebRTCIdentityHostMsg_RequestFailed::ID);

    base::Tuple<int, int> error_in_message;
    WebRTCIdentityHostMsg_RequestFailed::Read(&ipc, &error_in_message);
    EXPECT_EQ(FAKE_REQUEST_ID, base::get<0>(error_in_message));
    EXPECT_EQ(error, base::get<1>(error_in_message));
  }

  void VerifyIdentityReadyMessage(const std::string& cert,
                                  const std::string& key) {
    EXPECT_EQ(1, host_->GetNumberOfMessages());
    IPC::Message ipc = host_->GetLastMessage();
    EXPECT_EQ(ipc.type(), WebRTCIdentityHostMsg_IdentityReady::ID);

    base::Tuple<int, std::string, std::string> identity_in_message;
    WebRTCIdentityHostMsg_IdentityReady::Read(&ipc, &identity_in_message);
    EXPECT_EQ(FAKE_REQUEST_ID, base::get<0>(identity_in_message));
    EXPECT_EQ(cert, base::get<1>(identity_in_message));
    EXPECT_EQ(key, base::get<2>(identity_in_message));
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<MockResourceContext> mock_resource_context_;
  scoped_refptr<MockWebRTCIdentityStore> store_;
  scoped_refptr<WebRTCIdentityServiceHostForTest> host_;
};

}  // namespace

TEST_F(WebRTCIdentityServiceHostTest, TestCacheDisabled) {
  WebRTCIdentityServiceHostTestBrowserClient test_client;
  test_client.set_allow_cache(false);
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  SendRequestToHost();
  EXPECT_TRUE(store_->HasPendingRequest());
  EXPECT_FALSE(store_->enable_cache());
  EXPECT_EQ(GURL(FAKE_URL), test_client.url());
  EXPECT_EQ(GURL(FAKE_FIRST_PARTY_URL), test_client.first_party_url());

  // Restore the original content browser client.
  SetBrowserClientForTesting(old_client);
}

TEST_F(WebRTCIdentityServiceHostTest, TestSendAndCancelRequest) {
  SendRequestToHost();
  EXPECT_TRUE(store_->HasPendingRequest());
  SendCancelRequestToHost();
  EXPECT_FALSE(store_->HasPendingRequest());
}

TEST_F(WebRTCIdentityServiceHostTest, TestOnlyOneRequestAllowed) {
  SendRequestToHost();
  EXPECT_TRUE(store_->HasPendingRequest());
  EXPECT_EQ(0, host_->GetNumberOfMessages());
  SendRequestToHost();

  VerifyRequestFailedMessage(net::ERR_INSUFFICIENT_RESOURCES);
}

TEST_F(WebRTCIdentityServiceHostTest, TestOnIdentityReady) {
  SendRequestToHost();
  store_->RunCompletionCallback(net::OK, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY);
  VerifyIdentityReadyMessage(FAKE_CERTIFICATE, FAKE_PRIVATE_KEY);
}

TEST_F(WebRTCIdentityServiceHostTest, TestOnRequestFailed) {
  SendRequestToHost();
  store_->RunCompletionCallback(net::ERR_KEY_GENERATION_FAILED, "", "");
  VerifyRequestFailedMessage(net::ERR_KEY_GENERATION_FAILED);
}

TEST_F(WebRTCIdentityServiceHostTest, TestOriginAccessDenied) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->Remove(FAKE_RENDERER_ID);

  SendRequestToHost();
  VerifyRequestFailedMessage(net::ERR_ACCESS_DENIED);
}

// Verifies that we do not crash if we try to cancel a completed request.
TEST_F(WebRTCIdentityServiceHostTest, TestCancelAfterRequestCompleted) {
  SendRequestToHost();
  store_->RunCompletionCallback(net::OK, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY);
  SendCancelRequestToHost();
}

}  // namespace content
