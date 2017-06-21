// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const base::CommandLine::StringType FAKE_DEVICE_FLAG =
#if defined(OS_WIN)
    base::ASCIIToUTF16(switches::kUseFakeDeviceForMediaStream);
#else
    switches::kUseFakeDeviceForMediaStream;
#endif

bool IsUseFakeDeviceForMediaStream(const base::CommandLine::StringType& arg) {
  return arg.find(FAKE_DEVICE_FLAG) != std::string::npos;
}

void RemoveFakeDeviceFromCommandLine(base::CommandLine* command_line) {
  base::CommandLine::StringVector argv = command_line->argv();
  argv.erase(std::remove_if(argv.begin(), argv.end(),
                            IsUseFakeDeviceForMediaStream),
             argv.end());
  command_line->InitFromArgv(argv);
}

}  // namespace

namespace content {

// This class doesn't inherit from WebRtcContentBrowserTestBase like the others
// since we want it to actually acquire the real webcam on the system (if there
// is one).
class WebRtcWebcamBrowserTest: public ContentBrowserTest {
 public:
  ~WebRtcWebcamBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // The content_browsertests run with this flag by default, and this test is
    // the only current exception to that rule, so just remove the flag
    // --use-fake-device-for-media-stream here. We could also have all tests
    // involving media streams add this flag explicitly, but it will be really
    // unintuitive for developers to write tests involving media stream and have
    // them fail on what looks like random bots, so running with fake devices
    // is really a reasonable default.
    RemoveFakeDeviceFromCommandLine(command_line);
  }

  void SetUp() override {
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }
};

// The test is tagged as MANUAL since the webcam is a system-level resource; we
// only want it to run on bots where we can ensure sequential execution. The
// Android bots will run the test since they ignore MANUAL, but that's what we
// want here since the bot runs tests sequentially on the device.
IN_PROC_BROWSER_TEST_F(WebRtcWebcamBrowserTest,
                       MANUAL_CanAcquireVgaOnRealWebcam) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL(
      "/media/getusermedia-real-webcam.html"));
  NavigateToURL(shell(), url);

  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(shell()->web_contents(),
                                            "hasVideoInputDeviceOnSystem()",
                                            &result));
  if (result != "has-video-input-device") {
    VLOG(0) << "No video device; skipping test...";
    return;
  }

  // GetUserMedia should acquire VGA by default.
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "getUserMediaAndReturnVideoDimensions({video: true})",
      &result));

  if (result == "640x480" || result == "480x640") {
    // Don't care if the device happens to be in landscape or portrait mode
    // since we don't know how it is oriented in the lab :)
    return;
  }
  FAIL() << "Expected resolution to be 640x480 or 480x640, got" << result;
}

}  // namespace content
