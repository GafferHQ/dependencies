// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libdrm/drm_fourcc.h>
#include <linux/videodev2.h>

#include "base/numerics/safe_conversions.h"
#include "content/common/gpu/media/generic_v4l2_device.h"
#if defined(ARCH_CPU_ARMEL)
#include "content/common/gpu/media/tegra_v4l2_device.h"
#endif

namespace content {

V4L2Device::V4L2Device(Type type) : type_(type) {
}

V4L2Device::~V4L2Device() {
}

// static
scoped_refptr<V4L2Device> V4L2Device::Create(Type type) {
  DVLOG(3) << __PRETTY_FUNCTION__;

  scoped_refptr<GenericV4L2Device> generic_device(new GenericV4L2Device(type));
  if (generic_device->Initialize())
    return generic_device;

#if defined(ARCH_CPU_ARMEL)
  scoped_refptr<TegraV4L2Device> tegra_device(new TegraV4L2Device(type));
  if (tegra_device->Initialize())
    return tegra_device;
#endif

  DVLOG(1) << "Failed to create V4L2Device";
  return scoped_refptr<V4L2Device>();
}

// static
media::VideoFrame::Format V4L2Device::V4L2PixFmtToVideoFrameFormat(
    uint32 pix_fmt) {
  switch (pix_fmt) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      return media::VideoFrame::NV12;

    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      return media::VideoFrame::I420;

    case V4L2_PIX_FMT_RGB32:
      return media::VideoFrame::ARGB;

    default:
      LOG(FATAL) << "Add more cases as needed";
      return media::VideoFrame::UNKNOWN;
  }
}

// static
uint32 V4L2Device::VideoFrameFormatToV4L2PixFmt(
    media::VideoFrame::Format format) {
  switch (format) {
    case media::VideoFrame::NV12:
      return V4L2_PIX_FMT_NV12M;

    case media::VideoFrame::I420:
      return V4L2_PIX_FMT_YUV420M;

    default:
      LOG(FATAL) << "Add more cases as needed";
      return 0;
  }
}

// static
uint32 V4L2Device::VideoCodecProfileToV4L2PixFmt(
    media::VideoCodecProfile profile,
    bool slice_based) {
  if (profile >= media::H264PROFILE_MIN &&
      profile <= media::H264PROFILE_MAX) {
    if (slice_based)
      return V4L2_PIX_FMT_H264_SLICE;
    else
      return V4L2_PIX_FMT_H264;
  } else if (profile >= media::VP8PROFILE_MIN &&
             profile <= media::VP8PROFILE_MAX) {
    if (slice_based)
      return V4L2_PIX_FMT_VP8_FRAME;
    else
      return V4L2_PIX_FMT_VP8;
  } else if (profile >= media::VP9PROFILE_MIN &&
             profile <= media::VP9PROFILE_MAX) {
    return V4L2_PIX_FMT_VP9;
  } else {
    LOG(FATAL) << "Add more cases as needed";
    return 0;
  }
}

// static
uint32_t V4L2Device::V4L2PixFmtToDrmFormat(uint32_t format) {
  switch (format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      return DRM_FORMAT_NV12;

    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      return DRM_FORMAT_YUV420;

    case V4L2_PIX_FMT_RGB32:
      return DRM_FORMAT_ARGB8888;

    default:
      DVLOG(1) << "Add more cases as needed";
      return 0;
  }
}

// static
gfx::Size V4L2Device::CodedSizeFromV4L2Format(struct v4l2_format format) {
  gfx::Size coded_size;
  gfx::Size visible_size;
  media::VideoFrame::Format frame_format = media::VideoFrame::UNKNOWN;
  size_t bytesperline = 0;
  // Total bytes in the frame.
  size_t sizeimage = 0;

  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    DCHECK_GT(format.fmt.pix_mp.num_planes, 0);
    bytesperline =
        base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[0].bytesperline);
    for (size_t i = 0; i < format.fmt.pix_mp.num_planes; ++i) {
      sizeimage +=
          base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[i].sizeimage);
    }
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix_mp.width),
                         base::checked_cast<int>(format.fmt.pix_mp.height));
    frame_format =
        V4L2Device::V4L2PixFmtToVideoFrameFormat(format.fmt.pix_mp.pixelformat);
  } else {
    bytesperline = base::checked_cast<int>(format.fmt.pix.bytesperline);
    sizeimage = base::checked_cast<int>(format.fmt.pix.sizeimage);
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix.width),
                         base::checked_cast<int>(format.fmt.pix.height));
    frame_format =
        V4L2Device::V4L2PixFmtToVideoFrameFormat(format.fmt.pix.pixelformat);
  }

  // V4L2 does not provide per-plane bytesperline (bpl) when different
  // components are sharing one physical plane buffer. In this case, it only
  // provides bpl for the first component in the plane. So we can't depend on it
  // for calculating height, because bpl may vary within one physical plane
  // buffer. For example, YUV420 contains 3 components in one physical plane,
  // with Y at 8 bits per pixel, and Cb/Cr at 4 bits per pixel per component,
  // but we only get 8 pits per pixel from bytesperline in physical plane 0.
  // So we need to get total frame bpp from elsewhere to calculate coded height.

  // We need bits per pixel for one component only to calculate
  // coded_width from bytesperline.
  int plane_horiz_bits_per_pixel =
      media::VideoFrame::PlaneHorizontalBitsPerPixel(frame_format, 0);

  // Adding up bpp for each component will give us total bpp for all components.
  int total_bpp = 0;
  for (size_t i = 0; i < media::VideoFrame::NumPlanes(frame_format); ++i)
    total_bpp += media::VideoFrame::PlaneBitsPerPixel(frame_format, i);

  if (sizeimage == 0 || bytesperline == 0 || plane_horiz_bits_per_pixel == 0 ||
      total_bpp == 0 || (bytesperline * 8) % plane_horiz_bits_per_pixel != 0) {
    LOG(ERROR) << "Invalid format provided";
    return coded_size;
  }

  // Coded width can be calculated by taking the first component's bytesperline,
  // which in V4L2 always applies to the first component in physical plane
  // buffer.
  int coded_width = bytesperline * 8 / plane_horiz_bits_per_pixel;
  // Sizeimage is coded_width * coded_height * total_bpp.
  int coded_height = sizeimage * 8 / coded_width / total_bpp;

  coded_size.SetSize(coded_width, coded_height);
  // It's possible the driver gave us a slightly larger sizeimage than what
  // would be calculated from coded size. This is technically not allowed, but
  // some drivers (Exynos) like to have some additional alignment that is not a
  // multiple of bytesperline. The best thing we can do is to compensate by
  // aligning to next full row.
  if (sizeimage > media::VideoFrame::AllocationSize(frame_format, coded_size))
    coded_size.SetSize(coded_width, coded_height + 1);
  DVLOG(3) << "coded_size=" << coded_size.ToString();

  // Sanity checks. Calculated coded size has to contain given visible size
  // and fulfill buffer byte size requirements.
  DCHECK(gfx::Rect(coded_size).Contains(gfx::Rect(visible_size)));
  DCHECK_LE(sizeimage,
            media::VideoFrame::AllocationSize(frame_format, coded_size));

  return coded_size;
}

void V4L2Device::GetSupportedResolution(uint32_t pixelformat,
                                        gfx::Size* min_resolution,
                                        gfx::Size* max_resolution) {
  max_resolution->SetSize(0, 0);
  min_resolution->SetSize(0, 0);
  v4l2_frmsizeenum frame_size;
  memset(&frame_size, 0, sizeof(frame_size));
  frame_size.pixel_format = pixelformat;
  for (; Ioctl(VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0; ++frame_size.index) {
    if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
      if (frame_size.discrete.width >=
              base::checked_cast<uint32_t>(max_resolution->width()) &&
          frame_size.discrete.height >=
              base::checked_cast<uint32_t>(max_resolution->height())) {
        max_resolution->SetSize(frame_size.discrete.width,
                                frame_size.discrete.height);
      }
      if (min_resolution->IsEmpty() ||
          (frame_size.discrete.width <=
               base::checked_cast<uint32_t>(min_resolution->width()) &&
           frame_size.discrete.height <=
               base::checked_cast<uint32_t>(min_resolution->height()))) {
        min_resolution->SetSize(frame_size.discrete.width,
                                frame_size.discrete.height);
      }
    } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
               frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
      max_resolution->SetSize(frame_size.stepwise.max_width,
                              frame_size.stepwise.max_height);
      min_resolution->SetSize(frame_size.stepwise.min_width,
                              frame_size.stepwise.min_height);
      break;
    }
  }
  if (max_resolution->IsEmpty()) {
    max_resolution->SetSize(1920, 1088);
    LOG(ERROR) << "GetSupportedResolution failed to get maximum resolution for "
               << "fourcc " << std::hex << pixelformat
               << ", fall back to " << max_resolution->ToString();
  }
  if (min_resolution->IsEmpty()) {
    min_resolution->SetSize(16, 16);
    LOG(ERROR) << "GetSupportedResolution failed to get minimum resolution for "
               << "fourcc " << std::hex << pixelformat
               << ", fall back to " << min_resolution->ToString();
  }
}

media::VideoDecodeAccelerator::SupportedProfiles
V4L2Device::GetSupportedDecodeProfiles(const size_t num_formats,
                                       const uint32_t pixelformats[]) {
  DCHECK_EQ(type_, kDecoder);
  media::VideoDecodeAccelerator::SupportedProfiles profiles;
  media::VideoDecodeAccelerator::SupportedProfile profile;
  v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

  for (; Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0; ++fmtdesc.index) {
    if (std::find(pixelformats, pixelformats + num_formats,
                  fmtdesc.pixelformat) == pixelformats + num_formats)
      continue;
    int min_profile, max_profile;
    switch (fmtdesc.pixelformat) {
      case V4L2_PIX_FMT_H264:
      case V4L2_PIX_FMT_H264_SLICE:
        min_profile = media::H264PROFILE_MIN;
        max_profile = media::H264PROFILE_MAX;
        break;
      case V4L2_PIX_FMT_VP8:
      case V4L2_PIX_FMT_VP8_FRAME:
        min_profile = media::VP8PROFILE_MIN;
        max_profile = media::VP8PROFILE_MAX;
        break;
      case V4L2_PIX_FMT_VP9:
        min_profile = media::VP9PROFILE_MIN;
        max_profile = media::VP9PROFILE_MAX;
        break;
      default:
        NOTREACHED() << "Unhandled pixelformat " << std::hex
                     << fmtdesc.pixelformat;
        return profiles;
    }
    GetSupportedResolution(fmtdesc.pixelformat, &profile.min_resolution,
                           &profile.max_resolution);
    for (int media_profile = min_profile; media_profile <= max_profile;
         ++media_profile) {
      profile.profile = static_cast<media::VideoCodecProfile>(media_profile);
      profiles.push_back(profile);
    }
  }
  return profiles;
}

}  //  namespace content
