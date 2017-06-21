// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ipc/ipc_security_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

std::string GetCookieFromJS(RenderFrameHost* frame) {
  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame, "window.domAutomationController.send(document.cookie);", &cookie));
  return cookie;
}

}  // namespace

using RenderMessageFilterBrowserTest = ContentBrowserTest;

// Exercises basic cookie operations via javascript, including an http page
// interacting with secure cookies.
IN_PROC_BROWSER_TEST_F(RenderMessageFilterBrowserTest, Cookies) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  SetupCrossSiteRedirector(embedded_test_server());

  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // The server sends a HttpOnly cookie. The RenderMessageFilter should never
  // allow this to be sent to any renderer process.
  GURL https_url = https_server.GetURL("set-cookie?notforjs=1;HttpOnly");
  GURL http_url = embedded_test_server()->GetURL("/frame_with_load_event.html");

  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell(), http_url);
  NavigateToURL(shell2, https_url);

  WebContentsImpl* web_contents_https =
      static_cast<WebContentsImpl*>(shell2->web_contents());
  WebContentsImpl* web_contents_http =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  EXPECT_EQ("http://127.0.0.1/",
            web_contents_http->GetSiteInstance()->GetSiteURL().spec());
  EXPECT_EQ("https://127.0.0.1/",
            web_contents_https->GetSiteInstance()->GetSiteURL().spec());

  EXPECT_NE(web_contents_http->GetSiteInstance()->GetProcess(),
            web_contents_https->GetSiteInstance()->GetProcess());

  EXPECT_EQ("", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // Non-TLS page writes secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_http->GetMainFrame(),
                            "document.cookie = 'A=1; secure;';"));
  EXPECT_EQ("A=1", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_http->GetMainFrame(),
                            "document.cookie = 'B=2';"));
  EXPECT_EQ("A=1; B=2", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // Non-TLS page writes secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_https->GetMainFrame(),
                            "document.cookie = 'C=3;secure;';"));
  EXPECT_EQ("A=1; B=2; C=3",
            GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_https->GetMainFrame(),
                            "document.cookie = 'D=4';"));
  EXPECT_EQ("A=1; B=2; C=3; D=4",
            GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2; D=4", GetCookieFromJS(web_contents_http->GetMainFrame()));
}

// The RenderMessageFilter will kill processes when they access the cookies of
// sites other than the site the process is dedicated to, under site isolation.
IN_PROC_BROWSER_TEST_F(RenderMessageFilterBrowserTest,
                       CrossSiteCookieSecurityEnforcement) {
  // The code under test is only active under site isolation.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return;
  }

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  SetupCrossSiteRedirector(embedded_test_server());
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_with_load_event.html"));

  WebContentsImpl* tab = static_cast<WebContentsImpl*>(shell()->web_contents());

  // The iframe on the http page should get its own process.
  FrameTreeVisualizer v;
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetFrameTree()->root()));

  RenderFrameHost* main_frame = tab->GetMainFrame();
  RenderFrameHost* iframe =
      tab->GetFrameTree()->root()->child_at(0)->current_frame_host();

  EXPECT_NE(iframe->GetProcess(), main_frame->GetProcess());

  // Try to get cross-site cookies from the subframe's process and wait for it
  // to be killed.
  std::string response;
  FrameHostMsg_GetCookies illegal_get_cookies(
      iframe->GetRoutingID(), GURL("http://127.0.0.1/"),
      GURL("http://127.0.0.1/"), &response);

  RenderProcessHostWatcher iframe_killed(
      iframe->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      iframe->GetProcess()->GetChannel(), illegal_get_cookies);

  iframe_killed.Wait();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/ (no process)",
      v.DepictFrameTree(tab->GetFrameTree()->root()));

  // Now set a cross-site cookie from the main frame's process and wait for it
  // to be killed.
  RenderProcessHostWatcher main_frame_killed(
      tab->GetMainFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  FrameHostMsg_SetCookie illegal_set_cookie(tab->GetMainFrame()->GetRoutingID(),
                                            GURL("https://baz.com/"),
                                            GURL("https://baz.com/"), "pwn=ed");
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      tab->GetMainFrame()->GetProcess()->GetChannel(), illegal_set_cookie);

  main_frame_killed.Wait();

  EXPECT_EQ(
      " Site A\n"
      "Where A = http://127.0.0.1/ (no process)",
      v.DepictFrameTree(tab->GetFrameTree()->root()));
}

}  // namespace content
