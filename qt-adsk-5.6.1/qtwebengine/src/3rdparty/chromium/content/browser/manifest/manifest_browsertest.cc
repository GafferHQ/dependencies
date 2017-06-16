// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/manifest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"


namespace content {

class ManifestBrowserTest;

// Mock of a WebContentsDelegate that catches messages sent to the console.
class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockWebContentsDelegate(WebContents* web_contents, ManifestBrowserTest* test)
      : web_contents_(web_contents),
        test_(test) {
  }

  bool AddMessageToConsole(WebContents* source,
                           int32 level,
                           const base::string16& message,
                           int32 line_no,
                           const base::string16& source_id) override;

 private:
  WebContents* web_contents_;
  ManifestBrowserTest* test_;
};

class ManifestBrowserTest : public ContentBrowserTest  {
 protected:
  friend MockWebContentsDelegate;

  ManifestBrowserTest()
      : console_error_count_(0) {
    cors_embedded_test_server_.reset(new net::test_server::EmbeddedTestServer);
    base::FilePath test_data_dir;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    cors_embedded_test_server_->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("content/test/data/"));
  }

  ~ManifestBrowserTest() override {}

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    DCHECK(shell()->web_contents());

    mock_web_contents_delegate_.reset(
        new MockWebContentsDelegate(shell()->web_contents(), this));
    shell()->web_contents()->SetDelegate(mock_web_contents_delegate_.get());
  }

  void GetManifestAndWait() {
    shell()->web_contents()->GetManifest(
        base::Bind(&ManifestBrowserTest::OnGetManifest,
                   base::Unretained(this)));

    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

  void OnGetManifest(const Manifest& manifest) {
    manifest_ = manifest;
    message_loop_runner_->Quit();
  }

  const Manifest& manifest() const {
    return manifest_;
  }

  unsigned int console_error_count() const {
    return console_error_count_;
  }

  void OnReceivedConsoleError() {
    console_error_count_++;
  }

  net::test_server::EmbeddedTestServer* cors_embedded_test_server() const {
    return cors_embedded_test_server_.get();
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  scoped_ptr<MockWebContentsDelegate> mock_web_contents_delegate_;
  scoped_ptr<net::test_server::EmbeddedTestServer> cors_embedded_test_server_;
  Manifest manifest_;
  int console_error_count_;

  DISALLOW_COPY_AND_ASSIGN(ManifestBrowserTest);
};

// The implementation of AddMessageToConsole isn't inlined because it needs
// to know about |test_|.
bool MockWebContentsDelegate::AddMessageToConsole(
    WebContents* source,
    int32 level,
    const base::string16& message,
    int32 line_no,
    const base::string16& source_id) {
  DCHECK(source == web_contents_);

  if (level == logging::LOG_ERROR)
    test_->OnReceivedConsoleError();
  return false;
}

// If a page has no manifest, requesting a manifest should return the empty
// manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, NoManifest) {
  GURL test_url = GetTestUrl("manifest", "no-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());
}

// If a page manifest points to a 404 URL, requesting the manifest should return
// the empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, 404Manifest) {
  GURL test_url = GetTestUrl("manifest", "404-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());
}

// If a page has an empty manifest, requesting the manifest should return the
// empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, EmptyManifest) {
  GURL test_url = GetTestUrl("manifest", "empty-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());
}

// If a page's manifest can't be parsed correctly, requesting the manifest
// should return an empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, ParseErrorManifest) {
  GURL test_url = GetTestUrl("manifest", "parse-error-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_EQ(1u, console_error_count());
}

// If a page has a manifest that can be fetched and parsed, requesting the
// manifest should return a properly filled manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DummyManifest) {
  GURL test_url = GetTestUrl("manifest", "dummy-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());
}

// If a page changes manifest during its life-time, requesting the manifest
// should return the current manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, DynamicManifest) {
  GURL test_url = GetTestUrl("manifest", "dynamic-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  {
    GetManifestAndWait();
    EXPECT_TRUE(manifest().IsEmpty());
  }

  {
    std::string manifest_url =
        GetTestUrl("manifest", "dummy-manifest.json").spec();
    ASSERT_TRUE(content::ExecuteScript(
        shell()->web_contents(), "setManifestTo('" + manifest_url + "')"));

    GetManifestAndWait();
    EXPECT_FALSE(manifest().IsEmpty());
  }

  {
    std::string manifest_url =
        GetTestUrl("manifest", "empty-manifest.json").spec();
    ASSERT_TRUE(content::ExecuteScript(
        shell()->web_contents(), "setManifestTo('" + manifest_url + "')"));

    GetManifestAndWait();
    EXPECT_TRUE(manifest().IsEmpty());
  }

  EXPECT_EQ(0u, console_error_count());
}

// If a page's manifest lives in a different origin, it should follow the CORS
// rules and requesting the manifest should return an empty manifest (unless the
// response contains CORS headers).
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, CORSManifest) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(cors_embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_NE(embedded_test_server()->port(),
            cors_embedded_test_server()->port());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  std::string manifest_url = cors_embedded_test_server()->GetURL(
      "/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(content::ExecuteScript(shell()->web_contents(),
                                     "setManifestTo('" + manifest_url + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());

  EXPECT_EQ(0u, console_error_count());

  // The purpose of this second load is to make sure the first load is fully
  // finished. The first load will fail because of Access Control error but the
  // underlying Blink loader will continue fetching the file. There is no
  // reliable way to know when the fetch is finished from the browser test
  // except by fetching the same file from same origin, making it succeed when
  // it is actually fully loaded.
  manifest_url =
      embedded_test_server()->GetURL("/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(content::ExecuteScript(shell()->web_contents(),
                                     "setManifestTo('" + manifest_url + "')"));
  GetManifestAndWait();
}

// If a page's manifest lives in a different origin, it should be accessible if
// it has valid access controls headers.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, CORSManifestWithAcessControls) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(cors_embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_NE(embedded_test_server()->port(),
            cors_embedded_test_server()->port());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  std::string manifest_url = cors_embedded_test_server()->GetURL(
      "/manifest/manifest-cors.json").spec();
  ASSERT_TRUE(content::ExecuteScript(shell()->web_contents(),
                                     "setManifestTo('" + manifest_url + "')"));

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());

  EXPECT_EQ(0u, console_error_count());
}

// If a page's manifest is in an insecure origin while the page is in a secure
// origin, requesting the manifest should return the empty manifest.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, MixedContentManifest) {
  scoped_ptr<net::SpawnedTestServer> https_server(new net::SpawnedTestServer(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::BaseTestServer::SSLOptions(net::BaseTestServer::SSLOptions::CERT_OK),
      base::FilePath(FILE_PATH_LITERAL("content/test/data"))));

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(https_server->Start());

  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dynamic-manifest.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  std::string manifest_url =
      https_server->GetURL("/manifest/dummy-manifest.json").spec();
  ASSERT_TRUE(content::ExecuteScript(shell()->web_contents(),
                                     "setManifestTo('" + manifest_url + "')"));

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());

  EXPECT_EQ(0u, console_error_count());
}

// If a page's manifest has some parsing errors, they should show up in the
// developer console.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, ParsingErrorsManifest) {
  GURL test_url = GetTestUrl("manifest", "parsing-errors.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_TRUE(manifest().IsEmpty());
  EXPECT_EQ(6u, console_error_count());
}

// If a page has a manifest and the page is navigated to a page without a
// manifest, the page's manifest should be updated.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, Navigation) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  {
    GURL test_url =
        embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    navigation_observer.Wait();

    GetManifestAndWait();
    EXPECT_FALSE(manifest().IsEmpty());
    EXPECT_EQ(0u, console_error_count());
  }

  {
    GURL test_url =
        embedded_test_server()->GetURL("/manifest/no-manifest.html");

    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    navigation_observer.Wait();

    GetManifestAndWait();
    EXPECT_TRUE(manifest().IsEmpty());
    EXPECT_EQ(0u, console_error_count());
  }
}

// If a page has a manifest and the page is navigated using pushState (ie. same
// page), it should keep its manifest state.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, PushStateNavigation) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    navigation_observer.Wait();
  }

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(content::ExecuteScript(
        shell()->web_contents(),
        "history.pushState({foo: \"bar\"}, 'page', 'page.html');"));
    navigation_observer.Wait();
  }

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());
}

// If a page has a manifest and is navigated using an anchor (ie. same page), it
// should keep its manifest state.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, AnchorNavigation) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL test_url =
      embedded_test_server()->GetURL("/manifest/dummy-manifest.html");

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    shell()->LoadURL(test_url);
    navigation_observer.Wait();
  }

  {
    TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
    ASSERT_TRUE(content::ExecuteScript(
        shell()->web_contents(),
        "var a = document.createElement('a'); a.href='#foo';"
        "document.body.appendChild(a); a.click();"));
    navigation_observer.Wait();
  }

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());
}

namespace {

scoped_ptr<net::test_server::HttpResponse> CustomHandleRequestForCookies(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/index.html") {
    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<html><head>"
        "<link rel=manifest crossorigin='use-credentials' href=/manifest.json>"
        "</head></html>");
    return http_response.Pass();
  }

  const auto& iter = request.headers.find("Cookie");
  if (iter == request.headers.end() || request.relative_url != "/manifest.json")
    return scoped_ptr<net::test_server::HttpResponse>();

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("application/json");
  http_response->set_content(
      base::StringPrintf("{\"name\": \"%s\"}", iter->second.c_str()));

  return http_response.Pass();
}

}  // anonymous namespace

// This tests that when fetching a Manifest with 'use-credentials' set, the
// cookies associated with it are passed along the request.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, UseCredentialsSendCookies) {
  scoped_ptr<net::test_server::EmbeddedTestServer> custom_embedded_test_server(
      new net::test_server::EmbeddedTestServer());
  custom_embedded_test_server->RegisterRequestHandler(
      base::Bind(&CustomHandleRequestForCookies));

  ASSERT_TRUE(custom_embedded_test_server->InitializeAndWaitUntilReady());

  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        custom_embedded_test_server->base_url(),
                        "foobar"));

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(custom_embedded_test_server->GetURL("/index.html"));
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());

  // The custom embedded test server will fill the name field with the cookie
  // content.
  EXPECT_TRUE(base::EqualsASCII(manifest().name.string(), "foobar"));
}

namespace {

scoped_ptr<net::test_server::HttpResponse> CustomHandleRequestForNoCookies(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/index.html") {
    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<html><head><link rel=manifest href=/manifest.json></head></html>");
    return http_response.Pass();
  }

  const auto& iter = request.headers.find("Cookie");
  if (iter != request.headers.end() || request.relative_url != "/manifest.json")
    return scoped_ptr<net::test_server::HttpResponse>();

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("application/json");
  http_response->set_content("{\"name\": \"no cookies\"}");

  return http_response.Pass();
}

}  // anonymous namespace

// This tests that when fetching a Manifest without 'use-credentials' set, the
// cookies associated with it are not passed along the request.
IN_PROC_BROWSER_TEST_F(ManifestBrowserTest, NoUseCredentialsNoCookies) {
  scoped_ptr<net::test_server::EmbeddedTestServer> custom_embedded_test_server(
      new net::test_server::EmbeddedTestServer());
  custom_embedded_test_server->RegisterRequestHandler(
      base::Bind(&CustomHandleRequestForNoCookies));

  ASSERT_TRUE(custom_embedded_test_server->InitializeAndWaitUntilReady());

  ASSERT_TRUE(SetCookie(shell()->web_contents()->GetBrowserContext(),
                        custom_embedded_test_server->base_url(),
                        "foobar"));

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(custom_embedded_test_server->GetURL("/index.html"));
  navigation_observer.Wait();

  GetManifestAndWait();
  EXPECT_FALSE(manifest().IsEmpty());
  EXPECT_EQ(0u, console_error_count());

  // The custom embedded test server will fill set the name to 'no cookies' if
  // it did not find cookies.
  EXPECT_TRUE(base::EqualsASCII(manifest().name.string(), "no cookies"));
}

} // namespace content
