// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/demuxer_stream_provider_shim.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"

namespace media {

DemuxerStreamProviderShim::DemuxerStreamProviderShim(
    mojo::DemuxerStreamPtr audio,
    mojo::DemuxerStreamPtr video,
    const base::Closure& demuxer_ready_cb)
    : demuxer_ready_cb_(demuxer_ready_cb),
      streams_ready_(0),
      weak_factory_(this) {
  DCHECK(audio || video);
  DCHECK(!demuxer_ready_cb_.is_null());

  if (audio) {
    streams_.push_back(new MojoDemuxerStreamAdapter(
        audio.Pass(),
        base::Bind(&DemuxerStreamProviderShim::OnStreamReady,
                   weak_factory_.GetWeakPtr())));
  }

  if (video) {
    streams_.push_back(new MojoDemuxerStreamAdapter(
        video.Pass(),
        base::Bind(&DemuxerStreamProviderShim::OnStreamReady,
                   weak_factory_.GetWeakPtr())));
  }
}

DemuxerStreamProviderShim::~DemuxerStreamProviderShim() {
}

DemuxerStream* DemuxerStreamProviderShim::GetStream(DemuxerStream::Type type) {
  DCHECK(demuxer_ready_cb_.is_null());
  for (auto* stream : streams_) {
    if (stream->type() == type)
      return stream;
  }

  return nullptr;
}

void DemuxerStreamProviderShim::OnStreamReady() {
  if (++streams_ready_ == streams_.size())
    base::ResetAndReturn(&demuxer_ready_cb_).Run();
}

}  // namespace media
