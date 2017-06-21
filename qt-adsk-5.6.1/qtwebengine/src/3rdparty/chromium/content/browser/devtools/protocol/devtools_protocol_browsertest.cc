// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

}

class DevToolsProtocolTest : public ContentBrowserTest,
                             public DevToolsAgentHostClient {
 public:
  DevToolsProtocolTest()
      : last_sent_id_(0),
        in_dispatch_(false) {
  }

 protected:
  void SendCommand(const std::string& method,
                   scoped_ptr<base::DictionaryValue> params) {
    SendCommand(method, params.Pass(), true);
  }

  void SendCommand(const std::string& method,
                   scoped_ptr<base::DictionaryValue> params,
                   bool wait) {
    in_dispatch_ = true;
    base::DictionaryValue command;
    command.SetInteger(kIdParam, ++last_sent_id_);
    command.SetString(kMethodParam, method);
    if (params)
      command.Set(kParamsParam, params.release());

    std::string json_command;
    base::JSONWriter::Write(command, &json_command);
    agent_host_->DispatchProtocolMessage(json_command);
    // Some messages are dispatched synchronously.
    // Only run loop if we are not finished yet.
    if (in_dispatch_ && wait)
      base::MessageLoop::current()->Run();
    in_dispatch_ = false;
  }

  bool HasValue(const std::string& path) {
    base::Value* value = 0;
    return result_->Get(path, &value);
  }

  bool HasListItem(const std::string& path_to_list,
                   const std::string& name,
                   const std::string& value) {
    base::ListValue* list;
    if (!result_->GetList(path_to_list, &list))
      return false;

    for (size_t i = 0; i != list->GetSize(); i++) {
      base::DictionaryValue* item;
      if (!list->GetDictionary(i, &item))
        return false;
      std::string id;
      if (!item->GetString(name, &id))
        return false;
      if (id == value)
        return true;
    }
    return false;
  }

  void Attach() {
    agent_host_ = DevToolsAgentHost::GetOrCreateFor(shell()->web_contents());
    agent_host_->AttachClient(this);
  }

  void TearDownOnMainThread() override {
    if (agent_host_) {
      agent_host_->DetachClient();
      agent_host_ = nullptr;
    }
  }

  scoped_ptr<base::DictionaryValue> result_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  int last_sent_id_;
  std::vector<int> result_ids_;
  std::vector<std::string> notifications_;

 private:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    scoped_ptr<base::DictionaryValue> root(static_cast<base::DictionaryValue*>(
        base::JSONReader::DeprecatedRead(message)));
    int id;
    if (root->GetInteger("id", &id)) {
      result_ids_.push_back(id);
      base::DictionaryValue* result;
      EXPECT_TRUE(root->GetDictionary("result", &result));
      result_.reset(result->DeepCopy());
      in_dispatch_ = false;
      if (base::MessageLoop::current()->is_running())
        base::MessageLoop::current()->QuitNow();
    } else {
      std::string notification;
      EXPECT_TRUE(root->GetString("method", &notification));
      notifications_.push_back(notification);
    }
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override {
    EXPECT_TRUE(false);
  }

  bool in_dispatch_;
};

class SyntheticKeyEventTest : public DevToolsProtocolTest {
 protected:
  void SendKeyEvent(const std::string& type,
                    int modifier,
                    int windowsKeyCode,
                    int nativeKeyCode) {
    scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
    params->SetString("type", type);
    params->SetInteger("modifiers", modifier);
    params->SetInteger("windowsVirtualKeyCode", windowsKeyCode);
    params->SetInteger("nativeVirtualKeyCode", nativeKeyCode);
    SendCommand("Input.dispatchKeyEvent", params.Pass());
  }
};

IN_PROC_BROWSER_TEST_F(SyntheticKeyEventTest, KeyEventSynthesizeKeyIdentifier) {
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);
  Attach();
  ASSERT_TRUE(content::ExecuteScript(
      shell()->web_contents()->GetRenderViewHost(),
      "function handleKeyEvent(event) {"
        "domAutomationController.setAutomationId(0);"
        "domAutomationController.send(event.keyIdentifier);"
      "}"
      "document.body.addEventListener('keydown', handleKeyEvent);"
      "document.body.addEventListener('keyup', handleKeyEvent);"));

  DOMMessageQueue dom_message_queue;

  // Send enter (keycode 13).
  SendKeyEvent("rawKeyDown", 0, 13, 13);
  SendKeyEvent("keyUp", 0, 13, 13);

  std::string key_identifier;
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"Enter\"", key_identifier);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"Enter\"", key_identifier);

  // Send escape (keycode 27).
  SendKeyEvent("rawKeyDown", 0, 27, 27);
  SendKeyEvent("keyUp", 0, 27, 27);

  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"U+001B\"", key_identifier);
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&key_identifier));
  EXPECT_EQ("\"U+001B\"", key_identifier);
}

class CaptureScreenshotTest : public DevToolsProtocolTest {
 private:
#if !defined(OS_ANDROID)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
#endif
};

// Does not link on Android
#if defined(OS_ANDROID)
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#elif defined(OS_MACOSX)  // Fails on 10.9. http://crbug.com/430620
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#elif defined(MEMORY_SANITIZER)
// Also fails under MSAN. http://crbug.com/423583
#define MAYBE_CaptureScreenshot DISABLED_CaptureScreenshot
#else
#define MAYBE_CaptureScreenshot CaptureScreenshot
#endif
IN_PROC_BROWSER_TEST_F(CaptureScreenshotTest, MAYBE_CaptureScreenshot) {
  shell()->LoadURL(GURL("about:blank"));
  Attach();
  EXPECT_TRUE(content::ExecuteScript(
      shell()->web_contents()->GetRenderViewHost(),
      "document.body.style.background = '#123456'"));
  SendCommand("Page.captureScreenshot", nullptr);
  std::string base64;
  EXPECT_TRUE(result_->GetString("data", &base64));
  std::string png;
  EXPECT_TRUE(base::Base64Decode(base64, &png));
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(png.data()),
                        png.size(), &bitmap);
  SkColor color(bitmap.getColor(0, 0));
  EXPECT_TRUE(std::abs(0x12-(int)SkColorGetR(color)) <= 1);
  EXPECT_TRUE(std::abs(0x34-(int)SkColorGetG(color)) <= 1);
  EXPECT_TRUE(std::abs(0x56-(int)SkColorGetB(color)) <= 1);
}

#if defined(OS_ANDROID)
// Disabled, see http://crbug.com/469947.
IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, DISABLED_SynthesizePinchGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  int old_width;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(window.innerWidth)", &old_width));

  int old_height;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(window.innerHeight)", &old_height));

  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetInteger("x", old_width / 2);
  params->SetInteger("y", old_height / 2);
  params->SetDouble("scaleFactor", 2.0);
  SendCommand("Input.synthesizePinchGesture", params.Pass());

  int new_width;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(window.innerWidth)", &new_width));
  ASSERT_DOUBLE_EQ(2.0, static_cast<double>(old_width) / new_width);

  int new_height;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(window.innerHeight)", &new_height));
  ASSERT_DOUBLE_EQ(2.0, static_cast<double>(old_height) / new_height);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, SynthesizeScrollGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  int scroll_top;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(document.body.scrollTop)", &scroll_top));
  ASSERT_EQ(0, scroll_top);

  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetInteger("x", 0);
  params->SetInteger("y", 0);
  params->SetInteger("xDistance", 0);
  params->SetInteger("yDistance", -100);
  SendCommand("Input.synthesizeScrollGesture", params.Pass());

  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(document.body.scrollTop)", &scroll_top));
  ASSERT_EQ(100, scroll_top);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, SynthesizeTapGesture) {
  GURL test_url = GetTestUrl("devtools", "synthetic_gesture_tests.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();

  int scroll_top;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(document.body.scrollTop)", &scroll_top));
  ASSERT_EQ(0, scroll_top);

  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  params->SetInteger("x", 16);
  params->SetInteger("y", 16);
  params->SetString("gestureSourceType", "touch");
  SendCommand("Input.synthesizeTapGesture", params.Pass());

  // The link that we just tapped should take us to the bottom of the page. The
  // new value of |document.body.scrollTop| will depend on the screen dimensions
  // of the device that we're testing on, but in any case it should be greater
  // than 0.
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      shell()->web_contents(),
      "domAutomationController.send(document.body.scrollTop)", &scroll_top));
  ASSERT_GT(scroll_top, 0);
}
#endif  // defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, NavigationPreservesMessages) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url = test_server()->GetURL("files/devtools/navigation.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  Attach();
  SendCommand("Page.enable", nullptr, false);

  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  test_url = GetTestUrl("devtools", "navigation.html");
  params->SetString("url", test_url.spec());
  SendCommand("Page.navigate", params.Pass(), true);

  bool enough_results = result_ids_.size() >= 2u;
  EXPECT_TRUE(enough_results);
  if (enough_results) {
    EXPECT_EQ(1, result_ids_[0]);  // Page.enable
    EXPECT_EQ(2, result_ids_[1]);  // Page.navigate
  }

  enough_results = notifications_.size() >= 1u;
  EXPECT_TRUE(enough_results);
  bool found_frame_notification = false;
  for (const std::string& notification : notifications_) {
    if (notification == "Page.frameStartedLoading")
      found_frame_notification = true;
  }
  EXPECT_TRUE(found_frame_notification);
}

}  // namespace content
