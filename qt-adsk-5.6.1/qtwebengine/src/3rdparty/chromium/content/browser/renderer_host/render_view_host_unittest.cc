// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/filename_util.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "ui/base/page_transition_types.h"

namespace content {

class RenderViewHostTestBrowserClient : public TestContentBrowserClient {
 public:
  RenderViewHostTestBrowserClient() {}
  ~RenderViewHostTestBrowserClient() override {}

  bool IsHandledURL(const GURL& url) override {
    return url.scheme() == url::kFileScheme;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestBrowserClient);
};

class RenderViewHostTest : public RenderViewHostImplTestHarness {
 public:
  RenderViewHostTest() : old_browser_client_(NULL) {}
  ~RenderViewHostTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    old_browser_client_ = SetBrowserClientForTesting(&test_browser_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
  }

 private:
  RenderViewHostTestBrowserClient test_browser_client_;
  ContentBrowserClient* old_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTest);
};

// All about URLs reported by the renderer should get rewritten to about:blank.
// See RenderViewHost::OnNavigate for a discussion.
TEST_F(RenderViewHostTest, FilterAbout) {
  main_test_rfh()->NavigateAndCommitRendererInitiated(
      1, true, GURL("about:cache"));
  ASSERT_TRUE(controller().GetVisibleEntry());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            controller().GetVisibleEntry()->GetURL());
}

// Create a full screen popup RenderWidgetHost and View.
TEST_F(RenderViewHostTest, CreateFullscreenWidget) {
  int routing_id = process()->GetNextRoutingID();
  test_rvh()->CreateNewFullscreenWidget(routing_id);
}

// Makes sure that the RenderViewHost is not waiting for an unload ack when
// reloading a page. If this is not the case, when reloading, the contents may
// get closed out even though the user pressed the reload button.
TEST_F(RenderViewHostTest, ResetUnloadOnReload) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // This test is for a subtle timing bug. Here's the sequence that triggered
  // the bug:
  // . go to a page.
  // . go to a new page, preferably one that takes a while to resolve, such
  //   as one on a site that doesn't exist.
  //   . After this step IsWaitingForUnloadACK returns true on the first RVH.
  // . click stop before the page has been commited.
  // . click reload.
  //   . IsWaitingForUnloadACK still returns true, and if the hang monitor fires
  //     the contents gets closed.

  NavigateAndCommit(url1);
  controller().LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  // Simulate the ClosePage call which is normally sent by the net::URLRequest.
  test_rvh()->ClosePage();
  // Needed so that navigations are not suspended on the RFH.
  main_test_rfh()->SendBeforeUnloadACK(true);
  contents()->Stop();
  controller().Reload(false);
  EXPECT_FALSE(main_test_rfh()->IsWaitingForUnloadACK());
}

// Ensure we do not grant bindings to a process shared with unprivileged views.
TEST_F(RenderViewHostTest, DontGrantBindingsToSharedProcess) {
  // Create another view in the same process.
  scoped_ptr<TestWebContents> new_web_contents(
      TestWebContents::Create(browser_context(), rvh()->GetSiteInstance()));

  rvh()->AllowBindings(BINDINGS_POLICY_WEB_UI);
  EXPECT_FALSE(rvh()->GetEnabledBindings() & BINDINGS_POLICY_WEB_UI);
}

class MockDraggingRenderViewHostDelegateView
    : public RenderViewHostDelegateView {
 public:
  ~MockDraggingRenderViewHostDelegateView() override {}
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info) override {
    drag_url_ = drop_data.url;
    html_base_url_ = drop_data.html_base_url;
  }
  void UpdateDragCursor(blink::WebDragOperation operation) override {}
  void GotFocus() override {}
  void TakeFocus(bool reverse) override {}
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

  GURL drag_url() {
    return drag_url_;
  }

  GURL html_base_url() {
    return html_base_url_;
  }

 private:
  GURL drag_url_;
  GURL html_base_url_;
};

TEST_F(RenderViewHostTest, StartDragging) {
  TestWebContents* web_contents = contents();
  MockDraggingRenderViewHostDelegateView delegate_view;
  web_contents->set_delegate_view(&delegate_view);

  DropData drop_data;
  GURL file_url = GURL("file:///home/user/secrets.txt");
  drop_data.url = file_url;
  drop_data.html_base_url = file_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(GURL(url::kAboutBlankURL), delegate_view.drag_url());
  EXPECT_EQ(GURL(url::kAboutBlankURL), delegate_view.html_base_url());

  GURL http_url = GURL("http://www.domain.com/index.html");
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(http_url, delegate_view.drag_url());
  EXPECT_EQ(http_url, delegate_view.html_base_url());

  GURL https_url = GURL("https://www.domain.com/index.html");
  drop_data.url = https_url;
  drop_data.html_base_url = https_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(https_url, delegate_view.drag_url());
  EXPECT_EQ(https_url, delegate_view.html_base_url());

  GURL javascript_url = GURL("javascript:alert('I am a bookmarklet')");
  drop_data.url = javascript_url;
  drop_data.html_base_url = http_url;
  test_rvh()->TestOnStartDragging(drop_data);
  EXPECT_EQ(javascript_url, delegate_view.drag_url());
  EXPECT_EQ(http_url, delegate_view.html_base_url());
}

TEST_F(RenderViewHostTest, DragEnteredFileURLsStillBlocked) {
  DropData dropped_data;
  gfx::Point client_point;
  gfx::Point screen_point;
  // We use "//foo/bar" path (rather than "/foo/bar") since dragged paths are
  // expected to be absolute on any platforms.
  base::FilePath highlighted_file_path(FILE_PATH_LITERAL("//tmp/foo.html"));
  base::FilePath dragged_file_path(FILE_PATH_LITERAL("//tmp/image.jpg"));
  base::FilePath sensitive_file_path(FILE_PATH_LITERAL("//etc/passwd"));
  GURL highlighted_file_url = net::FilePathToFileURL(highlighted_file_path);
  GURL dragged_file_url = net::FilePathToFileURL(dragged_file_path);
  GURL sensitive_file_url = net::FilePathToFileURL(sensitive_file_path);
  dropped_data.url = highlighted_file_url;
  dropped_data.filenames.push_back(
      ui::FileInfo(dragged_file_path, base::FilePath()));

  rvh()->DragTargetDragEnter(dropped_data, client_point, screen_point,
                              blink::WebDragOperationNone, 0);

  int id = process()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  EXPECT_FALSE(policy->CanRequestURL(id, highlighted_file_url));
  EXPECT_FALSE(policy->CanReadFile(id, highlighted_file_path));
  EXPECT_TRUE(policy->CanRequestURL(id, dragged_file_url));
  EXPECT_TRUE(policy->CanReadFile(id, dragged_file_path));
  EXPECT_FALSE(policy->CanRequestURL(id, sensitive_file_url));
  EXPECT_FALSE(policy->CanReadFile(id, sensitive_file_path));
}

TEST_F(RenderViewHostTest, MessageWithBadHistoryItemFiles) {
  base::FilePath file_path;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file_path));
  file_path = file_path.AppendASCII("foo");
  EXPECT_EQ(0, process()->bad_msg_count());
  test_rvh()->TestOnUpdateStateWithFile(-1, file_path);
  EXPECT_EQ(1, process()->bad_msg_count());

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      process()->GetID(), file_path);
  test_rvh()->TestOnUpdateStateWithFile(-1, file_path);
  EXPECT_EQ(1, process()->bad_msg_count());
}

namespace {
void SetBadFilePath(const GURL& url,
                    const base::FilePath& file_path,
                    FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  params->page_state =
      PageState::CreateForTesting(url, false, "data", &file_path);
}
}

TEST_F(RenderViewHostTest, NavigationWithBadHistoryItemFiles) {
  GURL url("http://www.google.com");
  base::FilePath file_path;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file_path));
  file_path = file_path.AppendASCII("bar");
  auto set_bad_file_path_callback = base::Bind(SetBadFilePath, url, file_path);

  EXPECT_EQ(0, process()->bad_msg_count());
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url, false);
  main_test_rfh()->PrepareForCommit();
  contents()->GetMainFrame()->SendNavigateWithModificationCallback(
      1, 1, true, url, set_bad_file_path_callback);
  EXPECT_EQ(1, process()->bad_msg_count());

  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
      process()->GetID(), file_path);
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url, false);
  main_test_rfh()->PrepareForCommit();
  contents()->GetMainFrame()->SendNavigateWithModificationCallback(
      2, 2, true, url, set_bad_file_path_callback);
  EXPECT_EQ(1, process()->bad_msg_count());
}

TEST_F(RenderViewHostTest, RoutingIdSane) {
  RenderFrameHostImpl* root_rfh =
      contents()->GetFrameTree()->root()->current_frame_host();
  EXPECT_EQ(contents()->GetMainFrame(), root_rfh);
  EXPECT_EQ(test_rvh()->GetProcess(), root_rfh->GetProcess());
  EXPECT_NE(test_rvh()->GetRoutingID(), root_rfh->routing_id());
}

class TestSaveImageFromDataURL : public RenderMessageFilter {
 public:
  TestSaveImageFromDataURL(
      BrowserContext* context)
      : RenderMessageFilter(
            0,
            nullptr,
            context,
            context->GetRequestContext(),
            nullptr,
            nullptr,
            nullptr,
            nullptr) {
    Reset();
  }

  void Reset() {
    url_string_ = std::string();
    is_downloaded_ = false;
  }

  std::string& UrlString() const {
    return url_string_;
  }

  bool IsDownloaded() const {
    return is_downloaded_;
  }

  void Test(const std::string& url) {
    OnMessageReceived(ViewHostMsg_SaveImageFromDataURL(0, url));
  }

 protected:
  ~TestSaveImageFromDataURL() override {}
  void DownloadUrl(int render_view_id,
                   const GURL& url,
                   const Referrer& referrer,
                   const base::string16& suggested_name,
                   const bool use_prompt) const override {
    url_string_ = url.spec();
    is_downloaded_ = true;
  }

 private:
  mutable std::string url_string_;
  mutable bool is_downloaded_;
};

TEST_F(RenderViewHostTest, SaveImageFromDataURL) {
  scoped_refptr<TestSaveImageFromDataURL> tester(
      new TestSaveImageFromDataURL(browser_context()));

  tester->Reset();
  tester->Test("http://non-data-url.com");
  EXPECT_EQ(tester->UrlString(), "");
  EXPECT_FALSE(tester->IsDownloaded());

  const std::string data_url = "data:image/gif;base64,"
      "R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=";

  tester->Reset();
  tester->Test(data_url);
  EXPECT_EQ(tester->UrlString(), data_url);
  EXPECT_TRUE(tester->IsDownloaded());
}

}  // namespace content
