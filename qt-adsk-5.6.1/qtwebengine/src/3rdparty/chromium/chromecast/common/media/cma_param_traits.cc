// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/media/cma_param_traits.h"

#include <vector>

#include "chromecast/common/media/cma_param_traits_macros.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

// Note(gunsch): these are currently defined in content/, but not declared in
// content/public/. These headers need to be forward-declared for chromecast/,
// but without new implementations linked in.
// The correct long-term fix is to use Mojo instead of the content/ IPCs.
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::ChannelLayout,
                              media::ChannelLayout::CHANNEL_LAYOUT_NONE,
                              media::ChannelLayout::CHANNEL_LAYOUT_MAX)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media::VideoCodecProfile,
                              media::VIDEO_CODEC_PROFILE_MIN,
                              media::VIDEO_CODEC_PROFILE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::VideoFrame::Format,
                          media::VideoFrame::FORMAT_MAX)

namespace IPC {

void ParamTraits<media::AudioDecoderConfig>::Write(
    Message* m, const media::AudioDecoderConfig& p) {
  WriteParam(m, p.codec());
  WriteParam(m, p.sample_format());
  WriteParam(m, p.channel_layout());
  WriteParam(m, p.samples_per_second());
  WriteParam(m, p.is_encrypted());
  std::vector<uint8> extra_data;
  if (p.extra_data_size() > 0) {
    extra_data =
        std::vector<uint8>(p.extra_data(),
                           p.extra_data() + p.extra_data_size());
  }
  WriteParam(m, extra_data);
}

bool ParamTraits<media::AudioDecoderConfig>::Read(
    const Message* m,
    base::PickleIterator* iter,
    media::AudioDecoderConfig* r) {
  media::AudioCodec codec;
  media::SampleFormat sample_format;
  media::ChannelLayout channel_layout;
  int samples_per_second;
  bool is_encrypted;
  std::vector<uint8> extra_data;
  if (!ReadParam(m, iter, &codec) || !ReadParam(m, iter, &sample_format) ||
      !ReadParam(m, iter, &channel_layout) ||
      !ReadParam(m, iter, &samples_per_second) ||
      !ReadParam(m, iter, &is_encrypted) || !ReadParam(m, iter, &extra_data))
    return false;
  const uint8* extra_data_ptr = nullptr;
  if (!extra_data.empty())
    extra_data_ptr = &extra_data[0];
  *r = media::AudioDecoderConfig(codec, sample_format, channel_layout,
                                 samples_per_second,
                                 extra_data_ptr, extra_data.size(),
                                 is_encrypted);
  return true;
}

void ParamTraits<media::AudioDecoderConfig>::Log(
    const media::AudioDecoderConfig& p, std::string* l) {
  l->append(base::StringPrintf("<AudioDecoderConfig>"));
}

void ParamTraits<media::VideoDecoderConfig>::Write(
    Message* m, const media::VideoDecoderConfig& p) {
  WriteParam(m, p.codec());
  WriteParam(m, p.profile());
  WriteParam(m, p.format());
  WriteParam(m, p.coded_size());
  WriteParam(m, p.visible_rect());
  WriteParam(m, p.natural_size());
  WriteParam(m, p.is_encrypted());
  std::vector<uint8> extra_data;
  if (p.extra_data_size() > 0) {
    extra_data =
        std::vector<uint8>(p.extra_data(),
                           p.extra_data() + p.extra_data_size());
  }
  WriteParam(m, extra_data);
}

bool ParamTraits<media::VideoDecoderConfig>::Read(
    const Message* m,
    base::PickleIterator* iter,
    media::VideoDecoderConfig* r) {
  media::VideoCodec codec;
  media::VideoCodecProfile profile;
  media::VideoFrame::Format format;
  gfx::Size coded_size;
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  bool is_encrypted;
  std::vector<uint8> extra_data;
  if (!ReadParam(m, iter, &codec) || !ReadParam(m, iter, &profile) ||
      !ReadParam(m, iter, &format) || !ReadParam(m, iter, &coded_size) ||
      !ReadParam(m, iter, &visible_rect) ||
      !ReadParam(m, iter, &natural_size) ||
      !ReadParam(m, iter, &is_encrypted) || !ReadParam(m, iter, &extra_data))
    return false;
  const uint8* extra_data_ptr = nullptr;
  if (!extra_data.empty())
    extra_data_ptr = &extra_data[0];
  *r = media::VideoDecoderConfig(codec, profile, format,
                                 coded_size, visible_rect, natural_size,
                                 extra_data_ptr, extra_data.size(),
                                 is_encrypted);
  return true;
}

void ParamTraits<media::VideoDecoderConfig>::Log(
    const media::VideoDecoderConfig& p, std::string* l) {
  l->append(base::StringPrintf("<VideoDecoderConfig>"));
}

}  // namespace IPC
