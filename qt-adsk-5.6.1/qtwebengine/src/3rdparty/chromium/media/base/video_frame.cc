// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/string_piece.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "ui/gfx/geometry/point.h"

namespace media {

static bool IsPowerOfTwo(size_t x) {
  return x != 0 && (x & (x - 1)) == 0;
}

static inline size_t RoundUp(size_t value, size_t alignment) {
  DCHECK(IsPowerOfTwo(alignment));
  return ((value + (alignment - 1)) & ~(alignment - 1));
}

static inline size_t RoundDown(size_t value, size_t alignment) {
  DCHECK(IsPowerOfTwo(alignment));
  return value & ~(alignment - 1);
}

// Returns true if |plane| is a valid plane index for the given |format|.
static bool IsValidPlane(size_t plane, VideoFrame::Format format) {
  DCHECK_LE(VideoFrame::NumPlanes(format),
            static_cast<size_t>(VideoFrame::kMaxPlanes));
  return (plane < VideoFrame::NumPlanes(format));
}

// Returns true if |frame| is accesible mapped in the VideoFrame memory space.
// static
static bool IsStorageTypeMappable(VideoFrame::StorageType storage_type) {
  return
#if defined(OS_LINUX)
      // This is not strictly needed but makes explicit that, at VideoFrame
      // level, DmaBufs are not mappable from userspace.
      storage_type != VideoFrame::STORAGE_DMABUFS &&
#endif
      (storage_type == VideoFrame::STORAGE_UNOWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_OWNED_MEMORY ||
       storage_type == VideoFrame::STORAGE_SHMEM);
}

// Returns the pixel size per element for given |plane| and |format|. E.g. 2x2
// for the U-plane in I420.
static gfx::Size SampleSize(VideoFrame::Format format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));

  switch (plane) {
    case VideoFrame::kYPlane:
    case VideoFrame::kAPlane:
      return gfx::Size(1, 1);

    case VideoFrame::kUPlane:  // and VideoFrame::kUVPlane:
    case VideoFrame::kVPlane:
      switch (format) {
        case VideoFrame::YV24:
          return gfx::Size(1, 1);

        case VideoFrame::YV16:
          return gfx::Size(2, 1);

        case VideoFrame::YV12:
        case VideoFrame::I420:
        case VideoFrame::YV12A:
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
        case VideoFrame::NV12:
#endif
          return gfx::Size(2, 2);

        case VideoFrame::UNKNOWN:
        case VideoFrame::ARGB:
        case VideoFrame::XRGB:
          break;
      }
  }
  NOTREACHED();
  return gfx::Size();
}

// Return the alignment for the whole frame, calculated as the max of the
// alignment for each individual plane.
static gfx::Size CommonAlignment(VideoFrame::Format format) {
  int max_sample_width = 0;
  int max_sample_height = 0;
  for (size_t plane = 0; plane < VideoFrame::NumPlanes(format); ++plane) {
    const gfx::Size sample_size = SampleSize(format, plane);
    max_sample_width = std::max(max_sample_width, sample_size.width());
    max_sample_height = std::max(max_sample_height, sample_size.height());
  }
  return gfx::Size(max_sample_width, max_sample_height);
}

// Returns the number of bytes per element for given |plane| and |format|.
static int BytesPerElement(VideoFrame::Format format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  if (format == VideoFrame::ARGB || format == VideoFrame::XRGB)
    return 4;

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  if (format == VideoFrame::NV12 && plane == VideoFrame::kUVPlane)
    return 2;
#endif

  return 1;
}

// Rounds up |coded_size| if necessary for |format|.
static gfx::Size AdjustCodedSize(VideoFrame::Format format,
                                 const gfx::Size& coded_size) {
  const gfx::Size alignment = CommonAlignment(format);
  return gfx::Size(RoundUp(coded_size.width(), alignment.width()),
                   RoundUp(coded_size.height(), alignment.height()));
}

// static
std::string VideoFrame::FormatToString(Format format) {
  switch (format) {
    case UNKNOWN:
      return "UNKNOWN";
    case YV12:
      return "YV12";
    case YV16:
      return "YV16";
    case I420:
      return "I420";
    case YV12A:
      return "YV12A";
    case YV24:
      return "YV24";
    case ARGB:
      return "ARGB";
    case XRGB:
      return "XRGB";
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
    case NV12:
      return "NV12";
#endif
  }
  NOTREACHED() << "Invalid VideoFrame format provided: " << format;
  return "";
}

// static
bool VideoFrame::IsYuvPlanar(Format format) {
  switch (format) {
    case YV12:
    case I420:
    case YV16:
    case YV12A:
    case YV24:
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
    case NV12:
#endif
      return true;

    case UNKNOWN:
    case ARGB:
    case XRGB:
      return false;
  }
  return false;
}

// static
bool VideoFrame::IsValidConfig(Format format,
                               StorageType storage_type,
                               const gfx::Size& coded_size,
                               const gfx::Rect& visible_rect,
                               const gfx::Size& natural_size) {
  // Check maximum limits for all formats.
  if (coded_size.GetArea() > limits::kMaxCanvas ||
      coded_size.width() > limits::kMaxDimension ||
      coded_size.height() > limits::kMaxDimension ||
      visible_rect.x() < 0 || visible_rect.y() < 0 ||
      visible_rect.right() > coded_size.width() ||
      visible_rect.bottom() > coded_size.height() ||
      natural_size.GetArea() > limits::kMaxCanvas ||
      natural_size.width() > limits::kMaxDimension ||
      natural_size.height() > limits::kMaxDimension)
    return false;

  // TODO(mcasas): Remove parameter |storage_type| when the opaque storage types
  // comply with the checks below. Right now we skip them.
  if (!IsStorageTypeMappable(storage_type))
    return true;

  // Check format-specific width/height requirements.
  switch (format) {
    case UNKNOWN:
      return (coded_size.IsEmpty() && visible_rect.IsEmpty() &&
              natural_size.IsEmpty());
    case YV24:
    case YV12:
    case I420:
    case YV12A:
    case YV16:
    case ARGB:
    case XRGB:
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
    case NV12:
#endif
      // Check that software-allocated buffer formats are aligned correctly and
      // not empty.
      const gfx::Size alignment = CommonAlignment(format);
      return RoundUp(visible_rect.right(), alignment.width()) <=
                 static_cast<size_t>(coded_size.width()) &&
             RoundUp(visible_rect.bottom(), alignment.height()) <=
                 static_cast<size_t>(coded_size.height()) &&
             !coded_size.IsEmpty() && !visible_rect.IsEmpty() &&
             !natural_size.IsEmpty();
  }

  // TODO(mcasas): Check that storage type and underlying mailboxes/dataptr are
  // matching.
  NOTREACHED();
  return false;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrame(Format format,
                                                  const gfx::Size& coded_size,
                                                  const gfx::Rect& visible_rect,
                                                  const gfx::Size& natural_size,
                                                  base::TimeDelta timestamp) {
  if (!IsYuvPlanar(format)) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  // Since we're creating a new YUV frame (and allocating memory for it
  // ourselves), we can pad the requested |coded_size| if necessary if the
  // request does not line up on sample boundaries.
  const gfx::Size new_coded_size = AdjustCodedSize(format, coded_size);
  DCHECK(IsValidConfig(format, STORAGE_OWNED_MEMORY, new_coded_size,
                       visible_rect, natural_size));

  scoped_refptr<VideoFrame> frame(new VideoFrame(format, STORAGE_OWNED_MEMORY,
                                                 new_coded_size, visible_rect,
                                                 natural_size, timestamp));
  frame->AllocateYUV();
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapNativeTexture(
    Format format,
    const gpu::MailboxHolder& mailbox_holder,
    const ReleaseMailboxCB& mailbox_holder_release_cb,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  if (format != ARGB) {
    DLOG(ERROR) << "Only ARGB pixel format supported, got "
                << FormatToString(format);
    return nullptr;
  }
  gpu::MailboxHolder mailbox_holders[kMaxPlanes];
  mailbox_holders[kARGBPlane] = mailbox_holder;
  return new VideoFrame(format, STORAGE_OPAQUE, coded_size,
                        visible_rect, natural_size, mailbox_holders,
                        mailbox_holder_release_cb, timestamp);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapYUV420NativeTextures(
    const gpu::MailboxHolder& y_mailbox_holder,
    const gpu::MailboxHolder& u_mailbox_holder,
    const gpu::MailboxHolder& v_mailbox_holder,
    const ReleaseMailboxCB& mailbox_holder_release_cb,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  gpu::MailboxHolder mailbox_holders[kMaxPlanes];
  mailbox_holders[kYPlane] = y_mailbox_holder;
  mailbox_holders[kUPlane] = u_mailbox_holder;
  mailbox_holders[kVPlane] = v_mailbox_holder;
  return new VideoFrame(I420, STORAGE_OPAQUE, coded_size, visible_rect,
                        natural_size, mailbox_holders,
                        mailbox_holder_release_cb, timestamp);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalData(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8* data,
    size_t data_size,
    base::TimeDelta timestamp) {
  return WrapExternalStorage(format, STORAGE_UNOWNED_MEMORY, coded_size,
                             visible_rect, natural_size, data, data_size,
                             timestamp, base::SharedMemory::NULLHandle(), 0);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalSharedMemory(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8* data,
    size_t data_size,
    base::SharedMemoryHandle handle,
    size_t data_offset,
    base::TimeDelta timestamp) {
  return WrapExternalStorage(format, STORAGE_SHMEM, coded_size, visible_rect,
                             natural_size, data, data_size, timestamp, handle,
                             data_offset);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvData(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    int32 y_stride,
    int32 u_stride,
    int32 v_stride,
    uint8* y_data,
    uint8* u_data,
    uint8* v_data,
    base::TimeDelta timestamp) {
  const gfx::Size new_coded_size = AdjustCodedSize(format, coded_size);
  CHECK(IsValidConfig(format, STORAGE_UNOWNED_MEMORY, new_coded_size,
                      visible_rect, natural_size));

  scoped_refptr<VideoFrame> frame(new VideoFrame(format, STORAGE_UNOWNED_MEMORY,
                                                 new_coded_size, visible_rect,
                                                 natural_size, timestamp));
  frame->strides_[kYPlane] = y_stride;
  frame->strides_[kUPlane] = u_stride;
  frame->strides_[kVPlane] = v_stride;
  frame->data_[kYPlane] = y_data;
  frame->data_[kUPlane] = u_data;
  frame->data_[kVPlane] = v_data;
  return frame;
}

#if defined(OS_LINUX)
// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalDmabufs(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    const std::vector<int>& dmabuf_fds,
    base::TimeDelta timestamp) {
#if defined(OS_CHROMEOS)
  DCHECK_EQ(format, NV12);
#endif

  if (!IsValidConfig(format, STORAGE_DMABUFS, coded_size, visible_rect,
                     natural_size)) {
    return nullptr;
  }
  gpu::MailboxHolder mailbox_holders[kMaxPlanes];
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(format, STORAGE_DMABUFS, coded_size, visible_rect,
                     natural_size, mailbox_holders, ReleaseMailboxCB(),
                     timestamp);
  if (!frame || !frame->DuplicateFileDescriptors(dmabuf_fds))
    return nullptr;
  return frame;
}
#endif

#if defined(OS_MACOSX)
// static
scoped_refptr<VideoFrame> VideoFrame::WrapCVPixelBuffer(
    CVPixelBufferRef cv_pixel_buffer,
    base::TimeDelta timestamp) {
  DCHECK(cv_pixel_buffer);
  DCHECK(CFGetTypeID(cv_pixel_buffer) == CVPixelBufferGetTypeID());

  const OSType cv_format = CVPixelBufferGetPixelFormatType(cv_pixel_buffer);
  Format format;
  // There are very few compatible CV pixel formats, so just check each.
  if (cv_format == kCVPixelFormatType_420YpCbCr8Planar) {
    format = I420;
  } else if (cv_format == kCVPixelFormatType_444YpCbCr8) {
    format = YV24;
  } else if (cv_format == '420v') {
    // TODO(jfroy): Use kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange when the
    // minimum OS X and iOS SDKs permits it.
    format = NV12;
  } else {
    DLOG(ERROR) << "CVPixelBuffer format not supported: " << cv_format;
    return NULL;
  }

  const gfx::Size coded_size(CVImageBufferGetEncodedSize(cv_pixel_buffer));
  const gfx::Rect visible_rect(CVImageBufferGetCleanRect(cv_pixel_buffer));
  const gfx::Size natural_size(CVImageBufferGetDisplaySize(cv_pixel_buffer));

  if (!IsValidConfig(format, STORAGE_UNOWNED_MEMORY,
                     coded_size, visible_rect, natural_size)) {
    return NULL;
  }

  scoped_refptr<VideoFrame> frame(new VideoFrame(format, STORAGE_UNOWNED_MEMORY,
                                                 coded_size, visible_rect,
                                                 natural_size, timestamp));

  frame->cv_pixel_buffer_.reset(cv_pixel_buffer, base::scoped_policy::RETAIN);
  return frame;
}
#endif

// static
scoped_refptr<VideoFrame> VideoFrame::WrapVideoFrame(
      const scoped_refptr<VideoFrame>& frame,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size) {
  // Frames with textures need mailbox info propagated, and there's no support
  // for that here yet, see http://crbug/362521.
  CHECK(!frame->HasTextures());

  DCHECK(frame->visible_rect().Contains(visible_rect));
  scoped_refptr<VideoFrame> wrapping_frame(new VideoFrame(
      frame->format(), frame->storage_type(), frame->coded_size(), visible_rect,
      natural_size, frame->timestamp()));
  if (frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM)) {
    wrapping_frame->metadata()->SetBoolean(VideoFrameMetadata::END_OF_STREAM,
                                          true);
  }

  for (size_t i = 0; i < NumPlanes(frame->format()); ++i) {
    wrapping_frame->strides_[i] = frame->stride(i);
    wrapping_frame->data_[i] = frame->data(i);
  }

#if defined(OS_LINUX)
  // If there are any |dmabuf_fds_| plugged in, we should duplicate them.
  if (frame->storage_type() == STORAGE_DMABUFS) {
    std::vector<int> original_fds;
    for (size_t i = 0; i < kMaxPlanes; ++i)
      original_fds.push_back(frame->dmabuf_fd(i));
    if (!wrapping_frame->DuplicateFileDescriptors(original_fds))
      return nullptr;
  }
#endif

  return wrapping_frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEOSFrame() {
  scoped_refptr<VideoFrame> frame =
      new VideoFrame(UNKNOWN, STORAGE_UNKNOWN, gfx::Size(),
                     gfx::Rect(), gfx::Size(), kNoTimestamp());
  frame->metadata()->SetBoolean(VideoFrameMetadata::END_OF_STREAM, true);
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateColorFrame(
    const gfx::Size& size,
    uint8 y, uint8 u, uint8 v,
    base::TimeDelta timestamp) {
  scoped_refptr<VideoFrame> frame =
      CreateFrame(YV12, size, gfx::Rect(size), size, timestamp);
  FillYUV(frame.get(), y, u, v);
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateBlackFrame(const gfx::Size& size) {
  const uint8 kBlackY = 0x00;
  const uint8 kBlackUV = 0x80;
  const base::TimeDelta kZero;
  return CreateColorFrame(size, kBlackY, kBlackUV, kBlackUV, kZero);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateTransparentFrame(
    const gfx::Size& size) {
  const uint8 kBlackY = 0x00;
  const uint8 kBlackUV = 0x00;
  const uint8 kTransparentA = 0x00;
  const base::TimeDelta kZero;
  scoped_refptr<VideoFrame> frame =
      CreateFrame(YV12A, size, gfx::Rect(size), size, kZero);
  FillYUVA(frame.get(), kBlackY, kBlackUV, kBlackUV, kTransparentA);
  return frame;
}

#if defined(VIDEO_HOLE)
// This block and other blocks wrapped around #if defined(VIDEO_HOLE) is not
// maintained by the general compositor team. Please contact
// wonsik@chromium.org .

// static
scoped_refptr<VideoFrame> VideoFrame::CreateHoleFrame(
    const gfx::Size& size) {
  DCHECK(IsValidConfig(UNKNOWN, STORAGE_HOLE, size, gfx::Rect(size), size));
  scoped_refptr<VideoFrame> frame(new VideoFrame(
      UNKNOWN, STORAGE_HOLE, size, gfx::Rect(size), size, base::TimeDelta()));
  return frame;
}
#endif  // defined(VIDEO_HOLE)

// static
size_t VideoFrame::NumPlanes(Format format) {
  switch (format) {
    case ARGB:
    case XRGB:
      return 1;
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
    case NV12:
      return 2;
#endif
    case YV12:
    case YV16:
    case I420:
    case YV24:
      return 3;
    case YV12A:
      return 4;
    case UNKNOWN:
      break;
  }
  NOTREACHED() << "Unsupported video frame format: " << format;
  return 0;
}

// static
size_t VideoFrame::AllocationSize(Format format, const gfx::Size& coded_size) {
  size_t total = 0;
  for (size_t i = 0; i < NumPlanes(format); ++i)
    total += PlaneSize(format, i, coded_size).GetArea();
  return total;
}

// static
gfx::Size VideoFrame::PlaneSize(Format format,
                                size_t plane,
                                const gfx::Size& coded_size) {
  DCHECK(IsValidPlane(plane, format));

  int width = coded_size.width();
  int height = coded_size.height();
  if (format != ARGB) {
    // Align to multiple-of-two size overall. This ensures that non-subsampled
    // planes can be addressed by pixel with the same scaling as the subsampled
    // planes.
    width = RoundUp(width, 2);
    height = RoundUp(height, 2);
  }

  const gfx::Size subsample = SampleSize(format, plane);
  DCHECK(width % subsample.width() == 0);
  DCHECK(height % subsample.height() == 0);
  return gfx::Size(BytesPerElement(format, plane) * width / subsample.width(),
                   height / subsample.height());
}

// static
int VideoFrame::PlaneHorizontalBitsPerPixel(Format format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  const int bits_per_element = 8 * BytesPerElement(format, plane);
  const int horiz_pixels_per_element = SampleSize(format, plane).width();
  DCHECK_EQ(bits_per_element % horiz_pixels_per_element, 0);
  return bits_per_element / horiz_pixels_per_element;
}

// static
int VideoFrame::PlaneBitsPerPixel(Format format, size_t plane) {
  DCHECK(IsValidPlane(plane, format));
  return PlaneHorizontalBitsPerPixel(format, plane) /
      SampleSize(format, plane).height();
}

// static
size_t VideoFrame::RowBytes(size_t plane, Format format, int width) {
  DCHECK(IsValidPlane(plane, format));
  return BytesPerElement(format, plane) * Columns(plane, format, width);
}

// static
size_t VideoFrame::Rows(size_t plane, Format format, int height) {
  DCHECK(IsValidPlane(plane, format));
  const int sample_height = SampleSize(format, plane).height();
  return RoundUp(height, sample_height) / sample_height;
}

// static
size_t VideoFrame::Columns(size_t plane, Format format, int width) {
  DCHECK(IsValidPlane(plane, format));
  const int sample_width = SampleSize(format, plane).width();
  return RoundUp(width, sample_width) / sample_width;
}

// static
void VideoFrame::HashFrameForTesting(base::MD5Context* context,
                                     const scoped_refptr<VideoFrame>& frame) {
  DCHECK(context);
  for (size_t plane = 0; plane < NumPlanes(frame->format()); ++plane) {
    for (int row = 0; row < frame->rows(plane); ++row) {
      base::MD5Update(
          context,
          base::StringPiece(reinterpret_cast<char*>(frame->data(plane) +
                                                    frame->stride(plane) * row),
                            frame->row_bytes(plane)));
    }
  }
}

bool VideoFrame::IsMappable() const {
  return IsStorageTypeMappable(storage_type_);
}

bool VideoFrame::HasTextures() const {
  return !mailbox_holders_[0].mailbox.IsZero();
}

int VideoFrame::stride(size_t plane) const {
  DCHECK(IsValidPlane(plane, format_));
  return strides_[plane];
}

int VideoFrame::row_bytes(size_t plane) const {
  return RowBytes(plane, format_, coded_size_.width());
}

int VideoFrame::rows(size_t plane) const {
  return Rows(plane, format_, coded_size_.height());
}

const uint8* VideoFrame::data(size_t plane) const {
  DCHECK(IsValidPlane(plane, format_));
  DCHECK(IsMappable());
  return data_[plane];
}

uint8* VideoFrame::data(size_t plane) {
  DCHECK(IsValidPlane(plane, format_));
  DCHECK(IsMappable());
  return data_[plane];
}

const uint8* VideoFrame::visible_data(size_t plane) const {
  DCHECK(IsValidPlane(plane, format_));
  DCHECK(IsMappable());

  // Calculate an offset that is properly aligned for all planes.
  const gfx::Size alignment = CommonAlignment(format_);
  const gfx::Point offset(RoundDown(visible_rect_.x(), alignment.width()),
                          RoundDown(visible_rect_.y(), alignment.height()));

  const gfx::Size subsample = SampleSize(format_, plane);
  DCHECK(offset.x() % subsample.width() == 0);
  DCHECK(offset.y() % subsample.height() == 0);
  return data(plane) +
         stride(plane) * (offset.y() / subsample.height()) +  // Row offset.
         BytesPerElement(format_, plane) *                    // Column offset.
             (offset.x() / subsample.width());
}

uint8* VideoFrame::visible_data(size_t plane) {
  return const_cast<uint8*>(
      static_cast<const VideoFrame*>(this)->visible_data(plane));
}

const gpu::MailboxHolder&
VideoFrame::mailbox_holder(size_t texture_index) const {
  DCHECK(HasTextures());
  DCHECK(IsValidPlane(texture_index, format_));
  return mailbox_holders_[texture_index];
}

base::SharedMemoryHandle VideoFrame::shared_memory_handle() const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(shared_memory_handle_ != base::SharedMemory::NULLHandle());
  return shared_memory_handle_;
}

size_t VideoFrame::shared_memory_offset() const {
  DCHECK_EQ(storage_type_, STORAGE_SHMEM);
  DCHECK(shared_memory_handle_ != base::SharedMemory::NULLHandle());
  return shared_memory_offset_;
}

#if defined(OS_LINUX)
int VideoFrame::dmabuf_fd(size_t plane) const {
  DCHECK_EQ(storage_type_, STORAGE_DMABUFS);
  DCHECK(IsValidPlane(plane, format_));
  return dmabuf_fds_[plane].get();
}

bool VideoFrame::DuplicateFileDescriptors(const std::vector<int>& in_fds) {
  // TODO(mcasas): Support offsets for e.g. multiplanar inside a single |in_fd|.

  storage_type_ = STORAGE_DMABUFS;
  // TODO(posciak): This is not exactly correct, it's possible for one
  // buffer to contain more than one plane.
  if (in_fds.size() != NumPlanes(format_)) {
    LOG(FATAL) << "Not enough dmabuf fds provided, got: " <<  in_fds.size()
               << ", expected: " << NumPlanes(format_);
    return false;
  }

  // Make sure that all fds are closed if any dup() fails,
  base::ScopedFD temp_dmabuf_fds[kMaxPlanes];
  for (size_t i = 0; i < in_fds.size(); ++i) {
    temp_dmabuf_fds[i] = base::ScopedFD(HANDLE_EINTR(dup(in_fds[i])));
    if (!temp_dmabuf_fds[i].is_valid()) {
      DPLOG(ERROR) << "Failed duplicating a dmabuf fd";
      return false;
    }
  }
  for (size_t i = 0; i < kMaxPlanes; ++i)
    dmabuf_fds_[i].reset(temp_dmabuf_fds[i].release());

  return true;
}
#endif

void VideoFrame::AddSharedMemoryHandle(base::SharedMemoryHandle handle) {
  storage_type_ = STORAGE_SHMEM;
  shared_memory_handle_ = handle;
}

#if defined(OS_MACOSX)
CVPixelBufferRef VideoFrame::cv_pixel_buffer() const {
  return cv_pixel_buffer_.get();
}
#endif

void VideoFrame::AddDestructionObserver(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  done_callbacks_.push_back(callback);
}

void VideoFrame::UpdateReleaseSyncPoint(SyncPointClient* client) {
  DCHECK(HasTextures());
  base::AutoLock locker(release_sync_point_lock_);
  // Must wait on the previous sync point before inserting a new sync point so
  // that |mailbox_holders_release_cb_| guarantees the previous sync point
  // occurred when it waits on |release_sync_point_|.
  if (release_sync_point_)
    client->WaitSyncPoint(release_sync_point_);
  release_sync_point_ = client->InsertSyncPoint();
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalStorage(
    Format format,
    StorageType storage_type,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8* data,
    size_t data_size,
    base::TimeDelta timestamp,
    base::SharedMemoryHandle handle,
    size_t data_offset) {
  DCHECK(IsStorageTypeMappable(storage_type));

  const gfx::Size new_coded_size = AdjustCodedSize(format, coded_size);
  if (!IsValidConfig(format, storage_type, new_coded_size, visible_rect,
                     natural_size) ||
      data_size < AllocationSize(format, new_coded_size)) {
    return NULL;
  }
  DLOG_IF(ERROR, format != I420) << "Only I420 format supported: "
                                 << FormatToString(format);
  if (format != I420)
    return NULL;

  scoped_refptr<VideoFrame> frame;
  if (storage_type == STORAGE_SHMEM) {
    frame = new VideoFrame(format, storage_type, new_coded_size,
                           visible_rect, natural_size, timestamp, handle,
                           data_offset);
  } else {
    frame = new VideoFrame(format, storage_type, new_coded_size,
                           visible_rect, natural_size, timestamp);
  }
  frame->strides_[kYPlane] = new_coded_size.width();
  frame->strides_[kUPlane] = new_coded_size.width() / 2;
  frame->strides_[kVPlane] = new_coded_size.width() / 2;
  frame->data_[kYPlane] = data;
  frame->data_[kUPlane] = data + new_coded_size.GetArea();
  frame->data_[kVPlane] = data + (new_coded_size.GetArea() * 5 / 4);
  return frame;
}

VideoFrame::VideoFrame(Format format,
                       StorageType storage_type,
                       const gfx::Size& coded_size,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       base::TimeDelta timestamp)
    : format_(format),
      storage_type_(storage_type),
      coded_size_(coded_size),
      visible_rect_(visible_rect),
      natural_size_(natural_size),
      shared_memory_handle_(base::SharedMemory::NULLHandle()),
      shared_memory_offset_(0),
      timestamp_(timestamp),
      release_sync_point_(0) {
  DCHECK(IsValidConfig(format_, storage_type, coded_size_, visible_rect_,
                       natural_size_));
  memset(&mailbox_holders_, 0, sizeof(mailbox_holders_));
  memset(&strides_, 0, sizeof(strides_));
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::VideoFrame(Format format,
                       StorageType storage_type,
                       const gfx::Size& coded_size,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       base::TimeDelta timestamp,
                       base::SharedMemoryHandle handle,
                       size_t shared_memory_offset)
    : VideoFrame(format,
                 storage_type,
                 coded_size,
                 visible_rect,
                 natural_size,
                 timestamp) {
  DCHECK_EQ(storage_type, STORAGE_SHMEM);
  AddSharedMemoryHandle(handle);
  shared_memory_offset_ = shared_memory_offset;
}

VideoFrame::VideoFrame(Format format,
                       StorageType storage_type,
                       const gfx::Size& coded_size,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       const gpu::MailboxHolder(&mailbox_holders)[kMaxPlanes],
                       const ReleaseMailboxCB& mailbox_holder_release_cb,
                       base::TimeDelta timestamp)
    : VideoFrame(format,
                 storage_type,
                 coded_size,
                 visible_rect,
                 natural_size,
                 timestamp) {
  memcpy(&mailbox_holders_, mailbox_holders, sizeof(mailbox_holders_));
  mailbox_holders_release_cb_ = mailbox_holder_release_cb;
}

VideoFrame::~VideoFrame() {
  if (!mailbox_holders_release_cb_.is_null()) {
    uint32 release_sync_point;
    {
      // To ensure that changes to |release_sync_point_| are visible on this
      // thread (imply a memory barrier).
      base::AutoLock locker(release_sync_point_lock_);
      release_sync_point = release_sync_point_;
    }
    base::ResetAndReturn(&mailbox_holders_release_cb_).Run(release_sync_point);
  }

  for (auto& callback : done_callbacks_)
    base::ResetAndReturn(&callback).Run();
}

void VideoFrame::AllocateYUV() {
  DCHECK_EQ(storage_type_, STORAGE_OWNED_MEMORY);
  static_assert(0 == kYPlane, "y plane data must be index 0");

  size_t data_size = 0;
  size_t offset[kMaxPlanes];
  for (size_t plane = 0; plane < NumPlanes(format_); ++plane) {
    // The *2 in alignment for height is because some formats (e.g. h264) allow
    // interlaced coding, and then the size needs to be a multiple of two
    // macroblocks (vertically). See
    // libavcodec/utils.c:avcodec_align_dimensions2().
    const size_t height = RoundUp(rows(plane), kFrameSizeAlignment * 2);
    strides_[plane] = RoundUp(row_bytes(plane), kFrameSizeAlignment);
    offset[plane] = data_size;
    data_size += height * strides_[plane];
  }

  // The extra line of UV being allocated is because h264 chroma MC
  // overreads by one line in some cases, see libavcodec/utils.c:
  // avcodec_align_dimensions2() and libavcodec/x86/h264_chromamc.asm:
  // put_h264_chroma_mc4_ssse3().
  DCHECK(IsValidPlane(kUPlane, format_));
  data_size += strides_[kUPlane] + kFrameSizePadding;

  // FFmpeg expects the initialize allocation to be zero-initialized.  Failure
  // to do so can lead to unitialized value usage.  See http://crbug.com/390941
  uint8* data = reinterpret_cast<uint8*>(
      base::AlignedAlloc(data_size, kFrameAddressAlignment));
  memset(data, 0, data_size);

  for (size_t plane = 0; plane < NumPlanes(format_); ++plane)
    data_[plane] = data + offset[plane];

  AddDestructionObserver(base::Bind(&base::AlignedFree, data));
}

}  // namespace media
