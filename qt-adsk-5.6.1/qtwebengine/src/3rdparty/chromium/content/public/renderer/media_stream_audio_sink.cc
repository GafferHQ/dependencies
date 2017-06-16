// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_audio_sink.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

void MediaStreamAudioSink::AddToAudioTrack(
    MediaStreamAudioSink* sink,
    const blink::WebMediaStreamTrack& track) {
  DCHECK(track.source().type() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  // TODO(xians): Support remote audio track.
  if (!native_track->is_local_track()) {
    LOG(ERROR) << "Can't add the sink to a remote audio track";
    return;
  }

  WebRtcLocalAudioTrack* audio_track =
      static_cast<WebRtcLocalAudioTrack*>(native_track);
  audio_track->AddSink(sink);
}

void MediaStreamAudioSink::RemoveFromAudioTrack(
    MediaStreamAudioSink* sink,
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  // TODO(xians): Support remote audio track.
  if (!native_track->is_local_track()) {
    LOG(ERROR) << "Can't remove the sink from a remote audio track";
    return;
  }

  WebRtcLocalAudioTrack* audio_track =
      static_cast<WebRtcLocalAudioTrack*>(native_track);
  audio_track->RemoveSink(sink);
}

media::AudioParameters MediaStreamAudioSink::GetFormatFromAudioTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  if (!native_track->is_local_track()) {
    LOG(ERROR) << "Can't get format from a remote audio track";
    return media::AudioParameters();
  }

  WebRtcLocalAudioTrack* audio_track =
      static_cast<WebRtcLocalAudioTrack*>(native_track);
  return audio_track->GetOutputFormat();
}

}  // namespace content
