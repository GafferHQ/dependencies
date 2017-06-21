// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_LINUX_V4L2_CAPTURE_DELEGATE_MULTI_PLANE_H_
#define MEDIA_VIDEO_CAPTURE_LINUX_V4L2_CAPTURE_DELEGATE_MULTI_PLANE_H_

#include "base/memory/ref_counted.h"
#include "media/video/capture/linux/v4l2_capture_delegate.h"

#if defined(OS_OPENBSD)
#error "OpenBSD does not support MPlane capture API."
#endif

#ifdef V4L2_TYPE_IS_MULTIPLANAR

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// V4L2 specifics for MPLANE API.
class V4L2CaptureDelegateMultiPlane final : public V4L2CaptureDelegate {
 public:
  V4L2CaptureDelegateMultiPlane(
      const VideoCaptureDevice::Name& device_name,
      const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
      int power_line_frequency);

 private:
  // BufferTracker derivation to implement construction semantics for MPLANE.
  class BufferTrackerMPlane final : public BufferTracker {
   public:
    bool Init(int fd, const v4l2_buffer& buffer) override;

   private:
    ~BufferTrackerMPlane() override {}
  };

  ~V4L2CaptureDelegateMultiPlane() override;

  // V4L2CaptureDelegate virtual methods implementation.
  scoped_refptr<BufferTracker> CreateBufferTracker() const override;
  bool FillV4L2Format(v4l2_format* format,
                      uint32_t width,
                      uint32_t height,
                      uint32_t pixelformat_fourcc) const override;
  void FinishFillingV4L2Buffer(v4l2_buffer* buffer) const override;
  void SetPayloadSize(const scoped_refptr<BufferTracker>& buffer_tracker,
                      const v4l2_buffer& buffer) const override;
  void SendBuffer(const scoped_refptr<BufferTracker>& buffer_tracker,
                  const v4l2_format& format) const override;

  // Vector to allocate and track as many v4l2_plane structs as planes, needed
  // for v4l2_buffer.m.planes. This is a scratchpad marked mutable to enable
  // using it in otherwise const methods.
  mutable std::vector<struct v4l2_plane> v4l2_planes_;
};

}  // namespace media

#endif  // V4L2_TYPE_IS_MULTIPLANAR
#endif  // MEDIA_VIDEO_CAPTURE_LINUX_V4L2_CAPTURE_DELEGATE_SINGLE_PLANE_H_
