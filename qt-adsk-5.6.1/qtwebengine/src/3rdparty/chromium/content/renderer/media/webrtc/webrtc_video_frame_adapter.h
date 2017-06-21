// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_

#include "media/base/video_frame.h"
#include "third_party/webrtc/common_video/interface/video_frame_buffer.h"

namespace content {
// Thin adapter from media::VideoFrame to webrtc::VideoFrameBuffer. This
// implementation is read-only and will return null if trying to get a
// non-const pointer to the pixel data. This object will be accessed from
// different threads, but that's safe since it's read-only.
class WebRtcVideoFrameAdapter : public webrtc::VideoFrameBuffer {
 public:
  explicit WebRtcVideoFrameAdapter(
      const scoped_refptr<media::VideoFrame>& frame);

 private:
  int width() const override;
  int height() const override;

  const uint8_t* data(webrtc::PlaneType type) const override;

  uint8_t* data(webrtc::PlaneType type) override;

  int stride(webrtc::PlaneType type) const override;

  void* native_handle() const override;

  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override;

  friend class rtc::RefCountedObject<WebRtcVideoFrameAdapter>;

 protected:
  ~WebRtcVideoFrameAdapter() override;

  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_FRAME_ADAPTER_H_
