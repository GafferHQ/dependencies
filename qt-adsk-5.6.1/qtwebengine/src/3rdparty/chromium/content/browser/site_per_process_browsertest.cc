// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_frame_navigation_observer.h"
#include "ipc/ipc_security_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"

namespace content {

namespace {

// Helper function to send a postMessage and wait for a reply message.  The
// |post_message_script| is executed on the |sender_ftn| frame, and the sender
// frame is expected to post |reply_status| from the DOMAutomationController
// when it receives a reply.
void PostMessageAndWaitForReply(FrameTreeNode* sender_ftn,
                                const std::string& post_message_script,
                                const std::string& reply_status) {
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      sender_ftn->current_frame_host(),
      "window.domAutomationController.send(" + post_message_script + ");",
      &success));
  EXPECT_TRUE(success);

  content::DOMMessageQueue msg_queue;
  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    if (status == reply_status)
      break;
  }
}

// Helper function to extract and return "window.receivedMessages" from the
// |sender_ftn| frame.  This variable is used in post_message.html to count the
// number of messages received via postMessage by the current window.
int GetReceivedMessages(FrameTreeNode* ftn) {
  int received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      ftn->current_frame_host(),
      "window.domAutomationController.send(window.receivedMessages);",
      &received_messages));
  return received_messages;
}

class RedirectNotificationObserver : public NotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  RedirectNotificationObserver(int notification_type,
                               const NotificationSource& source);
  ~RedirectNotificationObserver() override;

  // Wait until the specified notification occurs.  If the notification was
  // emitted between the construction of this object and this call then it
  // returns immediately.
  void Wait();

  // Returns NotificationService::AllSources() if we haven't observed a
  // notification yet.
  const NotificationSource& source() const {
    return source_;
  }

  const NotificationDetails& details() const {
    return details_;
  }

  // NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  bool seen_;
  bool seen_twice_;
  bool running_;
  NotificationRegistrar registrar_;

  NotificationSource source_;
  NotificationDetails details_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(RedirectNotificationObserver);
};

RedirectNotificationObserver::RedirectNotificationObserver(
    int notification_type,
    const NotificationSource& source)
    : seen_(false),
      running_(false),
      source_(NotificationService::AllSources()) {
  registrar_.Add(this, notification_type, source);
}

RedirectNotificationObserver::~RedirectNotificationObserver() {}

void RedirectNotificationObserver::Wait() {
  if (seen_ && seen_twice_)
    return;

  running_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(seen_);
}

void RedirectNotificationObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  seen_twice_ = seen_;
  seen_ = true;
  if (!running_)
    return;

  message_loop_runner_->Quit();
  running_ = false;
}

// This observer keeps track of the number of created RenderFrameHosts.  Tests
// can use this to ensure that a certain number of child frames has been
// created after navigating.
class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  RenderFrameHostCreatedObserver(WebContents* web_contents,
                                 int expected_frame_count)
      : WebContentsObserver(web_contents),
        expected_frame_count_(expected_frame_count),
        frames_created_(0),
        message_loop_runner_(new MessageLoopRunner) {}

  ~RenderFrameHostCreatedObserver() override;

  // Runs a nested message loop and blocks until the expected number of
  // RenderFrameHosts is created.
  void Wait();

 private:
  // WebContentsObserver
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  // The number of RenderFrameHosts to wait for.
  int expected_frame_count_;

  // The number of RenderFrameHosts that have been created.
  int frames_created_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostCreatedObserver);
};

RenderFrameHostCreatedObserver::~RenderFrameHostCreatedObserver() {
}

void RenderFrameHostCreatedObserver::Wait() {
  message_loop_runner_->Run();
}

void RenderFrameHostCreatedObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  frames_created_++;
  if (frames_created_ == expected_frame_count_) {
    message_loop_runner_->Quit();
  }
}

// A WebContentsDelegate that catches messages sent to the console.
class ConsoleObserverDelegate : public WebContentsDelegate {
 public:
  ConsoleObserverDelegate(WebContents* web_contents, const std::string& filter)
      : web_contents_(web_contents),
        filter_(filter),
        message_(""),
        message_loop_runner_(new MessageLoopRunner) {}

  ~ConsoleObserverDelegate() override {}

  bool AddMessageToConsole(WebContents* source,
                           int32 level,
                           const base::string16& message,
                           int32 line_no,
                           const base::string16& source_id) override;

  std::string message() { return message_; }

  void Wait();

 private:
  WebContents* web_contents_;
  std::string filter_;
  std::string message_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleObserverDelegate);
};

void ConsoleObserverDelegate::Wait() {
  message_loop_runner_->Run();
}

bool ConsoleObserverDelegate::AddMessageToConsole(
    WebContents* source,
    int32 level,
    const base::string16& message,
    int32 line_no,
    const base::string16& source_id) {
  DCHECK(source == web_contents_);

  std::string ascii_message = base::UTF16ToASCII(message);
  if (base::MatchPattern(ascii_message, filter_)) {
    message_ = ascii_message;
    message_loop_runner_->Quit();
  }
  return false;
}

}  // namespace

//
// SitePerProcessBrowserTest
//

SitePerProcessBrowserTest::SitePerProcessBrowserTest() {
};

std::string SitePerProcessBrowserTest::DepictFrameTree(FrameTreeNode* node) {
  return visualizer_.DepictFrameTree(node);
}

void SitePerProcessBrowserTest::StartFrameAtDataURL() {
  std::string data_url_script =
      "var iframes = document.getElementById('test');iframes.src="
      "'data:text/html,dataurl';";
  ASSERT_TRUE(ExecuteScript(shell()->web_contents(), data_url_script));
}

void SitePerProcessBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kSitePerProcess);
};

void SitePerProcessBrowserTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  SetupCrossSiteRedirector(embedded_test_server());
}

// Ensure that navigating subframes in --site-per-process mode works and the
// correct documents are committed.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteIframe) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  {
    // There should be only one RenderWidgetHost when there are no
    // cross-process iframes.
    std::set<RenderWidgetHostView*> views_set =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetRenderWidgetHostViewsInTree();
    EXPECT_EQ(1U, views_set.size());
  }

  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        |--Site A\n"
      "        +--Site A\n"
      "             +--Site A\n"
      "Where A = http://127.0.0.1/",
      DepictFrameTree(root));

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), url);
  // Verify that the navigation succeeded and the expected URL was loaded.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  RenderViewHost* rvh = child->current_frame_host()->render_view_host();
  RenderProcessHost* rph = child->current_frame_host()->GetProcess();
  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(), rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);
  EXPECT_NE(shell()->web_contents()->GetRenderProcessHost(), rph);
  {
    // There should be now two RenderWidgetHosts, one for each process
    // rendering a frame.
    std::set<RenderWidgetHostView*> views_set =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetRenderWidgetHostViewsInTree();
    EXPECT_EQ(2U, views_set.size());
  }
  RenderFrameProxyHost* proxy_to_parent =
      child->render_manager()->GetProxyToParent();
  EXPECT_TRUE(proxy_to_parent);
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector());
  // The out-of-process iframe should have its own RenderWidgetHost,
  // independent of any RenderViewHost.
  EXPECT_NE(
      rvh->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());
  EXPECT_TRUE(child->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Load another cross-site page into the same iframe.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  NavigateFrameToURL(root->child_at(0), url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check again that a new process is created and is different from the
  // top level one and the previous one.
  ASSERT_EQ(2U, root->child_count());
  child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(),
            child->current_frame_host()->render_view_host());
  EXPECT_NE(rvh, child->current_frame_host()->render_view_host());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(shell()->web_contents()->GetRenderProcessHost(),
            child->current_frame_host()->GetProcess());
  EXPECT_NE(rph, child->current_frame_host()->GetProcess());
  {
    std::set<RenderWidgetHostView*> views_set =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetRenderWidgetHostViewsInTree();
    EXPECT_EQ(2U, views_set.size());
  }
  EXPECT_EQ(proxy_to_parent, child->render_manager()->GetProxyToParent());
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector());
  EXPECT_NE(
      child->current_frame_host()->render_view_host()->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());
  EXPECT_TRUE(child->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://127.0.0.1/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));
}

// Tests OOPIF rendering by checking that the RWH of the iframe generates
// OnSwapCompositorFrame message.
#if defined(OS_ANDROID)
// http://crbug.com/471850
#define MAYBE_CompositorFrameSwapped DISABLED_CompositorFrameSwapped
#else
#define MAYBE_CompositorFrameSwapped CompositorFrameSwapped
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       MAYBE_CompositorFrameSwapped) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());
  RenderWidgetHostViewBase* rwhv_base = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Wait for OnSwapCompositorFrame message.
  while (rwhv_base->RendererFrameNumber() <= 0) {
    // TODO(lazyboy): Find a better way to avoid sleeping like this. See
    // http://crbug.com/405282 for details.
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(10));
    run_loop.Run();
  }
}

// Ensure that OOPIFs are deleted after navigating to a new main frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CleanupCrossSiteIframe) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load a cross-site page into both iframes.
  GURL foo_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());
  NavigateFrameToURL(root->child_at(1), foo_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframes.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  int subframe_process_id = root->child_at(0)
                                ->current_frame_host()
                                ->GetSiteInstance()
                                ->GetProcess()
                                ->GetID();
  int subframe_rvh_id = root->child_at(0)
                            ->current_frame_host()
                            ->render_view_host()
                            ->GetRoutingID();
  EXPECT_TRUE(RenderViewHost::FromID(subframe_process_id, subframe_rvh_id));

  // Use Javascript in the parent to remove one of the frames and ensure that
  // the subframe goes away.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.body.removeChild("
                            "document.querySelectorAll('iframe')[0])"));
  ASSERT_EQ(1U, root->child_count());

  // Load a new same-site page in the top-level frame and ensure the other
  // subframe goes away.
  GURL new_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), new_url);
  ASSERT_EQ(0U, root->child_count());

  // Ensure the RVH for the subframe gets cleaned up when the frame goes away.
  EXPECT_FALSE(RenderViewHost::FromID(subframe_process_id, subframe_rvh_id));
}

// Ensure that root frames cannot be detached.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, RestrictFrameDetach) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site pages into both iframes.
  GURL foo_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());
  GURL bar_url = embedded_test_server()->GetURL("bar.com", "/title2.html");
  NavigateFrameToURL(root->child_at(1), bar_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(bar_url, observer.last_navigation_url());

  // Ensure that we have created new processes for the subframes.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* foo_child = root->child_at(0);
  SiteInstance* foo_site_instance =
      foo_child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), foo_site_instance);
  FrameTreeNode* bar_child = root->child_at(1);
  SiteInstance* bar_site_instance =
      bar_child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), bar_site_instance);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://foo.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Simulate an attempt to detach the root frame from foo_site_instance.  This
  // should kill foo_site_instance's process.
  RenderFrameProxyHost* foo_mainframe_rfph =
      root->render_manager()->GetRenderFrameProxyHost(foo_site_instance);
  content::RenderProcessHostWatcher foo_terminated(
      foo_mainframe_rfph->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  FrameHostMsg_Detach evil_msg2(foo_mainframe_rfph->GetRoutingID());
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      foo_mainframe_rfph->GetProcess()->GetChannel(), evil_msg2);
  foo_terminated.Wait();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://foo.com/ (no process)\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateRemoteFrame) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);

  // Emulate the main frame changing the src of the iframe such that it
  // navigates cross-site.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  NavigateIframeToURL(shell()->web_contents(), "test", url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check again that a new process is created and is different from the
  // top level one and the previous one.
  ASSERT_EQ(2U, root->child_count());
  child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(site_instance,
            child->current_frame_host()->GetSiteInstance());

  // Navigate back to the parent's origin and ensure we return to the
  // parent's process.
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
}

#if defined(OS_WIN)
// http://crbug.com/465722
#define MAYBE_NavigateRemoteFrameToBlankAndDataURLs \
    DISABLED_NavigateRemoteFrameToBlankAndDataURLs
#else
#define MAYBE_NavigateRemoteFrameToBlankAndDataURLs \
    NavigateRemoteFrameToBlankAndDataURLs
#endif

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       MAYBE_NavigateRemoteFrameToBlankAndDataURLs) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  ASSERT_EQ(2U, root->child_count());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Navigate iframe to a data URL. The navigation happens from a script in the
  // parent frame, so the data URL should be committed in the same SiteInstance
  // as the parent frame.
  GURL data_url("data:text/html,dataurl");
  NavigateIframeToURL(shell()->web_contents(), "test", data_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url, observer.last_navigation_url());

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Load cross-site page into iframe.
  url = embedded_test_server()->GetURL("bar.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  ASSERT_EQ(2U, root->child_count());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Navigate iframe to about:blank. The navigation happens from a script in the
  // parent frame, so it should be committed in the same SiteInstance as the
  // parent frame.
  GURL about_blank_url("about:blank");
  NavigateIframeToURL(shell()->web_contents(), "test", about_blank_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(about_blank_url, observer.last_navigation_url());

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
}

// This test checks that killing a renderer process of a remote frame
// and then navigating some other frame to the same SiteInstance of the killed
// process works properly.
// This can be illustrated as follows,
// where 1/2/3 are FrameTreeNode-s and A/B are processes and B* is the killed
// B process:
//
//     1        A                  A                           A
//    / \  ->  / \  -> Kill B ->  / \  -> Navigate 3 to B ->  / \  .
//   2   3    B   A              B*  A                       B*  B
//
// Initially, node1.proxy_hosts_ = {B}
// After we kill B, we make sure B stays in node1.proxy_hosts_, then we navigate
// 3 to B and we expect that to complete normally.
// See http://crbug.com/432107.
//
// Note that due to http://crbug.com/450681, node2 cannot be re-navigated to
// site B and stays in not rendered state.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToKilledProcess) {
  GURL main_url(embedded_test_server()->GetURL(
                    "/frame_tree/page_with_two_frames.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure node2 points to the correct cross-site page.
  GURL site_b_url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_EQ(site_b_url, node2->current_url());

  // Kill that cross-site renderer.
  RenderProcessHost* child_process =
      node2->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();

  // Now navigate the second iframe (node3) to the same site as the node2.
  FrameTreeNode* node3 = root->child_at(1);
  NavigateFrameToURL(node3, site_b_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(site_b_url, observer.last_navigation_url());
}

// This test is similar to
// SitePerProcessBrowserTest.NavigateRemoteFrameToKilledProcess with
// addition that node2 also has a cross-origin frame to site C.
//
//     1          A                  A                       A
//    / \        / \                / \                     / \  .
//   2   3 ->   B   A -> Kill B -> B*   A -> Navigate 3 -> B*  B
//  /          /
// 4          C
//
// Initially, node1.proxy_hosts_ = {B, C}
// After we kill B, we make sure B stays in node1.proxy_hosts_, but
// C gets cleared from node1.proxy_hosts_.
//
// Note that due to http://crbug.com/450681, node2 cannot be re-navigated to
// site B and stays in not rendered state.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToKilledProcessWithSubtree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames_nested.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  ASSERT_EQ(2U, root->child_count());

  GURL site_b_url(
      embedded_test_server()->GetURL(
          "bar.com", "/frame_tree/page_with_one_frame.html"));
  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(site_b_url, root->child_at(0)->current_url());

  // Ensure that a new process is created for node2.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  // Ensure that a new process is *not* created for node3.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  ASSERT_EQ(1U, root->child_at(0)->child_count());

  // Make sure node4 points to the correct cross-site page.
  FrameTreeNode* node4 = root->child_at(0)->child_at(0);
  GURL site_c_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_c_url, node4->current_url());

  // |site_instance_c| is expected to go away once we kill |child_process_b|
  // below, so create a local scope so we can extend the lifetime of
  // |site_instance_c| with a refptr.
  {
    // Initially each frame has proxies for the other sites.
    EXPECT_EQ(
        " Site A ------------ proxies for B C\n"
        "   |--Site B ------- proxies for A C\n"
        "   |    +--Site C -- proxies for A B\n"
        "   +--Site A ------- proxies for B C\n"
        "Where A = http://a.com/\n"
        "      B = http://bar.com/\n"
        "      C = http://baz.com/",
        DepictFrameTree(root));

    // Kill the render process for Site B.
    RenderProcessHost* child_process_b =
        root->child_at(0)->current_frame_host()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        child_process_b, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process_b->Shutdown(0, false);
    crash_observer.Wait();

    // The Site C frame (a child of the crashed Site B frame) should go away,
    // and there should be no remaining proxies for site C anywhere.
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   +--Site A ------- proxies for B\n"
        "Where A = http://a.com/\n"
        "      B = http://bar.com/ (no process)",
        DepictFrameTree(root));
  }

  // Now navigate the second iframe (node3) to Site B also.
  FrameTreeNode* node3 = root->child_at(1);
  GURL url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  NavigateFrameToURL(node3, url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));
}

// Verify that killing a cross-site frame's process B and then navigating a
// frame to B correctly recreates all proxies in B.
//
//      1           A                    A          A
//    / | \       / | \                / | \      / | \  .
//   2  3  4 ->  B  A  A -> Kill B -> B* A  A -> B* B  A
//
// After the last step, the test sends a postMessage from node 3 to node 4,
// verifying that a proxy for node 4 has been recreated in process B.  This
// verifies the fix for https://crbug.com/478892.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigatingToKilledProcessRestoresAllProxies) {
  // Navigate to a page with three frames: one cross-site and two same-site.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_three_frames.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Kill the first subframe's b.com renderer.
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();

  // Navigate the second subframe to b.com to recreate the b.com process.
  GURL b_url = embedded_test_server()->GetURL("b.com", "/post_message.html");
  NavigateFrameToURL(root->child_at(1), b_url);
  // TODO(alexmos): This can be removed once TestFrameNavigationObserver is
  // fixed to use DidFinishLoad.
  EXPECT_TRUE(
      WaitForRenderFrameReady(root->child_at(1)->current_frame_host()));
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(b_url, observer.last_navigation_url());
  EXPECT_TRUE(root->child_at(1)->current_frame_host()->IsRenderFrameLive());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Check that third subframe's proxy is available in the b.com process by
  // sending it a postMessage from second subframe, and waiting for a reply.
  PostMessageAndWaitForReply(root->child_at(1),
                             "postToSibling('subframe-msg','frame3')",
                             "\"done-frame2\"");
}

// Verify that proxy creation doesn't recreate a crashed process if no frame
// will be created in it.
//
//      1           A                    A          A
//    / | \       / | \                / | \      / | \    .
//   2  3  4 ->  B  A  A -> Kill B -> B* A  A -> B* A  A
//                                                      \  .
//                                                       A
//
// The test kills process B (node 2), creates a child frame of node 4 in
// process A, and then checks that process B isn't resurrected to create a
// proxy for the new child frame.  See https://crbug.com/476846.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CreateChildFrameAfterKillingProcess) {
  // Navigate to a page with three frames: one cross-site and two same-site.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_three_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  SiteInstance* b_site_instance =
      root->child_at(0)->current_frame_host()->GetSiteInstance();

  // Kill the first subframe's renderer (B).
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();

  // Add a new child frame to the third subframe.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      root->child_at(2)->current_frame_host(),
      "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  // The new frame should have a RenderFrameProxyHost for B, but it should not
  // be alive, and B should still not have a process (verified by last line of
  // expected DepictFrameTree output).
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/ (no process)",
      DepictFrameTree(root));
  FrameTreeNode* grandchild = root->child_at(2)->child_at(0);
  RenderFrameProxyHost* grandchild_rfph =
      grandchild->render_manager()->GetRenderFrameProxyHost(b_site_instance);
  EXPECT_FALSE(grandchild_rfph->is_render_frame_proxy_live());

  // Navigate the second subframe to b.com to recreate process B.
  TestNavigationObserver observer(shell()->web_contents());
  GURL b_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  NavigateFrameToURL(root->child_at(1), b_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(b_url, observer.last_navigation_url());

  // Ensure that the grandchild RenderFrameProxy in B was created when process
  // B was restored.
  EXPECT_TRUE(grandchild_rfph->is_render_frame_proxy_live());
}

// Verify that creating a child frame after killing and reloading an opener
// process doesn't crash. See https://crbug.com/501152.
//   1. Navigate to site A.
//   2. Open a popup with window.open and navigate it cross-process to site B.
//   3. Kill process A for the original tab.
//   4. Reload the original tab to resurrect process A.
//   5. Add a child frame to the top-level frame in the popup tab B.
// In step 5, we try to create proxies for the child frame in all SiteInstances
// for which its parent has proxies.  This includes A.  However, even though
// process A is live (step 4), the parent proxy in A is not live (which was
// incorrectly assumed previously).  This is because step 4 does not resurrect
// proxies for popups opened before the crash.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CreateChildFrameAfterKillingOpener) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();

  // Open a popup and navigate it cross-process to b.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(),
                            "popup = window.open('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(popup, popup_url));

  // Verify that each top-level frame has proxies in the other's SiteInstance.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(popup_root));

  // Kill the first window's renderer (a.com).
  RenderProcessHost* child_process = root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();
  EXPECT_FALSE(root->current_frame_host()->IsRenderFrameLive());

  // The proxy for the popup in a.com should've died.
  RenderFrameProxyHost* rfph =
      popup_root->render_manager()->GetRenderFrameProxyHost(site_instance_a);
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());

  // Recreate the a.com renderer.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // The popup's proxy in a.com should still not be live. Re-navigating the
  // main window to a.com doesn't reinitialize a.com proxies for popups
  // previously opened from the main window.
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());

  // Add a new child frame on the popup.
  RenderFrameHostCreatedObserver frame_observer(popup->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      popup->web_contents(),
      "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  // Both the child frame's and its parent's proxies should still not be live.
  // The main page can't reach them since it lost reference to the popup after
  // it crashed, so there is no need to create them.
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());
  RenderFrameProxyHost* child_rfph =
      popup_root->child_at(0)->render_manager()->GetRenderFrameProxyHost(
          site_instance_a);
  EXPECT_TRUE(child_rfph);
  EXPECT_FALSE(child_rfph->is_render_frame_proxy_live());
}

// In A-embed-B-embed-C scenario, verify that killing process B clears proxies
// of C from the tree.
//
//     1          A                  A
//    / \        / \                / \    .
//   2   3 ->   B   A -> Kill B -> B*  A
//  /          /
// 4          C
//
// node1 is the root.
// Initially, both node1.proxy_hosts_ and node3.proxy_hosts_ contain C.
// After we kill B, make sure proxies for C are cleared.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       KillingRendererClearsDescendantProxies) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames_nested.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();
  ASSERT_EQ(2U, root->child_count());

  GURL site_b_url(
      embedded_test_server()->GetURL(
          "bar.com", "/frame_tree/page_with_one_frame.html"));
  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(site_b_url, root->child_at(0)->current_url());

  // Ensure that a new process is created for node2.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  // Ensure that a new process is *not* created for node3.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  ASSERT_EQ(1U, root->child_at(0)->child_count());

  // Make sure node4 points to the correct cross-site-page.
  FrameTreeNode* node4 = root->child_at(0)->child_at(0);
  GURL site_c_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_c_url, node4->current_url());

  // |site_instance_c|'s frames and proxies are expected to go away once we kill
  // |child_process_b| below.
  scoped_refptr<SiteInstanceImpl> site_instance_c =
      node4->current_frame_host()->GetSiteInstance();

  // Initially proxies for both B and C will be present in the root.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site C -- proxies for A B\n"
      "   +--Site A ------- proxies for B C\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  EXPECT_GT(site_instance_c->active_frame_count(), 0U);

  // Kill process B.
  RenderProcessHost* child_process_b =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process_b, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process_b->Shutdown(0, false);
  crash_observer.Wait();

  // Make sure proxy C has gone from root.
  // Make sure proxy C has gone from node3 as well.
  // Make sure proxy B stays around in root and node3.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/ (no process)",
      DepictFrameTree(root));

  EXPECT_EQ(0U, site_instance_c->active_frame_count());
}

// Crash a subframe and ensures its children are cleared from the FrameTree.
// See http://crbug.com/338508.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrashSubframe) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  StartFrameAtDataURL();

  // Load cross-site page into iframe.
  EXPECT_TRUE(NavigateIframeToURL(
      shell()->web_contents(), "test",
      embedded_test_server()->GetURL("/cross-site/foo.com/title2.html")));

  // Check the subframe process.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ("foo.com", child->current_url().host());
  EXPECT_EQ("/title2.html", child->current_url().path());

  EXPECT_TRUE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());

  // Crash the subframe process.
  RenderProcessHost* root_process = root->current_frame_host()->GetProcess();
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  {
    RenderProcessHostWatcher crash_observer(
        child_process,
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0, false);
    crash_observer.Wait();
  }

  // Ensure that the child frame still exists but has been cleared.
  EXPECT_EQ(2U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(GURL(), child->current_url());

  EXPECT_FALSE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(child->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(child->current_frame_host()->render_frame_created_);

  // Now crash the top-level page to clear the child frame.
  {
    RenderProcessHostWatcher crash_observer(
        root_process,
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    root_process->Shutdown(0, false);
    crash_observer.Wait();
  }
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(GURL(), root->current_url());
}

// When a new subframe is added, related SiteInstances that can reach the
// subframe should create proxies for it (https://crbug.com/423587).  This test
// checks that if A embeds B and later adds a new subframe A2, A2 gets a proxy
// in B's process.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CreateProxiesForNewFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  // Make sure the frame starts out at the correct cross-site URL.
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
            root->child_at(0)->current_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://b.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Add a new child frame to the top-level frame.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.domAutomationController.send("
                            "    addFrame('data:text/html,foo'));"));
  frame_observer.Wait();

  // The new frame should have a proxy in Site B, for use by the old frame.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://b.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
// TODO(creis): Replace SpawnedTestServer with host_resolver to get test to run
// on Android (http://crbug.com/187570).
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectOnce) {
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  GURL http_url(test_server()->GetURL("files/title1.html"));
  GURL https_url(https_server.GetURL("files/title1.html"));

  NavigateToURL(shell(), main_url);

  TestNavigationObserver observer(shell()->web_contents());
  {
    // Load cross-site client-redirect page into Iframe.
    // Should be blocked.
    GURL client_redirect_https_url(https_server.GetURL(
        "client-redirect?files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_https_url));
    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(https_server.GetURL(
        "server-redirect?" + http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(https_server.GetURL(
        "server-redirect?files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
   }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL client_redirect_http_url(test_server()->GetURL(
        "client-redirect?" + https_url.spec()));

    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // Redirecting to Cross-site Page should be blocked.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?files/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
   }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to same-site page.
    GURL client_redirect_http_url(test_server()->GetURL(
        "client-redirect?" + http_url.spec()));
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // Redirecting to Same-site Page should be loaded successfully.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
// TODO(creis): Replace SpawnedTestServer with host_resolver to get test to run
// on Android (http://crbug.com/187570).
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectTwice) {
  ASSERT_TRUE(test_server()->Start());
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  GURL http_url(test_server()->GetURL("files/title1.html"));
  GURL https_url(https_server.GetURL("files/title1.html"));

  NavigateToURL(shell(), main_url);

  TestNavigationObserver observer(shell()->web_contents());
  {
    // Load client-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_https_url(https_server.GetURL(
        "client-redirect?" + http_url.spec()));
    GURL client_redirect_http_url(test_server()->GetURL(
        "client-redirect?" + client_redirect_https_url.spec()));

    // We should wait until second client redirect get cancelled.
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirect back to same-site page.
    GURL server_redirect_https_url(https_server.GetURL(
        "server-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirects back to cross-site page.
    GURL server_redirect_https_url(https_server.GetURL(
        "server-redirect?" + https_url.spec()));
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_http_url(https_server.GetURL(
        "client-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(test_server()->GetURL(
        "server-redirect?" + client_redirect_http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    // DidFailProvisionalLoad when navigating to client_redirect_http_url.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }
}

// Ensure that when navigating a frame cross-process RenderFrameProxyHosts are
// created in the FrameTree skipping the subtree of the navigating frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ProxyCreationSkipsSubtree) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  EXPECT_TRUE(root->child_at(1) != NULL);
  EXPECT_EQ(2U, root->child_at(1)->child_count());

  {
    // Load same-site page into iframe.
    TestNavigationObserver observer(shell()->web_contents());
    GURL http_url(embedded_test_server()->GetURL("/title1.html"));
    NavigateFrameToURL(root->child_at(0), http_url);
    EXPECT_EQ(http_url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(
        " Site A\n"
        "   |--Site A\n"
        "   +--Site A\n"
        "        |--Site A\n"
        "        +--Site A\n"
        "             +--Site A\n"
        "Where A = http://127.0.0.1/",
        DepictFrameTree(root));
  }

  // Create the cross-site URL to navigate to.
  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/title2.html");

  // Load cross-site page into the second iframe without waiting for the
  // navigation to complete. Once LoadURLWithParams returns, we would expect
  // proxies to have been created in the frame tree, but children of the
  // navigating frame to still be present. The reason is that we don't run the
  // message loop, so no IPCs that alter the frame tree can be processed.
  FrameTreeNode* child = root->child_at(1);
  SiteInstance* site = NULL;
  bool browser_side_navigation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation);
  std::string cross_site_rfh_type =
      browser_side_navigation ? "speculative" : "pending";
  {
    TestNavigationObserver observer(shell()->web_contents());
    TestFrameNavigationObserver navigation_observer(child);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator()->GetController()->LoadURLWithParams(params);

    if (browser_side_navigation) {
      site = child->render_manager()
                 ->speculative_frame_host()
                 ->GetSiteInstance();
    } else {
      site = child->render_manager()->pending_frame_host()->GetSiteInstance();
    }
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site);

    std::string tree = base::StringPrintf(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site A (B %s)\n"
        "        |--Site A\n"
        "        +--Site A\n"
        "             +--Site A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        cross_site_rfh_type.c_str());
    EXPECT_EQ(tree, DepictFrameTree(root));

    // Now that the verification is done, run the message loop and wait for the
    // navigation to complete.
    navigation_observer.Wait();
    EXPECT_FALSE(child->render_manager()->pending_frame_host());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(cross_site_url, observer.last_navigation_url());

    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        DepictFrameTree(root));
  }

  // Load another cross-site page into the same iframe.
  cross_site_url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    // Perform the same checks as the first cross-site navigation, since
    // there have been issues in subsequent cross-site navigations. Also ensure
    // that the SiteInstance has properly changed.
    // TODO(nasko): Once we have proper cleanup of resources, add code to
    // verify that the intermediate SiteInstance/RenderFrameHost have been
    // properly cleaned up.
    TestNavigationObserver observer(shell()->web_contents());
    TestFrameNavigationObserver navigation_observer(child);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator()->GetController()->LoadURLWithParams(params);

    SiteInstance* site2;
    if (browser_side_navigation) {
      site2 = child->render_manager()
                  ->speculative_frame_host()
                  ->GetSiteInstance();
    } else {
      site2 = child->render_manager()->pending_frame_host()->GetSiteInstance();
    }
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site2);
    EXPECT_NE(site, site2);

    std::string tree = base::StringPrintf(
        " Site A ------------ proxies for B C\n"
        "   |--Site A ------- proxies for B C\n"
        "   +--Site B (C %s) -- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/\n"
        "      C = http://bar.com/",
        cross_site_rfh_type.c_str());
    EXPECT_EQ(tree, DepictFrameTree(root));

    navigation_observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(cross_site_url, observer.last_navigation_url());
    EXPECT_EQ(0U, child->child_count());
  }
}

// Verify that origin replication works for an A-embed-B-embed-C hierarchy.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, OriginReplication) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Navigate the first subframe to a cross-site page with two subframes.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/frame_tree/1-1.html"));
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(foo_url, root->child_at(0)->current_url());

  // Ensure that a new process is created for the subframe.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Load cross-site page into subframe's subframe.
  ASSERT_EQ(2U, root->child_at(0)->child_count());
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0)->child_at(0), bar_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(bar_url, observer.last_navigation_url());

  // Check that a new process is created and is different from the top one and
  // the middle one.
  FrameTreeNode* bottom_child = root->child_at(0)->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            bottom_child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->child_at(0)->current_frame_host()->GetSiteInstance(),
            bottom_child->current_frame_host()->GetSiteInstance());

  // Check that foo.com frame's location.ancestorOrigins contains the correct
  // origin for the parent.  The origin should have been replicated as part of
  // the ViewMsg_New message that created the parent's RenderFrameProxy in
  // foo.com's process.
  int ancestor_origins_length = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(0)->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(1, ancestor_origins_length);
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(0)->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ(result + "/", main_url.GetOrigin().spec());

  // Check that bar.com frame's location.ancestorOrigins contains the correct
  // origin for its two ancestors. The topmost parent origin should be
  // replicated as part of ViewMsg_New, and the middle frame (foo.com's) origin
  // should be replicated as part of FrameMsg_NewFrameProxy sent for foo.com's
  // frame in bar.com's process.
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(2, ancestor_origins_length);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ(result + "/", foo_url.GetOrigin().spec());
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins[1]);",
      &result));
  EXPECT_EQ(result + "/", main_url.GetOrigin().spec());
}

// Check that iframe sandbox flags are replicated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, SandboxFlagsReplication) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Navigate the second (sandboxed) subframe to a cross-site page with a
  // subframe.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/frame_tree/1-1.html"));
  NavigateFrameToURL(root->child_at(1), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(foo_url, root->child_at(1)->current_url());

  // Load cross-site page into subframe's subframe.
  ASSERT_EQ(2U, root->child_at(1)->child_count());
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1)->child_at(0), bar_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(bar_url, observer.last_navigation_url());

  // Opening a popup in the sandboxed foo.com iframe should fail.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(root->child_at(1)->current_frame_host(),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Opening a popup in a frame whose parent is sandboxed should also fail.
  // Here, bar.com frame's sandboxed parent frame is a remote frame in
  // bar.com's process.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1)->child_at(0)->current_frame_host(),
      "window.domAutomationController.send("
      "!window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Same, but now try the case where bar.com frame's sandboxed parent is a
  // local frame in bar.com's process.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(2)->child_at(0)->current_frame_host(),
      "window.domAutomationController.send("
      "!window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Check that foo.com frame's location.ancestorOrigins contains the correct
  // origin for the parent, which should be unaffected by sandboxing.
  int ancestor_origins_length = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(1)->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(1, ancestor_origins_length);
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(1)->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ(result + "/", main_url.GetOrigin().spec());

  // Now check location.ancestorOrigins for the bar.com frame. The middle frame
  // (foo.com's) origin should be unique, since that frame is sandboxed, and
  // the top frame should match |main_url|.
  FrameTreeNode* bottom_child = root->child_at(1)->child_at(0);
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(2, ancestor_origins_length);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ("null", result);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins[1]);",
      &result));
  EXPECT_EQ(main_url.GetOrigin().spec(), result + "/");
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, DynamicSandboxFlags) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure first frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());

  // Navigate second frame to another cross-site page.
  GURL baz_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1), baz_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(baz_url, observer.last_navigation_url());

  // Both frames should not be sandboxed to start with.
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(0)->current_replication_state().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(0)->effective_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(1)->current_replication_state().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(1)->effective_sandbox_flags());

  // Dynamically update sandbox flags for the first frame.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.domAutomationController.send("
                            "document.querySelector('iframe').sandbox="
                            "'allow-scripts');"));

  // Check that updated sandbox flags are propagated to browser process.
  // The new flags should be set in current_replication_state(), while
  // effective_sandbox_flags() should still reflect the old flags, because
  // sandbox flag updates take place only after navigations. "allow-scripts"
  // resets both SandboxFlags::Scripts and SandboxFlags::AutomaticFeatures bits
  // per blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::All & ~blink::WebSandboxFlags::Scripts &
      ~blink::WebSandboxFlags::AutomaticFeatures;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->current_replication_state().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(0)->effective_sandbox_flags());

  // Navigate the first frame to a page on the same site.  The new sandbox
  // flags should take effect.
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/frame_tree/2-4.html"));
  NavigateFrameToURL(root->child_at(0), bar_url);
  // (The new page has a subframe; wait for it to load as well.)
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(bar_url, root->child_at(0)->current_url());
  ASSERT_EQ(1U, root->child_at(0)->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site B -- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // Confirm that the browser process has updated the frame's current sandbox
  // flags.
  EXPECT_EQ(expected_flags,
            root->child_at(0)->current_replication_state().sandbox_flags);
  EXPECT_EQ(expected_flags, root->child_at(0)->effective_sandbox_flags());

  // Opening a popup in the now-sandboxed frame should fail.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(root->child_at(0)->current_frame_host(),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Navigate the child of the now-sandboxed frame to a page on baz.com.  The
  // child should inherit the latest sandbox flags from its parent frame, which
  // is currently a proxy in baz.com's renderer process.  This checks that the
  // proxies of |root->child_at(0)| were also updated with the latest sandbox
  // flags.
  GURL baz_child_url(embedded_test_server()->GetURL("baz.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0)->child_at(0), baz_child_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(baz_child_url, observer.last_navigation_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site C -- proxies for A B\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // Opening a popup in the child of a sandboxed frame should fail.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0)->child_at(0)->current_frame_host(),
      "window.domAutomationController.send("
      "!window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DynamicSandboxFlagsRemoteToLocal) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure the two frames starts out at correct URLs.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html"),
            root->child_at(1)->current_url());

  // Update the second frame's sandbox flags.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.domAutomationController.send("
                            "document.querySelectorAll('iframe')[1].sandbox="
                            "'allow-scripts');"));

  // Check that the current sandbox flags are updated but the effective
  // sandbox flags are not.
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::All & ~blink::WebSandboxFlags::Scripts &
      ~blink::WebSandboxFlags::AutomaticFeatures;
  EXPECT_EQ(expected_flags,
            root->child_at(1)->current_replication_state().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(1)->effective_sandbox_flags());

  // Navigate the second subframe to a page on bar.com.  This will trigger a
  // remote-to-local frame swap in bar.com's process.  The target page has
  // another frame, so use TestFrameNavigationObserver to wait for all frames
  // to be loaded.
  TestFrameNavigationObserver frame_observer(root->child_at(1), 2);
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_one_frame.html"));
  NavigateFrameToURL(root->child_at(1), bar_url);
  frame_observer.Wait();
  EXPECT_EQ(bar_url, root->child_at(1)->current_url());
  ASSERT_EQ(1U, root->child_at(1)->child_count());

  // Confirm that the browser process has updated the current sandbox flags.
  EXPECT_EQ(expected_flags,
            root->child_at(1)->current_replication_state().sandbox_flags);
  EXPECT_EQ(expected_flags, root->child_at(1)->effective_sandbox_flags());

  // Opening a popup in the sandboxed second frame should fail.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(root->child_at(1)->current_frame_host(),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Make sure that the child frame inherits the sandbox flags of its
  // now-sandboxed parent frame.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1)->child_at(0)->current_frame_host(),
      "window.domAutomationController.send("
      "!window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DynamicSandboxFlagsRendererInitiatedNavigation) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(1U, root->child_count());

  // Make sure the frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
            root->child_at(0)->current_url());

  // The frame should not be sandboxed to start with.
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(0)->current_replication_state().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(0)->effective_sandbox_flags());

  // Dynamically update the frame's sandbox flags.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.domAutomationController.send("
                            "document.querySelector('iframe').sandbox="
                            "'allow-scripts');"));

  // Check that updated sandbox flags are propagated to browser process.
  // The new flags should be set in current_replication_state(), while
  // effective_sandbox_flags() should still reflect the old flags, because
  // sandbox flag updates take place only after navigations. "allow-scripts"
  // resets both SandboxFlags::Scripts and SandboxFlags::AutomaticFeatures bits
  // per blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::All & ~blink::WebSandboxFlags::Scripts &
      ~blink::WebSandboxFlags::AutomaticFeatures;
  EXPECT_EQ(expected_flags,
            root->child_at(0)->current_replication_state().sandbox_flags);
  EXPECT_EQ(blink::WebSandboxFlags::None,
            root->child_at(0)->effective_sandbox_flags());

  // Perform a renderer-initiated same-site navigation in the first frame. The
  // new sandbox flags should take effect.
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  ASSERT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "window.location.href='/title2.html'"));
  frame_observer.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title2.html"),
            root->child_at(0)->current_url());

  // Confirm that the browser process has updated the frame's current sandbox
  // flags.
  EXPECT_EQ(expected_flags,
            root->child_at(0)->current_replication_state().sandbox_flags);
  EXPECT_EQ(expected_flags, root->child_at(0)->effective_sandbox_flags());

  // Opening a popup in the now-sandboxed frame should fail.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(root->child_at(0)->current_frame_host(),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that when a new child frame is added, the proxies created for it in
// other SiteInstances have correct sandbox flags and origin.
//
//     A         A           A
//    /         / \         / \    .
//   B    ->   B   A   ->  B   A
//                              \  .
//                               B
//
// The test checks sandbox flags and origin for the proxy added in step 2, by
// checking whether the grandchild frame added in step 3 sees proper sandbox
// flags and origin for its (remote) parent.  This wasn't addressed when
// https://crbug.com/423587 was fixed.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ProxiesForNewChildFramesHaveCorrectReplicationState) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // In the root frame, add a new sandboxed local frame, which itself has a
  // child frame on baz.com.  Wait for three RenderFrameHosts to be created:
  // the new sandboxed local frame, its child (while it's still local), and a
  // pending RFH when starting the cross-site navigation to baz.com.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 3);
  EXPECT_TRUE(
      ExecuteScript(root->current_frame_host(),
                    "window.domAutomationController.send("
                    "    addFrame('/frame_tree/page_with_one_frame.html',"
                    "             'allow-scripts allow-same-origin'))"));
  frame_observer.Wait();

  // Wait for the cross-site navigation to baz.com in the grandchild to finish.
  FrameTreeNode* bottom_child = root->child_at(1)->child_at(0);
  TestFrameNavigationObserver navigation_observer(bottom_child);
  navigation_observer.Wait();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Use location.ancestorOrigins to check that the grandchild on baz.com sees
  // correct origin for its parent.
  int ancestor_origins_length = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(2, ancestor_origins_length);
  std::string parent_origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child->current_frame_host(),
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &parent_origin));
  EXPECT_EQ(main_url.GetOrigin().spec(), parent_origin + "/");

  // Check that the sandbox flags in the browser process are correct.
  // "allow-scripts" resets both WebSandboxFlags::Scripts and
  // WebSandboxFlags::AutomaticFeatures bits per blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::All & ~blink::WebSandboxFlags::Scripts &
      ~blink::WebSandboxFlags::AutomaticFeatures &
      ~blink::WebSandboxFlags::Origin;
  EXPECT_EQ(expected_flags,
            root->child_at(1)->current_replication_state().sandbox_flags);

  // The child of the sandboxed frame should've inherited sandbox flags, so it
  // should not be able to create popups.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(bottom_child->current_frame_host(),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that a child frame can retrieve the name property set by its parent.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, WindowNameReplication) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // Ensure that a new process is created for the subframe.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Check that the window.name seen by the frame matches the name attribute
  // specified by its parent in the iframe tag.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(0)->current_frame_host(),
      "window.domAutomationController.send(window.name);", &result));
  EXPECT_EQ("3-1-name", result);
}

// Verify that dynamic updates to a frame's window.name propagate to the
// frame's proxies, so that the latest frame names can be used in navigations.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, DynamicWindowName) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // Browser process should know the child frame's original window.name
  // specified in the iframe element.
  EXPECT_EQ(root->child_at(0)->frame_name(), "3-1-name");

  // Update the child frame's window.name.
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "window.domAutomationController.send("
                            "window.name = 'updated-name');"));

  // The change should propagate to the browser process.
  EXPECT_EQ(root->child_at(0)->frame_name(), "updated-name");

  // The proxy in the parent process should also receive the updated name.
  // Check that it can reference the child frame by its new name.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(shell()->web_contents(),
                                  "window.domAutomationController.send("
                                  "frames['updated-name'] == frames[0]);",
                                  &success));
  EXPECT_TRUE(success);

  // Issue a renderer-initiated navigation from the root frame to the child
  // frame using the frame's name. Make sure correct frame is navigated.
  //
  // TODO(alexmos): When blink::createWindow is refactored to handle
  // RemoteFrames, this should also be tested via window.open(url, frame_name)
  // and a more complicated frame hierarchy (https://crbug.com/463742)
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  std::string script = base::StringPrintf(
      "window.domAutomationController.send("
      "frames['updated-name'].location.href = '%s');",
      foo_url.spec().c_str());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), script));
  frame_observer.Wait();
  EXPECT_EQ(foo_url, root->child_at(0)->current_url());
}

// Verify that when a frame is navigated to a new origin, the origin update
// propagates to the frame's proxies.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, OriginUpdatesReachProxies) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Navigate second subframe to a baz.com.  This should send an origin update
  // to the frame's proxy in the bar.com (first frame's) process.
  GURL frame_url = embedded_test_server()->GetURL("baz.com", "/title2.html");
  NavigateFrameToURL(root->child_at(1), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // The first frame can't directly observe the second frame's origin with
  // JavaScript.  Instead, try to navigate the second frame from the first
  // frame.  This should fail with a console error message, which should
  // contain the second frame's updated origin (see blink::Frame::canNavigate).
  scoped_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Unsafe JavaScript attempt to initiate navigation*"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  // frames[1] can't be used due to a bug where RemoteFrames are created out of
  // order (https://crbug.com/478792).  Instead, target second frame by name.
  EXPECT_TRUE(ExecuteScript(
      root->child_at(0)->current_frame_host(),
      "window.domAutomationController.send("
      "    parent.frames['frame2'].location.href = 'data:text/html,foo');"));
  console_delegate->Wait();

  std::string frame_origin =
      root->child_at(1)->current_replication_state().origin.string();
  EXPECT_EQ(frame_origin + "/", frame_url.GetOrigin().spec());
  EXPECT_TRUE(
      base::MatchPattern(console_delegate->message(), "*" + frame_origin + "*"))
      << "Error message does not contain the frame's latest origin ("
      << frame_origin << ")";
}

// Ensure that navigating subframes in --site-per-process mode properly fires
// the DidStopLoading event on WebContentsObserver.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteDidStopLoading) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  TestNavigationObserver nav_observer(shell()->web_contents(), 1);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator()->GetController()->LoadURLWithParams(params);
  nav_observer.Wait();

  // Verify that the navigation succeeded and the expected URL was loaded.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
}

// Ensure that the renderer does not crash when navigating a frame that has a
// sibling RemoteFrame.  See https://crbug.com/426953.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateWithSiblingRemoteFrame) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  TestNavigationObserver observer(shell()->web_contents());

  // Make sure the first frame is out of process.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            node2->current_frame_host()->GetSiteInstance());

  // Make sure the second frame is in the parent's process.
  FrameTreeNode* node3 = root->child_at(1);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Navigate the second iframe (node3) to a URL in its own process.
  GURL title_url = embedded_test_server()->GetURL("/title2.html");
  NavigateFrameToURL(node3, title_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(title_url, observer.last_navigation_url());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(node3->current_frame_host()->IsRenderFrameLive());
}

// Verify that load events for iframe elements work when the child frame is
// out-of-process.  In such cases, the load event is forwarded from the child
// frame to the parent frame via the browser process.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, LoadEventForwarding) {
  // Load a page with a cross-site frame.  The parent page has an onload
  // handler in the iframe element that appends "LOADED" to the document title.
  {
    GURL main_url(
        embedded_test_server()->GetURL("/frame_with_load_event.html"));
    base::string16 expected_title(base::UTF8ToUTF16("LOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  }

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load another cross-site page into the iframe and check that the load event
  // is fired.
  {
    GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
    base::string16 expected_title(base::UTF8ToUTF16("LOADEDLOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    TestNavigationObserver observer(shell()->web_contents());
    NavigateFrameToURL(root->child_at(0), foo_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(foo_url, observer.last_navigation_url());
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  }
}

// Check that postMessage can be routed between cross-site iframes.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, SubframePostMessage) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  ASSERT_EQ(2U, root->child_count());

  // Verify the frames start at correct URLs.  First frame should be
  // same-site; second frame should be cross-site.
  GURL same_site_url(embedded_test_server()->GetURL("/post_message.html"));
  EXPECT_EQ(same_site_url, root->child_at(0)->current_url());
  GURL foo_url(embedded_test_server()->GetURL("foo.com",
                                              "/post_message.html"));
  EXPECT_EQ(foo_url, root->child_at(1)->current_url());
  EXPECT_NE(root->child_at(0)->current_frame_host()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  // Send a message from first, same-site frame to second, cross-site frame.
  // Expect the second frame to reply back to the first frame.
  PostMessageAndWaitForReply(root->child_at(0),
                             "postToSibling('subframe-msg','subframe2')",
                             "\"done-subframe1\"");

  // Send a postMessage from second, cross-site frame to its parent.  Expect
  // parent to send a reply to the frame.
  base::string16 expected_title(base::ASCIIToUTF16("subframe-msg"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  PostMessageAndWaitForReply(root->child_at(1), "postToParent('subframe-msg')",
                             "\"done-subframe2\"");
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Verify the total number of received messages for each subframe.  First
  // frame should have one message (reply from second frame).  Second frame
  // should have two messages (message from first frame and reply from parent).
  // Parent should have one message (from second frame).
  EXPECT_EQ(1, GetReceivedMessages(root->child_at(0)));
  EXPECT_EQ(2, GetReceivedMessages(root->child_at(1)));
  EXPECT_EQ(1, GetReceivedMessages(root));
}

// Check that postMessage can be sent from a subframe on a cross-process opener
// tab, and that its event.source points to a valid proxy.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       PostMessageWithSubframeOnOpenerChain) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  ASSERT_EQ(2U, root->child_count());

  // Verify the initial state of the world.  First frame should be same-site;
  // second frame should be cross-site.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Open a popup from the first subframe (so that popup's window.opener points
  // to the subframe) and navigate it to bar.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "openPopup('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(
      embedded_test_server()->GetURL("bar.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURL(popup, popup_url));

  // From the popup, open another popup for baz.com.  This will be used to
  // check that the whole opener chain is processed when creating proxies and
  // not just an immediate opener.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(
      ExecuteScript(popup->web_contents(), "openPopup('about:blank');"));
  Shell* popup2 = new_shell_observer2.GetShell();
  GURL popup2_url(
      embedded_test_server()->GetURL("baz.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURL(popup2, popup2_url));

  // Ensure that we've created proxies for SiteInstances of both popups (C, D)
  // in the main window's frame tree.
  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site A ------- proxies for B C D\n"
      "   +--Site B ------- proxies for A C D\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/\n"
      "      C = http://bar.com/\n"
      "      D = http://baz.com/",
      DepictFrameTree(root));

  // Check the first popup's frame tree as well.  Note that it doesn't have a
  // proxy for foo.com, since foo.com can't reach the popup.  It does have a
  // proxy for its opener a.com (which can reach it via the window.open
  // reference) and second popup (which can reach it via window.opener).
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(
      " Site C ------------ proxies for A D\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/\n"
      "      D = http://baz.com/",
      DepictFrameTree(popup_root));

  // Send a message from first subframe on main page to the first popup and
  // wait for a reply back. The reply verifies that the proxy for the opener
  // tab's subframe is targeted properly.
  PostMessageAndWaitForReply(root->child_at(0), "postToPopup('subframe-msg')",
                             "\"done-subframe1\"");

  // Send a postMessage from the popup to window.opener and ensure that it
  // reaches subframe1.  This verifies that the subframe opener information
  // propagated to the popup's RenderFrame.  Wait for subframe1 to send a reply
  // message to the popup.
  EXPECT_TRUE(ExecuteScript(popup->web_contents(), "window.name = 'popup';"));
  PostMessageAndWaitForReply(popup_root, "postToOpener('subframe-msg', '*')",
                             "\"done-popup\"");

  // Second a postMessage from popup2 to window.opener.opener, which should
  // resolve to subframe1.  This tests opener chains of length greater than 1.
  // As before, subframe1 will send a reply to popup2.
  FrameTreeNode* popup2_root =
      static_cast<WebContentsImpl*>(popup2->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_TRUE(ExecuteScript(popup2->web_contents(), "window.name = 'popup2';"));
  PostMessageAndWaitForReply(popup2_root,
                             "postToOpenerOfOpener('subframe-msg', '*')",
                             "\"done-popup2\"");

  // Verify the total number of received messages for each subframe:
  //  - 3 for first subframe (two from first popup, one from second popup)
  //  - 2 for popup (both from first subframe)
  //  - 1 for popup2 (reply from first subframe)
  //  - 0 for other frames
  EXPECT_EQ(0, GetReceivedMessages(root));
  EXPECT_EQ(3, GetReceivedMessages(root->child_at(0)));
  EXPECT_EQ(0, GetReceivedMessages(root->child_at(1)));
  EXPECT_EQ(2, GetReceivedMessages(popup_root));
  EXPECT_EQ(1, GetReceivedMessages(popup2_root));
}

// Check that parent.frames[num] references correct sibling frames when the
// parent is remote.  See https://crbug.com/478792.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, IndexedFrameAccess) {
  // Start on a page with three same-site subframes.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(3U, root->child_count());
  FrameTreeNode* child0 = root->child_at(0);
  FrameTreeNode* child1 = root->child_at(1);
  FrameTreeNode* child2 = root->child_at(2);

  // Send each of the frames to a different site.  Each new renderer will first
  // create proxies for the parent and two sibling subframes and then create
  // and insert the new RenderFrame into the frame tree.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/post_message.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/post_message.html"));
  GURL d_url(embedded_test_server()->GetURL("d.com", "/post_message.html"));
  NavigateFrameToURL(child0, b_url);
  // TODO(alexmos): The calls to WaitForRenderFrameReady can be removed once
  // TestFrameNavigationObserver is fixed to use DidFinishLoad.
  EXPECT_TRUE(WaitForRenderFrameReady(child0->current_frame_host()));
  NavigateFrameToURL(child1, c_url);
  EXPECT_TRUE(WaitForRenderFrameReady(child1->current_frame_host()));
  NavigateFrameToURL(child2, d_url);
  EXPECT_TRUE(WaitForRenderFrameReady(child2->current_frame_host()));

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   |--Site C ------- proxies for A B D\n"
      "   +--Site D ------- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  // Check that each subframe sees itself at correct index in parent.frames.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child0->current_frame_host(),
      "window.domAutomationController.send(window === parent.frames[0]);",
      &success));
  EXPECT_TRUE(success);

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child1->current_frame_host(),
      "window.domAutomationController.send(window === parent.frames[1]);",
      &success));
  EXPECT_TRUE(success);

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child2->current_frame_host(),
      "window.domAutomationController.send(window === parent.frames[2]);",
      &success));
  EXPECT_TRUE(success);

  // Send a postMessage from B to parent.frames[1], which should go to C, and
  // wait for reply.
  PostMessageAndWaitForReply(child0, "postToSibling('subframe-msg', 1)",
                             "\"done-1-1-name\"");

  // Send a postMessage from C to parent.frames[2], which should go to D, and
  // wait for reply.
  PostMessageAndWaitForReply(child1, "postToSibling('subframe-msg', 2)",
                             "\"done-1-2-name\"");

  // Verify the total number of received messages for each subframe.
  EXPECT_EQ(1, GetReceivedMessages(child0));
  EXPECT_EQ(2, GetReceivedMessages(child1));
  EXPECT_EQ(1, GetReceivedMessages(child2));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, RFPHDestruction) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Load another cross-site page.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  NavigateIframeToURL(shell()->web_contents(), "test", url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://127.0.0.1/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Navigate back to the parent's origin.
  url = embedded_test_server()->GetURL("/title1.html");
  NavigateFrameToURL(child, url);
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        |--Site A\n"
      "        +--Site A\n"
      "             +--Site A\n"
      "Where A = http://127.0.0.1/",
      DepictFrameTree(root));
}

}  // namespace content
