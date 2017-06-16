// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/linux/v4l2_capture_delegate.h"

#include <poll.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "media/base/bind_to_current_loop.h"
#include "media/video/capture/linux/v4l2_capture_delegate_multi_plane.h"
#include "media/video/capture/linux/v4l2_capture_delegate_single_plane.h"
#include "media/video/capture/linux/video_capture_device_linux.h"

#if !defined(OS_OPENBSD)
#include <linux/version.h>
#endif

namespace media {

// Desired number of video buffers to allocate. The actual number of allocated
// buffers by v4l2 driver can be higher or lower than this number.
// kNumVideoBuffers should not be too small, or Chrome may not return enough
// buffers back to driver in time.
const uint32 kNumVideoBuffers = 4;
// Timeout in milliseconds v4l2_thread_ blocks waiting for a frame from the hw.
const int kCaptureTimeoutMs = 200;
// The number of continuous timeouts tolerated before treated as error.
const int kContinuousTimeoutLimit = 10;
// MJPEG is preferred if the requested width or height is larger than this.
const int kMjpegWidth = 640;
const int kMjpegHeight = 480;
// Typical framerate, in fps
const int kTypicalFramerate = 30;

// V4L2 color formats supported by V4L2CaptureDelegate derived classes.
// This list is ordered by precedence of use -- but see caveats for MJPEG.
static struct{
  uint32_t fourcc;
  VideoPixelFormat pixel_format;
  size_t num_planes;
} const kSupportedFormatsAndPlanarity[] = {
  {V4L2_PIX_FMT_YUV420, PIXEL_FORMAT_I420, 1},
  {V4L2_PIX_FMT_YUYV, PIXEL_FORMAT_YUY2, 1},
  {V4L2_PIX_FMT_UYVY, PIXEL_FORMAT_UYVY, 1},
  {V4L2_PIX_FMT_RGB24, PIXEL_FORMAT_RGB24, 1},
#if !defined(OS_OPENBSD) && defined(V4L_PIX_FMT_YUV420M)
  // TODO(mcasas): add V4L2_PIX_FMT_YVU420M when available in bots.
  {V4L2_PIX_FMT_YUV420M, PIXEL_FORMAT_I420, 3},
#endif
  // MJPEG is usually sitting fairly low since we don't want to have to decode.
  // However, is needed for large resolutions due to USB bandwidth limitations,
  // so GetListOfUsableFourCcs() can duplicate it on top, see that method.
  {V4L2_PIX_FMT_MJPEG, PIXEL_FORMAT_MJPEG, 1},
  // JPEG works as MJPEG on some gspca webcams from field reports, see
  // https://code.google.com/p/webrtc/issues/detail?id=529, put it as the least
  // preferred format.
  {V4L2_PIX_FMT_JPEG, PIXEL_FORMAT_MJPEG, 1},
};

// static
scoped_refptr<V4L2CaptureDelegate>
V4L2CaptureDelegate::CreateV4L2CaptureDelegate(
    const VideoCaptureDevice::Name& device_name,
    const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
    int power_line_frequency) {
  switch (device_name.capture_api_type()) {
    case VideoCaptureDevice::Name::V4L2_SINGLE_PLANE:
      return make_scoped_refptr(new V4L2CaptureDelegateSinglePlane(
          device_name, v4l2_task_runner, power_line_frequency));
    case VideoCaptureDevice::Name::V4L2_MULTI_PLANE:
#if !defined(OS_OPENBSD) && defined(V4L2_TYPE_IS_MULTIPLANAR)
      return make_scoped_refptr(new V4L2CaptureDelegateMultiPlane(
          device_name, v4l2_task_runner, power_line_frequency));
#endif
    default:
      NOTIMPLEMENTED() << "Unknown V4L2 capture API type";
      return scoped_refptr<V4L2CaptureDelegate>();
  }
}

// static
size_t V4L2CaptureDelegate::GetNumPlanesForFourCc(uint32_t fourcc) {
  for (const auto& fourcc_and_pixel_format : kSupportedFormatsAndPlanarity) {
    if (fourcc_and_pixel_format.fourcc == fourcc)
      return fourcc_and_pixel_format.num_planes;
  }
  DVLOG(1) << "Unknown fourcc " << FourccToString(fourcc);
  return 0;
}

// static
VideoPixelFormat V4L2CaptureDelegate::V4l2FourCcToChromiumPixelFormat(
    uint32_t v4l2_fourcc) {
  for (const auto& fourcc_and_pixel_format : kSupportedFormatsAndPlanarity) {
    if (fourcc_and_pixel_format.fourcc == v4l2_fourcc)
      return fourcc_and_pixel_format.pixel_format;
  }
  // Not finding a pixel format is OK during device capabilities enumeration.
  // Let the caller decide if PIXEL_FORMAT_UNKNOWN is an error or not.
  DVLOG(1) << "Unsupported pixel format: " << FourccToString(v4l2_fourcc);
  return PIXEL_FORMAT_UNKNOWN;
}

// static
std::list<uint32_t> V4L2CaptureDelegate::GetListOfUsableFourCcs(
    bool prefer_mjpeg) {
  std::list<uint32_t> supported_formats;
  for (const auto& format : kSupportedFormatsAndPlanarity)
    supported_formats.push_back(format.fourcc);

  // Duplicate MJPEG on top of the list depending on |prefer_mjpeg|.
  if (prefer_mjpeg)
    supported_formats.push_front(V4L2_PIX_FMT_MJPEG);

  return supported_formats;
}

// static
std::string V4L2CaptureDelegate::FourccToString(uint32_t fourcc) {
  return base::StringPrintf("%c%c%c%c", fourcc & 0xFF, (fourcc >> 8) & 0xFF,
                            (fourcc >> 16) & 0xFF, (fourcc >> 24) & 0xFF);
}

V4L2CaptureDelegate::BufferTracker::BufferTracker() {
}

V4L2CaptureDelegate::BufferTracker::~BufferTracker() {
  for (const auto& plane : planes_) {
    if (plane.start == nullptr)
      continue;
    const int result = munmap(plane.start, plane.length);
    PLOG_IF(ERROR, result < 0) << "Error munmap()ing V4L2 buffer";
  }
}

void V4L2CaptureDelegate::BufferTracker::AddMmapedPlane(uint8_t* const start,
                                                        size_t length) {
  Plane plane;
  plane.start = start;
  plane.length = length;
  plane.payload_size = 0;
  planes_.push_back(plane);
}

V4L2CaptureDelegate::V4L2CaptureDelegate(
    const VideoCaptureDevice::Name& device_name,
    const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
    int power_line_frequency)
#ifdef V4L2_TYPE_IS_MULTIPLANAR
    : capture_type_((device_name.capture_api_type() ==
                     VideoCaptureDevice::Name::V4L2_SINGLE_PLANE)
                        ? V4L2_BUF_TYPE_VIDEO_CAPTURE
                        : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE),
#else
    : capture_type_(V4L2_BUF_TYPE_VIDEO_CAPTURE),
#endif
      v4l2_task_runner_(v4l2_task_runner),
      device_name_(device_name),
      power_line_frequency_(power_line_frequency),
      is_capturing_(false),
      timeout_count_(0),
      rotation_(0) {
}

V4L2CaptureDelegate::~V4L2CaptureDelegate() {
}

void V4L2CaptureDelegate::AllocateAndStart(
    int width,
    int height,
    float frame_rate,
    scoped_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  DCHECK(client);
  client_ = client.Pass();

  // Need to open camera with O_RDWR after Linux kernel 3.3.
  device_fd_.reset(HANDLE_EINTR(open(device_name_.id().c_str(), O_RDWR)));
  if (!device_fd_.is_valid()) {
    SetErrorState("Failed to open V4L2 device driver file.");
    return;
  }

  v4l2_capability cap = {};
  if (!((HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QUERYCAP, &cap)) == 0) &&
#ifdef V4L2_TYPE_IS_MULTIPLANAR
        ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ||
          cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) &&
         !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) &&
         !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT_MPLANE)))) {
#else
        ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
         !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)))) {
#endif
    device_fd_.reset();
    SetErrorState("This is not a V4L2 video capture device");
    return;
  }

  // Get supported video formats in preferred order.
  // For large resolutions, favour mjpeg over raw formats.
  const std::list<uint32_t>& desired_v4l2_formats =
      GetListOfUsableFourCcs(width > kMjpegWidth || height > kMjpegHeight);
  std::list<uint32_t>::const_iterator best = desired_v4l2_formats.end();

  v4l2_fmtdesc fmtdesc = {};
  fmtdesc.type = capture_type_;
  for (; HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_ENUM_FMT, &fmtdesc)) == 0;
       ++fmtdesc.index) {
    best = std::find(desired_v4l2_formats.begin(), best, fmtdesc.pixelformat);
  }
  if (best == desired_v4l2_formats.end()) {
    SetErrorState("Failed to find a supported camera format.");
    return;
  }

  DVLOG(1) << "Chosen pixel format is " << FourccToString(*best);

  video_fmt_.type = capture_type_;
  if (!FillV4L2Format(&video_fmt_, width, height, *best)) {
    SetErrorState("Failed filling in V4L2 Format");
    return;
  }

  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_S_FMT, &video_fmt_)) < 0) {
    SetErrorState("Failed to set video capture format");
    return;
  }
  const VideoPixelFormat pixel_format =
      V4l2FourCcToChromiumPixelFormat(video_fmt_.fmt.pix.pixelformat);
  if (pixel_format == PIXEL_FORMAT_UNKNOWN) {
    SetErrorState("Unsupported pixel format");
    return;
  }

  // Set capture framerate in the form of capture interval.
  v4l2_streamparm streamparm = {};
  streamparm.type = capture_type_;
  // The following line checks that the driver knows about framerate get/set.
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_G_PARM, &streamparm)) >= 0) {
    // Now check if the device is able to accept a capture framerate set.
    if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
      // |frame_rate| is float, approximate by a fraction.
      streamparm.parm.capture.timeperframe.numerator =
          media::kFrameRatePrecision;
      streamparm.parm.capture.timeperframe.denominator =
          (frame_rate) ? (frame_rate * media::kFrameRatePrecision)
                       : (kTypicalFramerate * media::kFrameRatePrecision);

      if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_S_PARM, &streamparm)) <
          0) {
        SetErrorState("Failed to set camera framerate");
        return;
      }
      DVLOG(2) << "Actual camera driverframerate: "
               << streamparm.parm.capture.timeperframe.denominator << "/"
               << streamparm.parm.capture.timeperframe.numerator;
    }
  }
  // TODO(mcasas): what should be done if the camera driver does not allow
  // framerate configuration, or the actual one is different from the desired?

  // Set anti-banding/anti-flicker to 50/60Hz. May fail due to not supported
  // operation (|errno| == EINVAL in this case) or plain failure.
  if ((power_line_frequency_ == V4L2_CID_POWER_LINE_FREQUENCY_50HZ)
      || (power_line_frequency_ == V4L2_CID_POWER_LINE_FREQUENCY_60HZ)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
      || (power_line_frequency_ == V4L2_CID_POWER_LINE_FREQUENCY_AUTO)
#endif
     ) {
    struct v4l2_control control = {};
    control.id = V4L2_CID_POWER_LINE_FREQUENCY;
    control.value = power_line_frequency_;
    const int retval =
        HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_S_CTRL, &control));
    if (retval != 0)
      DVLOG(1) << "Error setting power line frequency removal";
  }

  capture_format_.frame_size.SetSize(video_fmt_.fmt.pix.width,
                                     video_fmt_.fmt.pix.height);
  capture_format_.frame_rate = frame_rate;
  capture_format_.pixel_format = pixel_format;

  v4l2_requestbuffers r_buffer = {};
  r_buffer.type = capture_type_;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = kNumVideoBuffers;
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_REQBUFS, &r_buffer)) < 0) {
    SetErrorState("Error requesting MMAP buffers from V4L2");
    return;
  }
  for (unsigned int i = 0; i < r_buffer.count; ++i) {
    if (!MapAndQueueBuffer(i)) {
      SetErrorState("Allocate buffer failed");
      return;
    }
  }

  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_STREAMON, &capture_type_))
      < 0) {
    SetErrorState("VIDIOC_STREAMON failed");
    return;
  }

  is_capturing_ = true;
  // Post task to start fetching frames from v4l2.
  v4l2_task_runner_->PostTask(
      FROM_HERE, base::Bind(&V4L2CaptureDelegate::DoCapture, this));
}

void V4L2CaptureDelegate::StopAndDeAllocate() {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  // The order is important: stop streaming, clear |buffer_pool_|,
  // thus munmap()ing the v4l2_buffers, and then return them to the OS.
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_STREAMOFF, &capture_type_))
      < 0) {
    SetErrorState("VIDIOC_STREAMOFF failed");
    return;
  }

  buffer_tracker_pool_.clear();

  v4l2_requestbuffers r_buffer = {};
  r_buffer.type = capture_type_;
  r_buffer.memory = V4L2_MEMORY_MMAP;
  r_buffer.count = 0;
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_REQBUFS, &r_buffer)) < 0)
    SetErrorState("Failed to VIDIOC_REQBUFS with count = 0");

  // At this point we can close the device.
  // This is also needed for correctly changing settings later via VIDIOC_S_FMT.
  device_fd_.reset();
  is_capturing_ = false;
  client_.reset();
}

void V4L2CaptureDelegate::SetRotation(int rotation) {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  DCHECK(rotation >= 0 && rotation < 360 && rotation % 90 == 0);
  rotation_ = rotation;
}

bool V4L2CaptureDelegate::MapAndQueueBuffer(int index) {
  v4l2_buffer buffer;
  FillV4L2Buffer(&buffer, index);

  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QUERYBUF, &buffer)) < 0) {
    DLOG(ERROR) << "Error querying status of a MMAP V4L2 buffer";
    return false;
  }

  const scoped_refptr<BufferTracker>& buffer_tracker = CreateBufferTracker();
  if (!buffer_tracker->Init(device_fd_.get(), buffer)) {
    DLOG(ERROR) << "Error creating BufferTracker";
    return false;
  }
  buffer_tracker_pool_.push_back(buffer_tracker);

  // Enqueue the buffer in the drivers incoming queue.
  if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QBUF, &buffer)) < 0) {
    DLOG(ERROR) << "Error enqueuing a V4L2 buffer back into the driver";
    return false;
  }
  return true;
}

void V4L2CaptureDelegate::FillV4L2Buffer(v4l2_buffer* buffer,
                                         int i) const {
  memset(buffer, 0, sizeof(*buffer));
  buffer->memory = V4L2_MEMORY_MMAP;
  buffer->index = i;
  FinishFillingV4L2Buffer(buffer);
}

void V4L2CaptureDelegate::DoCapture() {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  if (!is_capturing_)
    return;

  pollfd device_pfd = {};
  device_pfd.fd = device_fd_.get();
  device_pfd.events = POLLIN;
  const int result = HANDLE_EINTR(poll(&device_pfd, 1, kCaptureTimeoutMs));
  if (result < 0) {
    SetErrorState("Poll failed");
    return;
  }
  // Check if poll() timed out; track the amount of times it did in a row and
  // throw an error if it times out too many times.
  if (result == 0) {
    timeout_count_++;
    if (timeout_count_ >= kContinuousTimeoutLimit) {
      SetErrorState("Multiple continuous timeouts while read-polling.");
      timeout_count_ = 0;
      return;
    }
  } else {
    timeout_count_ = 0;
  }

  // Deenqueue, send and reenqueue a buffer if the driver has filled one in.
  if (device_pfd.revents & POLLIN) {
    v4l2_buffer buffer;
    FillV4L2Buffer(&buffer, 0);

    if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_DQBUF, &buffer)) < 0) {
      SetErrorState("Failed to dequeue capture buffer");
      return;
    }

    SetPayloadSize(buffer_tracker_pool_[buffer.index], buffer);
    SendBuffer(buffer_tracker_pool_[buffer.index], video_fmt_);

    if (HANDLE_EINTR(ioctl(device_fd_.get(), VIDIOC_QBUF, &buffer)) < 0) {
      SetErrorState("Failed to enqueue capture buffer");
      return;
    }
  }

  v4l2_task_runner_->PostTask(
      FROM_HERE, base::Bind(&V4L2CaptureDelegate::DoCapture, this));
}

void V4L2CaptureDelegate::SetErrorState(const std::string& reason) {
  DCHECK(v4l2_task_runner_->BelongsToCurrentThread());
  is_capturing_ = false;
  client_->OnError(reason);
}

}  // namespace media
