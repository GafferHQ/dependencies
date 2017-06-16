/*
 * libjingle
 * Copyright 2008 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Author: Ronghua Wu (ronghuawu@google.com)

#include <string>

#include "talk/media/base/testutils.h"
#include "talk/media/webrtc/webrtcpassthroughrender.h"
#include "webrtc/base/gunit.h"

class WebRtcPassthroughRenderTest : public testing::Test {
 public:
  class ExternalRenderer : public webrtc::VideoRenderCallback {
   public:
    ExternalRenderer() : frame_num_(0) {
    }

    virtual ~ExternalRenderer() {
    }

    virtual int32_t RenderFrame(const uint32_t stream_id,
                                const webrtc::VideoFrame& videoFrame) {
      ++frame_num_;
      LOG(INFO) << "RenderFrame stream_id: " << stream_id
                << " frame_num: " << frame_num_;
      return 0;
    }

    int frame_num() const {
      return frame_num_;
    }

   private:
    int frame_num_;
  };

  WebRtcPassthroughRenderTest()
      : renderer_(new cricket::WebRtcPassthroughRender()) {
  }

  ~WebRtcPassthroughRenderTest() {
  }

  webrtc::VideoRenderCallback* AddIncomingRenderStream(int stream_id) {
    return renderer_->AddIncomingRenderStream(stream_id, 0, 0, 0, 0, 0);
  }

  bool HasIncomingRenderStream(int stream_id) {
    return renderer_->HasIncomingRenderStream(stream_id);
  }

  bool DeleteIncomingRenderStream(int stream_id) {
    return (renderer_->DeleteIncomingRenderStream(stream_id) == 0);
  }

  bool AddExternalRenderCallback(int stream_id,
                                 webrtc::VideoRenderCallback* renderer) {
    return (renderer_->AddExternalRenderCallback(stream_id, renderer) == 0);
  }

  bool StartRender(int stream_id) {
    return (renderer_->StartRender(stream_id) == 0);
  }

  bool StopRender(int stream_id) {
    return (renderer_->StopRender(stream_id) == 0);
  }

 private:
  rtc::scoped_ptr<cricket::WebRtcPassthroughRender> renderer_;
};

TEST_F(WebRtcPassthroughRenderTest, Streams) {
  const int stream_id1 = 1234;
  const int stream_id2 = 5678;
  const int stream_id3 = 9012;  // A stream that doesn't exist.
  webrtc::VideoRenderCallback* stream = NULL;
  // Add a new stream
  stream = AddIncomingRenderStream(stream_id1);
  EXPECT_TRUE(stream != NULL);
  EXPECT_TRUE(HasIncomingRenderStream(stream_id1));
  // Tried to add a already existed stream should return null
  stream =AddIncomingRenderStream(stream_id1);
  EXPECT_TRUE(stream == NULL);
  stream = AddIncomingRenderStream(stream_id2);
  EXPECT_TRUE(stream != NULL);
  EXPECT_TRUE(HasIncomingRenderStream(stream_id2));
  // Remove the stream
  EXPECT_FALSE(DeleteIncomingRenderStream(stream_id3));
  EXPECT_TRUE(DeleteIncomingRenderStream(stream_id2));
  EXPECT_TRUE(!HasIncomingRenderStream(stream_id2));
  // Add back the removed stream
  stream = AddIncomingRenderStream(stream_id2);
  EXPECT_TRUE(stream != NULL);
  EXPECT_TRUE(HasIncomingRenderStream(stream_id2));
}

TEST_F(WebRtcPassthroughRenderTest, Renderer) {
  webrtc::VideoFrame frame;
  const int stream_id1 = 1234;
  const int stream_id2 = 5678;
  const int stream_id3 = 9012;  // A stream that doesn't exist.
  webrtc::VideoRenderCallback* stream1 = NULL;
  webrtc::VideoRenderCallback* stream2 = NULL;
  // Add two new stream
  stream1 = AddIncomingRenderStream(stream_id1);
  EXPECT_TRUE(stream1 != NULL);
  EXPECT_TRUE(HasIncomingRenderStream(stream_id1));
  stream2 = AddIncomingRenderStream(stream_id2);
  EXPECT_TRUE(stream2 != NULL);
  EXPECT_TRUE(HasIncomingRenderStream(stream_id2));
  // Register the external renderer
  WebRtcPassthroughRenderTest::ExternalRenderer renderer1;
  WebRtcPassthroughRenderTest::ExternalRenderer renderer2;
  EXPECT_FALSE(AddExternalRenderCallback(stream_id3, &renderer1));
  EXPECT_TRUE(AddExternalRenderCallback(stream_id1, &renderer1));
  EXPECT_TRUE(AddExternalRenderCallback(stream_id2, &renderer2));
  int test_frame_num = 10;
  // RenderFrame without starting the render
  for (int i = 0; i < test_frame_num; ++i) {
    stream1->RenderFrame(stream_id1, frame);
  }
  EXPECT_EQ(0, renderer1.frame_num());
  // Start the render and test again.
  EXPECT_FALSE(StartRender(stream_id3));
  EXPECT_TRUE(StartRender(stream_id1));
  for (int i = 0; i < test_frame_num; ++i) {
    stream1->RenderFrame(stream_id1, frame);
  }
  EXPECT_EQ(test_frame_num, renderer1.frame_num());
  // Stop the render and test again.
  EXPECT_FALSE(StopRender(stream_id3));
  EXPECT_TRUE(StopRender(stream_id1));
  for (int i = 0; i < test_frame_num; ++i) {
    stream1->RenderFrame(stream_id1, frame);
  }
  // The frame number should not have changed.
  EXPECT_EQ(test_frame_num, renderer1.frame_num());

  // Test on stream2 with a differnt number.
  EXPECT_TRUE(StartRender(stream_id2));
  test_frame_num = 30;
  for (int i = 0; i < test_frame_num; ++i) {
    stream2->RenderFrame(stream_id2, frame);
  }
  EXPECT_EQ(test_frame_num, renderer2.frame_num());
}
