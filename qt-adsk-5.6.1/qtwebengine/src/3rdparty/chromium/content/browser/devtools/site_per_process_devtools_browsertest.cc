// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {

class SitePerProcessDevToolsBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessDevToolsBrowserTest() {}
};

class TestClient: public DevToolsAgentHostClient {
 public:
  TestClient() : closed_(false) {}
  ~TestClient() override {}

  bool closed() { return closed_; }

  void DispatchProtocolMessage(
      DevToolsAgentHost* agent_host,
      const std::string& message) override {
  }

  void AgentHostClosed(
      DevToolsAgentHost* agent_host,
      bool replaced_with_another_client) override {
    closed_ = true;
  }

 private:
  bool closed_;
};

// Fails on Android, http://crbug.com/464993.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteIframeAgentHost DISABLED_CrossSiteIframeAgentHost
#else
#define MAYBE_CrossSiteIframeAgentHost CrossSiteIframeAgentHost
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessDevToolsBrowserTest,
                       MAYBE_CrossSiteIframeAgentHost) {
  DevToolsAgentHost::List list;
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(test_server()->GetURL("files/title1.html"));
  NavigateFrameToURL(child, http_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());

  // Load cross-site page into iframe.
  GURL::Replacements replace_host;
  GURL cross_site_url(test_server()->GetURL("files/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());
  EXPECT_EQ(DevToolsAgentHost::TYPE_FRAME, list[1]->GetType());
  EXPECT_EQ(cross_site_url.spec(), list[1]->GetURL().spec());

  // Attaching to child frame.
  scoped_refptr<DevToolsAgentHost> child_host = list[1];
  TestClient client;
  child_host->AttachClient(&client);

  // Load back same-site page into iframe.
  NavigateFrameToURL(root->child_at(0), http_url);

  list = DevToolsAgentHost::GetOrCreateAll();
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(DevToolsAgentHost::TYPE_WEB_CONTENTS, list[0]->GetType());
  EXPECT_EQ(main_url.spec(), list[0]->GetURL().spec());
  EXPECT_TRUE(client.closed());
  child_host->DetachClient();
  child_host = nullptr;
}

}  // namespace content
