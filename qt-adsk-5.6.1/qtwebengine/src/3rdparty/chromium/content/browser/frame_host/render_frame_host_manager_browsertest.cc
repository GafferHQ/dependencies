// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/page_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/net_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using base::ASCIIToUTF16;

namespace content {

namespace {

const char kOpenUrlViaClickTargetFunc[] =
    "(function(url) {\n"
    "  var lnk = document.createElement(\"a\");\n"
    "  lnk.href = url;\n"
    "  lnk.target = \"_blank\";\n"
    "  document.body.appendChild(lnk);\n"
    "  lnk.click();\n"
    "})";

// Adds a link with given url and target=_blank, and clicks on it.
void OpenUrlViaClickTarget(const internal::ToRenderFrameHost& adapter,
                           const GURL& url) {
  EXPECT_TRUE(ExecuteScript(adapter,
      std::string(kOpenUrlViaClickTargetFunc) + "(\"" + url.spec() + "\");"));
}

Shell* OpenPopup(const internal::ToRenderFrameHost& opener,
                 const std::string& name) {
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener,
      "window.domAutomationController.send(!!window.open('', '" + name + "'));",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  return new_shell;
}

}  // anonymous namespace

class RenderFrameHostManagerTest : public ContentBrowserTest {
 public:
  RenderFrameHostManagerTest() : foo_com_("foo.com") {
    replace_host_.SetHostStr(foo_com_);
  }

  static bool GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    std::vector<net::SpawnedTestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::SpawnedTestServer::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
  }

  void StartServer() {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server()->Start());

    foo_host_port_ = test_server()->host_port_pair();
    foo_host_port_.set_host(foo_com_);
  }

  void StartEmbeddedServer() {
    // Support multiple sites on the embedded test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    SetupCrossSiteRedirector(embedded_test_server());
  }

  // Returns a URL on foo.com with the given path.
  GURL GetCrossSiteURL(const std::string& path) {
    GURL cross_site_url(test_server()->GetURL(path));
    return cross_site_url.ReplaceComponents(replace_host_);
  }

 protected:
  std::string foo_com_;
  GURL::Replacements replace_host_;
  net::HostPortPair foo_host_port_;
};

// Web pages should not have script access to the swapped out page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, NoScriptAccessAfterSwapOut) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Open a same-site link in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site.
  NavigateToURL(new_shell, GetCrossSiteURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // We should no longer have script access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_FALSE(success);

  // We now navigate the window to an about:blank page.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickBlankTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish.
  WaitForLoadStop(new_shell->web_contents());
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_EQ(blank_url,
            new_shell->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and target=_blank should create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithRelNoreferrerAndTargetBlank) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetVisibleURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// As of crbug.com/69267, we create a new BrowsingInstance (and SiteInstance)
// for rel=noreferrer links in new windows, even to same site pages and named
// targets.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithSameSiteRelNoreferrer) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a same-site rel=noreferrer + target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteNoRefTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Opens in new window.
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetVisibleURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with just
// target=_blank should not create a new SiteInstance, unless we
// are running in --site-per-process mode.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyTargetBlank) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  EXPECT_TRUE(NavigateToURL(shell(), test_server()->GetURL(replacement_path)));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the cross-site transition in the new tab to finish.
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    EXPECT_EQ(orig_site_instance, blank_site_instance);
  else
    EXPECT_NE(orig_site_instance, blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and no target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyRelNoreferrer) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same window.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/files/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess))
    EXPECT_EQ(orig_site_instance, noref_site_instance);
  else
    EXPECT_NE(orig_site_instance, noref_site_instance);
}

// Test for crbug.com/116192.  Targeted links should still work after the
// named target window has swapped processes.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       AllowTargetedNavigationsAfterSwap) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  GURL cross_site_url(GetCrossSiteURL("files/title1.html"));
  NavigateToURL(new_shell, cross_site_url);
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Clicking the original link in the first tab should cause us to swap back.
  TestNavigationObserver navigation_observer(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back and shown the new window again.
  scoped_refptr<SiteInstance> revisit_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);

  // If it navigates away to another process, the original window should
  // still be able to close it (using a cross-process close message).
  NavigateToURL(new_shell, cross_site_url);
  EXPECT_EQ(new_site_instance.get(),
            new_shell->web_contents()->GetSiteInstance());
  WebContentsDestroyedWatcher close_watcher(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(testCloseWindow());",
      &success));
  EXPECT_TRUE(success);
  close_watcher.Wait();
}

// Test that setting the opener to null in a window affects cross-process
// navigations, including those to existing entries.  http://crbug.com/156669.
// This test crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_DisownOpener DISABLED_DisownOpener
#else
#define MAYBE_DisownOpener DisownOpener
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, MAYBE_DisownOpener) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=_blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  NavigateToURL(new_shell, GetCrossSiteURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Now disown the opener.
  EXPECT_TRUE(ExecuteScript(new_shell->web_contents(),
                            "window.opener = null;"));
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());

  // Go back and ensure the opener is still null.
  {
    TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell->web_contents(),
      "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());

  // Now navigate forward again (creating a new process) and check opener.
  NavigateToURL(new_shell, GetCrossSiteURL("files/title1.html"));
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell->web_contents(),
      "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());
}

// Test that subframes can disown their openers.  http://crbug.com/225528.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, DisownSubframeOpener) {
  const GURL frame_url("data:text/html,<iframe name=\"foo\"></iframe>");
  NavigateToURL(shell(), frame_url);

  // Give the frame an opener using window.open.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.open('about:blank','foo');"));

  // Now disown the frame's opener.  Shouldn't crash.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "window.frames[0].opener = null;"));
}

// Check that window.name is preserved for top frames when they navigate
// cross-process.  See https://crbug.com/504164.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PreserveTopFrameWindowNameOnCrossProcessNavigations) {
  StartEmbeddedServer();

  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Open a popup using window.open with a 'foo' window.name.
  Shell* new_shell = OpenPopup(shell()->web_contents(), "foo");

  // The window.name for the new popup should be "foo".
  std::string name;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_shell->web_contents(),
      "window.domAutomationController.send(window.name);", &name));
  EXPECT_EQ("foo", name);

  // Now navigate the new tab to a different site.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // window.name should still be "foo".
  name = "";
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_shell->web_contents(),
      "window.domAutomationController.send(window.name);", &name));
  EXPECT_EQ("foo", name);

  // Open another popup from the 'foo' popup and navigate it cross-site.
  Shell* new_shell2 = OpenPopup(new_shell->web_contents(), "bar");
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURL(new_shell2, bar_url));

  // Check that the new popup's window.opener has name "foo", which verifies
  // that new swapped-out RenderViews also propagate window.name.  This has to
  // be done via window.open, since window.name isn't readable cross-origin.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell2->web_contents(),
      "window.domAutomationController.send("
      "    window.opener === window.open('','foo'));",
      &success));
  EXPECT_TRUE(success);
}

// Test for crbug.com/99202.  PostMessage calls should still work after
// navigating the source and target windows to different sites.
// Specifically:
// 1) Create 3 windows (opener, "foo", and _blank) and send "foo" cross-process.
// 2) Fail to post a message from "foo" to opener with the wrong target origin.
// 3) Post a message from "foo" to opener, which replies back to "foo".
// 4) Post a message from _blank to "foo".
// 5) Post a message from "foo" to a subframe of opener, which replies back.
// 6) Post a message from _blank to a subframe of "foo".
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SupportCrossProcessPostMessage) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);
  RenderFrameHostManager* opener_manager = static_cast<WebContentsImpl*>(
      opener_contents)->GetRenderManagerForTesting();

  // 1) Open two more windows, one named.  These initially have openers but no
  // reference to each other.  We will later post a message between them.

  // First, a named target=foo window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/files/navigate_opener.html",
            foo_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell, GetCrossSiteURL("files/post_message.html"));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // Second, a target=_blank window.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on the original site.
  Shell* new_shell2 = new_shell_observer2.GetShell();
  WebContents* new_contents = new_shell2->web_contents();
  WaitForLoadStop(new_contents);
  EXPECT_EQ("/files/title2.html", new_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell2, test_server()->GetURL("files/post_message.html"));
  EXPECT_EQ(orig_site_instance.get(), new_contents->GetSiteInstance());
  RenderFrameHostManager* new_manager =
      static_cast<WebContentsImpl*>(new_contents)->GetRenderManagerForTesting();

  // We now have three windows.  The opener should have a swapped out RVH
  // for the new SiteInstance, but the _blank window should not.
  EXPECT_EQ(3u, Shell::windows().size());
  EXPECT_TRUE(
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));
  EXPECT_FALSE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // 2) Fail to post a message from the foo window to the opener if the target
  // origin is wrong.  We won't see an error, but we can check for the right
  // number of received messages below.
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg',"
      "    'http://google.com'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));

  // 3) Post a message from the foo window to the opener.  The opener will
  // reply, causing the foo window to update its own title.
  base::string16 expected_title = ASCIIToUTF16("msg");
  TitleWatcher title_watcher(foo_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg','*'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // We should have received only 1 message in the opener and "foo" tabs,
  // and updated the title.
  int opener_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      opener_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &opener_received_messages));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, opener_received_messages);
  EXPECT_EQ(ASCIIToUTF16("msg"), foo_contents->GetTitle());

  // 4) Now post a message from the _blank window to the foo window.  The
  // foo window will update its title and will not reply.
  expected_title = ASCIIToUTF16("msg2");
  TitleWatcher title_watcher2(foo_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_contents,
      "window.domAutomationController.send(postToFoo('msg2'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_EQ(expected_title, title_watcher2.WaitAndGetTitle());

  // This postMessage should have created a swapped out RVH for the new
  // SiteInstance in the target=_blank window.
  EXPECT_TRUE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // TODO(nasko): Test subframe targeting of postMessage once
  // http://crbug.com/153701 is fixed.
}

// Test for crbug.com/278336. MessagePorts should work cross-process. I.e.,
// messages which contain Transferables and get intercepted by
// RenderViewImpl::willCheckAndDispatchMessageEvent (because the RenderView is
// swapped out) should work.
// Specifically:
// 1) Create 2 windows (opener and "foo") and send "foo" cross-process.
// 2) Post a message containing a message port from opener to "foo".
// 3) Post a message from "foo" back to opener via the passed message port.
// The test will be enabled when the feature implementation lands.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SupportCrossProcessPostMessageWithMessagePort) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);
  RenderFrameHostManager* opener_manager = static_cast<WebContentsImpl*>(
      opener_contents)->GetRenderManagerForTesting();

  // 1) Open a named target=foo window. We will later post a message between the
  // opener and the new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/files/navigate_opener.html",
            foo_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell, GetCrossSiteURL("files/post_message.html"));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // We now have two windows. The opener should have a swapped out RVH
  // for the new SiteInstance.
  EXPECT_EQ(2u, Shell::windows().size());
  EXPECT_TRUE(
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // 2) Post a message containing a MessagePort from opener to the the foo
  // window. The foo window will reply via the passed port, causing the opener
  // to update its own title.
  base::string16 expected_title = ASCIIToUTF16("msg-back-via-port");
  TitleWatcher title_observer(opener_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(postWithPortToFoo());",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));
  ASSERT_EQ(expected_title, title_observer.WaitAndGetTitle());

  // Check message counts.
  int opener_received_messages_via_port = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      opener_contents,
      "window.domAutomationController.send(window.receivedMessagesViaPort);",
      &opener_received_messages_via_port));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  int foo_received_messages_with_port = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessagesWithPort);",
      &foo_received_messages_with_port));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, foo_received_messages_with_port);
  EXPECT_EQ(1, opener_received_messages_via_port);
  EXPECT_EQ(ASCIIToUTF16("msg-with-port"), foo_contents->GetTitle());
  EXPECT_EQ(ASCIIToUTF16("msg-back-via-port"), opener_contents->GetTitle());
}

// Test for crbug.com/116192.  Navigations to a window's opener should
// still work after a process swap.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       AllowTargetedNavigationsInOpenerAfterSwap) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original tab and SiteInstance for later comparison.
  WebContents* orig_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      orig_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the original (opener) tab to a different site.
  NavigateToURL(shell(), GetCrossSiteURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The opened tab should be able to navigate the opener back to its process.
  TestNavigationObserver navigation_observer(orig_contents);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell->web_contents(),
      "window.domAutomationController.send(navigateOpener());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back into this process.
  scoped_refptr<SiteInstance> revisit_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);
}

// Test that subframes do not crash when sending a postMessage to the top frame
// from an unload handler while the top frame is being swapped out as part of
// navigating cross-process.  https://crbug.com/475651.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PostMessageFromSubframeUnloadHandler) {
  StartServer();

  GURL frame_url(test_server()->GetURL("files/post_message.html"));
  GURL main_url("data:text/html,<iframe name='foo' src='" + frame_url.spec() +
                "'></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(nullptr, orig_site_instance.get());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());

  // Register an unload handler that sends a postMessage to the top frame.
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "registerUnload();"));

  // Navigate the top frame cross-site.  This will cause the top frame to be
  // swapped out and run unload handlers, and the original renderer process
  // should then terminate since it's not rendering any other frames.
  RenderProcessHostWatcher exit_observer(
      root->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(shell(), GetCrossSiteURL("files/title1.html")));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Ensure that the original renderer process exited cleanly without crashing.
  exit_observer.Wait();
  EXPECT_EQ(true, exit_observer.did_exit_normally());
}

// Test that opening a new window in the same SiteInstance and then navigating
// both windows to a different SiteInstance allows the first process to exit.
// See http://crbug.com/126333.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ProcessExitWithSwappedOutViews) {
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> opened_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, opened_site_instance);

  // Now navigate the opened window to a different site.
  NavigateToURL(new_shell, GetCrossSiteURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The original process should still be alive, since it is still used in the
  // first window.
  RenderProcessHost* orig_process = orig_site_instance->GetProcess();
  EXPECT_TRUE(orig_process->HasConnection());

  // Navigate the first window to a different site as well.  The original
  // process should exit, since all of its views are now swapped out.
  RenderProcessHostWatcher exit_observer(
      orig_process,
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  NavigateToURL(shell(), GetCrossSiteURL("files/title1.html"));
  exit_observer.Wait();
  scoped_refptr<SiteInstance> new_site_instance2(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(new_site_instance, new_site_instance2);
}

// Test for crbug.com/76666.  A cross-site navigation that fails with a 204
// error should not make us ignore future renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, ClickLinkAfter204Error) {
  StartServer();

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Load a cross-site page that fails with a 204 error.
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell(),
                                             GetCrossSiteURL("nocontent")));

  // We should still be looking at the normal page.  Because we started from a
  // blank new tab, the typed URL will still be visible until the user clears it
  // manually.  The last committed URL will be the previous page.
  scoped_refptr<SiteInstance> post_nav_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, post_nav_site_instance);
  EXPECT_EQ("/nocontent",
            shell()->web_contents()->GetVisibleURL().path());
  EXPECT_FALSE(
      shell()->web_contents()->GetController().GetLastCommittedEntry());

  // Renderer-initiated navigations should work.
  base::string16 expected_title = ASCIIToUTF16("Title Of Awesomeness");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  GURL url = test_server()->GetURL("files/title2.html");
  EXPECT_TRUE(ExecuteScript(
      shell()->web_contents(),
      base::StringPrintf("location.href = '%s'", url.spec().c_str())));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Opens in same tab.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/files/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, new_site_instance);
}

// Test for crbug.com/9682.  We should show the URL for a pending renderer-
// initiated navigation in a new tab, until the content of the initial
// about:blank page is modified by another window.  At that point, we should
// revert to showing about:blank to prevent a URL spoof.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, ShowLoadingURLUntilSpoof) {
  ASSERT_TRUE(test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(
      shell(), test_server()->GetURL("files/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(clickNoContentTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  WebContents* contents = new_shell->web_contents();
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());

  // Now modify the contents of the new window from the opener.  This will also
  // modify the title of the document to give us something to listen for.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(contents, expected_title);
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(modifyNewWindow());",
      &success));
  EXPECT_TRUE(success);
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // At this point, we should no longer be showing the destination URL.
  // The visible entry should be null, resulting in about:blank in the address
  // bar.
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Test for crbug.com/9682.  We should not show the URL for a pending renderer-
// initiated navigation in a new tab if it is not the initial navigation.  In
// this case, the renderer will not notify us of a modification, so we cannot
// show the pending URL without allowing a spoof.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontShowLoadingURLIfNotInitialNav) {
  ASSERT_TRUE(test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(
      shell(), test_server()->GetURL("files/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  // By using an onclick handler that first creates the window, the slow
  // navigation is not considered an initial navigation.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send("
      "clickNoContentScriptedTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Ensure the destination URL is not visible, because it is not the initial
  // navigation.
  WebContents* contents = new_shell->web_contents();
  EXPECT_FALSE(contents->GetController().IsInitialNavigation());
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_BackForwardNotStale DISABLED_BackForwardNotStale
#else
#define MAYBE_BackForwardNotStale BackForwardNotStale
#endif
// Test for http://crbug.com/93427.  Ensure that cross-site navigations
// do not cause back/forward navigations to be considered stale by the
// renderer.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, MAYBE_BackForwardNotStale) {
  StartServer();
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Visit a page on first site.
  NavigateToURL(shell(), test_server()->GetURL("files/title1.html"));

  // Visit three pages on second site.
  NavigateToURL(shell(), GetCrossSiteURL("files/title1.html"));
  NavigateToURL(shell(), GetCrossSiteURL("files/title2.html"));
  NavigateToURL(shell(), GetCrossSiteURL("files/title3.html"));

  // History is now [blank, A1, B1, B2, *B3].
  WebContents* contents = shell()->web_contents();
  EXPECT_EQ(5, contents->GetController().GetEntryCount());

  // Open another window in same process to keep this process alive.
  Shell* new_shell = CreateBrowser();
  NavigateToURL(new_shell, GetCrossSiteURL("files/title1.html"));

  // Go back three times to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward twice to B2.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }

  // Go back twice to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward directly to B3.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoToIndex(4);
    forward_nav_load_observer.Wait();
  }
}

// Test for http://crbug.com/130016.
// Swapping out a render view should update its visiblity state.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwappedOutViewHasCorrectVisibilityState) {
  // This test is invalid in --site-per-process mode, as swapped-out is no
  // longer used.
  if (RenderFrameHostManager::IsSwappedOutStateForbidden())
    return;
  StartServer();

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      foo_host_port_,
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Open a same-site link in a new widnow.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  RenderViewHost* rvh = new_shell->web_contents()->GetRenderViewHost();

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      rvh,
      "window.domAutomationController.send("
      "    document.visibilityState == 'visible');",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site. This should swap out the
  // tab's existing RenderView, causing it become hidden.
  NavigateToURL(new_shell, GetCrossSiteURL("files/title1.html"));

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      rvh,
      "window.domAutomationController.send("
      "    document.visibilityState == 'hidden');",
      &success));
  EXPECT_TRUE(success);

  // Going back should make the previously swapped-out view to become visible
  // again.
  {
    TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  EXPECT_EQ(rvh, new_shell->web_contents()->GetRenderViewHost());

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      rvh,
      "window.domAutomationController.send("
      "    document.visibilityState == 'visible');",
      &success));
  EXPECT_TRUE(success);
}

// This class ensures that all the given RenderViewHosts have properly been
// shutdown.
class RenderViewHostDestructionObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostDestructionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderViewHostDestructionObserver() override {}
  void EnsureRVHGetsDestructed(RenderViewHost* rvh) {
    watched_render_view_hosts_.insert(rvh);
  }
  size_t GetNumberOfWatchedRenderViewHosts() const {
    return watched_render_view_hosts_.size();
  }

 private:
  // WebContentsObserver implementation:
  void RenderViewDeleted(RenderViewHost* rvh) override {
    watched_render_view_hosts_.erase(rvh);
  }

  std::set<RenderViewHost*> watched_render_view_hosts_;
};

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_LeakingRenderViewHosts DISABLED_LeakingRenderViewHosts
#else
#define MAYBE_LeakingRenderViewHosts LeakingRenderViewHosts
#endif
// Test for crbug.com/90867. Make sure we don't leak render view hosts since
// they may cause crashes or memory corruptions when trying to call dead
// delegate_. This test also verifies crbug.com/117420 and crbug.com/143255 to
// ensure that a separate SiteInstance is created when navigating to view-source
// URLs, regardless of current URL.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_LeakingRenderViewHosts) {
  StartServer();

  // Observe the created render_view_host's to make sure they will not leak.
  RenderViewHostDestructionObserver rvh_observers(shell()->web_contents());

  GURL navigated_url(test_server()->GetURL("files/title2.html"));
  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       navigated_url.spec());

  // Let's ensure that when we start with a blank window, navigating away to a
  // view-source URL, we create a new SiteInstance.
  RenderViewHost* blank_rvh = shell()->web_contents()->GetRenderViewHost();
  SiteInstance* blank_site_instance = blank_rvh->GetSiteInstance();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), GURL::EmptyGURL());
  EXPECT_EQ(blank_site_instance->GetSiteURL(), GURL::EmptyGURL());
  rvh_observers.EnsureRVHGetsDestructed(blank_rvh);

  // Now navigate to the view-source URL and ensure we got a different
  // SiteInstance and RenderViewHost.
  NavigateToURL(shell(), view_source_url);
  EXPECT_NE(blank_rvh, shell()->web_contents()->GetRenderViewHost());
  EXPECT_NE(blank_site_instance, shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance());
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  // Load a random page and then navigate to view-source: of it.
  // This used to cause two RVH instances for the same SiteInstance, which
  // was a problem.  This is no longer the case.
  NavigateToURL(shell(), navigated_url);
  SiteInstance* site_instance1 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  NavigateToURL(shell(), view_source_url);
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());
  SiteInstance* site_instance2 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();

  // Ensure that view-source navigations force a new SiteInstance.
  EXPECT_NE(site_instance1, site_instance2);

  // Now navigate to a different instance so that we swap out again.
  NavigateToURL(shell(), GetCrossSiteURL("files/title2.html"));
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  // This used to leak a render view host.
  shell()->Close();

  RunAllPendingInMessageLoop();  // Needed on ChromeOS.

  EXPECT_EQ(0U, rvh_observers.GetNumberOfWatchedRenderViewHosts());
}

// Test for crbug.com/143155.  Frame tree updates during unload should not
// interrupt the intended navigation and show swappedout:// instead.
// Specifically:
// 1) Open 2 tabs in an HTTP SiteInstance, with a subframe in the opener.
// 2) Send the second tab to a different foo.com SiteInstance.
//    This creates a swapped out opener for the first tab in the foo process.
// 3) Navigate the first tab to the foo.com SiteInstance, and have the first
//    tab's unload handler remove its frame.
// This used to cause an update to the frame tree of the swapped out RV,
// just as it was navigating to a real page.  That pre-empted the real
// navigation and visibly sent the tab to swappedout://.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontPreemptNavigationWithFrameTreeUpdate) {
  StartServer();

  // 1. Load a page that deletes its iframe during unload.
  NavigateToURL(shell(),
                test_server()->GetURL("files/remove_frame_on_unload.html"));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Open a same-site page in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(openWindow());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title1.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance.get(),
            new_shell->web_contents()->GetSiteInstance());

  // 2. Send the second tab to a different process.
  NavigateToURL(new_shell, GetCrossSiteURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // 3. Send the first tab to the second tab's process.
  NavigateToURL(shell(), GetCrossSiteURL("files/title1.html"));

  // Make sure it ends up at the right page.
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(GetCrossSiteURL("files/title1.html"),
            shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(new_site_instance.get(),
            shell()->web_contents()->GetSiteInstance());
}

// Ensure that renderer-side debug URLs do not cause a process swap, since they
// are meant to run in the current page.  We had a bug where we expected a
// BrowsingInstance swap to occur on pages like view-source and extensions,
// which broke chrome://crash and javascript: URLs.
// See http://crbug.com/335503.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, RendererDebugURLsDontSwap) {
  ASSERT_TRUE(test_server()->Start());

  GURL original_url(test_server()->GetURL("files/title2.html"));
  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       original_url.spec());

  NavigateToURL(shell(), view_source_url);

  // Check that javascript: URLs work.
  base::string16 expected_title = ASCIIToUTF16("msg");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  shell()->LoadURL(GURL("javascript:document.title='msg'"));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Crash the renderer of the view-source page.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GURL(kChromeUICrashURL)));
  crash_observer.Wait();
}

// Ensure that renderer-side debug URLs don't take effect on crashed renderers.
// Otherwise, we might try to load an unprivileged about:blank page into a
// WebUI-enabled RenderProcessHost, failing a safety check in InitRenderView.
// See http://crbug.com/334214.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       IgnoreRendererDebugURLsWhenCrashed) {
  // Visit a WebUI page with bindings.
  GURL webui_url = GURL(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), webui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));

  // Crash the renderer of the WebUI page.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GURL(kChromeUICrashURL)));
  crash_observer.Wait();

  // Load the crash URL again but don't wait for any action.  If it is not
  // ignored this time, we will fail the WebUI CHECK in InitRenderView.
  shell()->LoadURL(GURL(kChromeUICrashURL));

  // Ensure that such URLs can still work as the initial navigation of a tab.
  // We postpone the initial navigation of the tab using an empty GURL, so that
  // we can add a watcher for crashes.
  Shell* shell2 = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(), NULL, gfx::Size());
  RenderProcessHostWatcher crash_observer2(
      shell2->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell2, GURL(kChromeUIKillURL)));
  crash_observer2.Wait();
}

// The test fails with Android ASAN with changes in v8 that seem unrelated.
//   See http://crbug.com/428329.
#if defined(OS_ANDROID) && defined(THREAD_SANITIZER)
#define MAYBE_ClearPendingWebUIOnCommit DISABLED_ClearPendingWebUIOnCommit
#else
#define MAYBE_ClearPendingWebUIOnCommit ClearPendingWebUIOnCommit
#endif
// Ensure that pending_and_current_web_ui_ is cleared when a URL commits.
// Otherwise it might get picked up by InitRenderView when granting bindings
// to other RenderViewHosts.  See http://crbug.com/330811.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_ClearPendingWebUIOnCommit) {
  // Visit a WebUI page with bindings.
  GURL webui_url(GURL(std::string(kChromeUIScheme) + "://" +
                      std::string(kChromeUIGpuHost)));
  NavigateToURL(shell(), webui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  WebUIImpl* webui = web_contents->GetRenderManagerForTesting()->web_ui();
  EXPECT_TRUE(webui);
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->pending_web_ui());

  // Navigate to another WebUI URL that reuses the WebUI object.  Make sure we
  // clear pending_web_ui() when it commits.
  GURL webui_url2(webui_url.spec() + "#foo");
  NavigateToURL(shell(), webui_url2);
  EXPECT_EQ(webui, web_contents->GetRenderManagerForTesting()->web_ui());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->pending_web_ui());
}

class RFHMProcessPerTabTest : public RenderFrameHostManagerTest {
 public:
  RFHMProcessPerTabTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kProcessPerTab);
  }
};

// Test that we still swap processes for BrowsingInstance changes even in
// --process-per-tab mode.  See http://crbug.com/343017.
// Disabled on Android: http://crbug.com/345873.
// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(OS_ANDROID) || defined(THREAD_SANITIZER)
#define MAYBE_BackFromWebUI DISABLED_BackFromWebUI
#else
#define MAYBE_BackFromWebUI BackFromWebUI
#endif
IN_PROC_BROWSER_TEST_F(RFHMProcessPerTabTest, MAYBE_BackFromWebUI) {
  ASSERT_TRUE(test_server()->Start());
  GURL original_url(test_server()->GetURL("files/title2.html"));
  NavigateToURL(shell(), original_url);

  // Visit a WebUI page with bindings.
  GURL webui_url(GURL(std::string(kChromeUIScheme) + "://" +
                      std::string(kChromeUIGpuHost)));
  NavigateToURL(shell(), webui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));

  // Go back and ensure we have no WebUI bindings.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();
  EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));
}

// crbug.com/372360
// The test loads url1, opens a link pointing to url2 in a new tab, and
// navigates the new tab to url1.
// The following is needed for the bug to happen:
//  - url1 must require webui bindings;
//  - navigating to url2 in the site instance of url1 should not swap
//   browsing instances, but should require a new site instance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, WebUIGetsBindings) {
  GURL url1(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIGpuHost));
  GURL url2(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIAccessibilityHost));

  // Visit a WebUI page with bindings.
  NavigateToURL(shell(), url1);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));
  SiteInstance* site_instance1 = shell()->web_contents()->GetSiteInstance();

  // Open a new tab. Initially it gets a render view in the original tab's
  // current site instance.
  TestNavigationObserver nav_observer(NULL);
  nav_observer.StartWatchingNewWebContents();
  ShellAddedObserver shao;
  OpenUrlViaClickTarget(shell()->web_contents(), url2);
  nav_observer.Wait();
  Shell* new_shell = shao.GetShell();
  WebContentsImpl* new_web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  SiteInstance* site_instance2 = new_web_contents->GetSiteInstance();

  EXPECT_NE(site_instance2, site_instance1);
  EXPECT_TRUE(site_instance2->IsRelatedSiteInstance(site_instance1));
  RenderViewHost* initial_rvh = new_web_contents->
      GetRenderManagerForTesting()->GetSwappedOutRenderViewHost(site_instance1);
  ASSERT_TRUE(initial_rvh);
  // The following condition is what was causing the bug.
  EXPECT_EQ(0, initial_rvh->GetEnabledBindings());

  // Navigate to url1 and check bindings.
  NavigateToURL(new_shell, url1);
  // The navigation should have used the first SiteInstance, otherwise
  // |initial_rvh| did not have a chance to be used.
  EXPECT_EQ(new_web_contents->GetSiteInstance(), site_instance1);
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
      new_web_contents->GetRenderViewHost()->GetEnabledBindings());
}

// crbug.com/424526
// The test loads a WebUI page in rocess-per-tab mode, then navigates to a blank
// page and then to a regular page. The bug reproduces if blank page is visited
// in between WebUI and regular page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ForceSwapAfterWebUIBindings) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  ASSERT_TRUE(test_server()->Start());

  const GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), web_ui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetRenderProcessHost()->GetID()));

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  GURL regular_page_url(test_server()->GetURL("files/title2.html"));
  NavigateToURL(shell(), regular_page_url);
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetRenderProcessHost()->GetID()));
}

class FileChooserDelegate : public WebContentsDelegate {
 public:
  FileChooserDelegate(const base::FilePath& file)
      : file_(file), file_chosen_(false) {}

  void RunFileChooser(WebContents* web_contents,
                      const FileChooserParams& params) override {
    // Send the selected file to the renderer process.
    FileChooserFileInfo file_info;
    file_info.file_path = file_;
    std::vector<FileChooserFileInfo> files;
    files.push_back(file_info);
    web_contents->GetRenderViewHost()->FilesSelectedInChooser(
        files, FileChooserParams::Open);

    file_chosen_ = true;
  }

  bool file_chosen() { return file_chosen_; }

 private:
  base::FilePath file_;
  bool file_chosen_;
};

// Test for http://crbug.com/262948.
// Flaky on Mac. http://crbug.com/452018
#if defined(OS_MACOSX)
#define MAYBE_RestoreFileAccessForHistoryNavigation \
  DISABLED_RestoreFileAccessForHistoryNavigation
#else
#define MAYBE_RestoreFileAccessForHistoryNavigation \
  RestoreFileAccessForHistoryNavigation
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_RestoreFileAccessForHistoryNavigation) {
  StartServer();
  base::FilePath file;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");

  // Navigate to url and get it to reference a file in its PageState.
  GURL url1(test_server()->GetURL("files/file_input.html"));
  NavigateToURL(shell(), url1);
  int process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();
  scoped_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(file));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Navigate to a different process without access to the file, and wait for
  // the old process to exit.
  RenderProcessHostWatcher exit_observer(
      shell()->web_contents()->GetRenderProcessHost(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  NavigateToURL(shell(), GetCrossSiteURL("files/title1.html"));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // Ensure that the file ended up in the PageState of the previous entry.
  NavigationEntry* prev_entry =
      shell()->web_contents()->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, files.size());
  EXPECT_EQ(file, files.at(0));

  // Go back, ending up in a different RenderProcessHost than before.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();
  EXPECT_NE(process_id,
            shell()->web_contents()->GetRenderProcessHost()->GetID());

  // Ensure that the file access still exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // Navigate to a same site page to trigger a PageState update and ensure the
  // renderer is not killed.
  EXPECT_TRUE(
      NavigateToURL(shell(), test_server()->GetURL("files/title2.html")));
}

// Test for http://crbug.com/441966.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RestoreSubframeFileAccessForHistoryNavigation) {
  StartServer();
  base::FilePath file;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");

  // Navigate to url and get it to reference a file in its PageState.
  GURL url1(test_server()->GetURL("files/file_input_subframe.html"));
  NavigateToURL(shell(), url1);
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  int process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();
  scoped_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(file));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Navigate to a different process without access to the file, and wait for
  // the old process to exit.
  RenderProcessHostWatcher exit_observer(
      shell()->web_contents()->GetRenderProcessHost(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  NavigateToURL(shell(), GetCrossSiteURL("files/title1.html"));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // Ensure that the file ended up in the PageState of the previous entry.
  NavigationEntry* prev_entry =
      shell()->web_contents()->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, files.size());
  EXPECT_EQ(file, files.at(0));

  // Go back, ending up in a different RenderProcessHost than before.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();
  EXPECT_NE(process_id,
            shell()->web_contents()->GetRenderProcessHost()->GetID());

  // Ensure that the file access still exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));
}

// This class implements waiting for RenderFrameHost destruction. It relies on
// the fact that RenderFrameDeleted event is fired when RenderFrameHost is
// destroyed.
// Note: RenderFrameDeleted is also fired when the process associated with the
// RenderFrameHost crashes, so this cannot be used in cases where process dying
// is expected.
class RenderFrameHostDestructionObserver : public WebContentsObserver {
 public:
  explicit RenderFrameHostDestructionObserver(RenderFrameHost* rfh)
      : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
        message_loop_runner_(new MessageLoopRunner),
        deleted_(false),
        render_frame_host_(rfh) {}
  ~RenderFrameHostDestructionObserver() override {}

  void Wait() {
    if (deleted_)
      return;

    message_loop_runner_->Run();
  }

  // WebContentsObserver implementation:
  void RenderFrameDeleted(RenderFrameHost* rfh) override {
    if (rfh == render_frame_host_) {
      CHECK(!deleted_);
      deleted_ = true;
    }

    if (deleted_ && message_loop_runner_->loop_running()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, message_loop_runner_->QuitClosure());
    }
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  bool deleted_;
  RenderFrameHost* render_frame_host_;
};

// Ensures that no RenderFrameHost/RenderViewHost objects are leaked when
// doing a simple cross-process navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       CleanupOnCrossProcessNavigation) {
  StartEmbeddedServer();

  // Do an initial navigation and capture objects we expect to be cleaned up
  // on cross-process navigation.
  GURL start_url = embedded_test_server()->GetURL("/title1.html");
  NavigateToURL(shell(), start_url);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  int32 orig_site_instance_id =
      root->current_frame_host()->GetSiteInstance()->GetId();
  int initial_process_id =
      root->current_frame_host()->GetSiteInstance()->GetProcess()->GetID();
  int initial_rfh_id = root->current_frame_host()->GetRoutingID();
  int initial_rvh_id =
      root->current_frame_host()->render_view_host()->GetRoutingID();

  // Navigate cross-process and ensure that cleanup is performed as expected.
  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/title2.html");
  RenderFrameHostDestructionObserver rfh_observer(root->current_frame_host());
  NavigateToURL(shell(), cross_site_url);
  rfh_observer.Wait();

  EXPECT_NE(orig_site_instance_id,
            root->current_frame_host()->GetSiteInstance()->GetId());
  EXPECT_FALSE(RenderFrameHost::FromID(initial_process_id, initial_rfh_id));
  EXPECT_FALSE(RenderViewHost::FromID(initial_process_id, initial_rvh_id));
}

// Ensure that the opener chain proxies and RVHs are properly reinitialized if
// a tab crashes and reloads.  See https://crbug.com/505090.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ReinitializeOpenerChainAfterCrashAndReload) {
  StartEmbeddedServer();

  GURL main_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance);

  // Open a popup and navigate it cross-site.
  Shell* new_shell = OpenPopup(shell()->web_contents(), "foo");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();

  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/title2.html");
  EXPECT_TRUE(NavigateToURL(new_shell, cross_site_url));

  scoped_refptr<SiteInstance> foo_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(foo_site_instance, orig_site_instance);

  // Kill the popup's process.
  RenderProcessHost* popup_process =
      popup_root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      popup_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  popup_process->Shutdown(0, false);
  crash_observer.Wait();
  EXPECT_FALSE(popup_root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(
      popup_root->current_frame_host()->render_view_host()->IsRenderViewLive());

  // The swapped-out RVH and proxy for the opener page in the foo.com
  // SiteInstance should not be live.
  RenderFrameHostManager* opener_manager = root->render_manager();
  RenderViewHostImpl* opener_rvh =
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get());
  EXPECT_TRUE(opener_rvh);
  EXPECT_FALSE(opener_rvh->IsRenderViewLive());
  RenderFrameProxyHost* opener_rfph =
      opener_manager->GetRenderFrameProxyHost(foo_site_instance.get());
  EXPECT_TRUE(opener_rfph);
  EXPECT_FALSE(opener_rfph->is_render_frame_proxy_live());

  // Re-navigate the popup to the same URL and check that this recreates the
  // opener's swapped out RVH and proxy in the foo.com SiteInstance.
  EXPECT_TRUE(NavigateToURL(new_shell, cross_site_url));
  EXPECT_TRUE(opener_rvh->IsRenderViewLive());
  EXPECT_TRUE(opener_rfph->is_render_frame_proxy_live());
}

// When two frames are same-origin but cross-process, they should behave as if
// they are not same-origin and should not crash.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SameOriginFramesInDifferentProcesses) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             "a.com", "/click-noreferrer-links.html"));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(nullptr, orig_site_instance.get());

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());"
      "saveWindowReference();",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Do a cross-site navigation that winds up same-site. The same-site
  // navigation to a.com will commit in a different process than the original
  // a.com window.
  NavigateToURL(new_shell, embedded_test_server()->GetURL(
                               "b.com", "/cross-site/a.com/title1.html"));
  if (AreAllSitesIsolatedForTesting()) {
    // In --site-per-process mode, both windows will actually be in the same
    // process.
    EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
              new_shell->web_contents()->GetSiteInstance());
  } else {
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
              new_shell->web_contents()->GetSiteInstance());
  }

  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send((function() {\n"
      "  try {\n"
      "    return getLastOpenedWindowLocation();\n"
      "  } catch (e) {\n"
      "    return e.toString();\n"
      "  }\n"
      "})())",
      &result));
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_THAT(result,
                ::testing::MatchesRegex("http://a.com:\\d+/title1.html"));
  } else {
    // Accessing a property with normal security checks should throw a
    // SecurityError if the same-origin windows are in different processes.
    EXPECT_THAT(result,
                ::testing::MatchesRegex("SecurityError: Blocked a frame with "
                                        "origin \"http://a.com:\\d+\" from "
                                        "accessing a cross-origin frame."));
  }
}

}  // namespace content
