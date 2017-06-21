// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_api.h"

#include "base/base64.h"
#include "base/callback.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/video_capturer_source.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "url/gurl.h"

namespace content {

namespace {

blink::WebString MakeTrackId() {
  std::string track_id;
  base::Base64Encode(base::RandBytesAsString(64), &track_id);
  return base::UTF8ToUTF16(track_id);
}

}  // namespace

bool AddVideoTrackToMediaStream(
    scoped_ptr<media::VideoCapturerSource> source,
    bool is_remote,
    bool is_readonly,
    const std::string& media_stream_url) {
  blink::WebMediaStream stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(
          GURL(media_stream_url));

  if (stream.isNull()) {
    LOG(ERROR) << "Stream not found";
    return false;
  }
  blink::WebString track_id = MakeTrackId();
  blink::WebMediaStreamSource webkit_source;
  scoped_ptr<MediaStreamVideoSource> media_stream_source(
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(),
          source.Pass()));
  webkit_source.initialize(
      track_id,
      blink::WebMediaStreamSource::TypeVideo,
      track_id,
      is_remote,
      is_readonly);
  webkit_source.setExtraData(media_stream_source.get());

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  stream.addTrack(MediaStreamVideoTrack::CreateVideoTrack(
      media_stream_source.release(),
      constraints,
      MediaStreamVideoSource::ConstraintsCallback(),
      true));
  return true;
}

bool AddAudioTrackToMediaStream(
    scoped_refptr<media::AudioCapturerSource> source,
    const media::AudioParameters& params,
    bool is_remote,
    bool is_readonly,
    const std::string& media_stream_url) {
  DCHECK(params.IsValid()) << params.AsHumanReadableString();
  blink::WebMediaStream stream =
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(
          GURL(media_stream_url));

  if (stream.isNull()) {
    LOG(ERROR) << "Stream not found";
    return false;
  }
  blink::WebMediaStreamSource webkit_source;
  blink::WebString track_id = MakeTrackId();
  webkit_source.initialize(
      track_id,
      blink::WebMediaStreamSource::TypeAudio,
      track_id,
      is_remote,
      is_readonly);

  MediaStreamAudioSource* audio_source(
      new MediaStreamAudioSource(
          -1,
          StreamDeviceInfo(),
          MediaStreamSource::SourceStoppedCallback(),
          RenderThreadImpl::current()->GetPeerConnectionDependencyFactory()));

  blink::WebMediaConstraints constraints;
  constraints.initialize();
  scoped_refptr<WebRtcAudioCapturer> capturer(
      WebRtcAudioCapturer::CreateCapturer(-1, StreamDeviceInfo(), constraints,
                                          nullptr, audio_source));
  capturer->SetCapturerSource(source, params);
  audio_source->SetAudioCapturer(capturer);
  webkit_source.setExtraData(audio_source);

  blink::WebMediaStreamTrack web_media_audio_track;
  web_media_audio_track.initialize(webkit_source);
  RenderThreadImpl::current()->GetPeerConnectionDependencyFactory()->
      CreateLocalAudioTrack(web_media_audio_track);

  stream.addTrack(web_media_audio_track);
  return true;
}

}  // namespace content
