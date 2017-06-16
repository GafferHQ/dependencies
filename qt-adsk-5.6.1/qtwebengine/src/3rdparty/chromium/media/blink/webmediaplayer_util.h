// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_UTIL_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_UTIL_H_

#include "base/time/time.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaTypes.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebSetSinkIdError.h"
#include "third_party/WebKit/public/platform/WebTimeRange.h"
#include "url/gurl.h"

namespace media {

// Platform independent method for converting and rounding floating point
// seconds to an int64 timestamp.
//
// Refer to https://bugs.webkit.org/show_bug.cgi?id=52697 for details.
base::TimeDelta MEDIA_EXPORT ConvertSecondsToTimestamp(double seconds);

blink::WebTimeRanges MEDIA_EXPORT ConvertToWebTimeRanges(
    const Ranges<base::TimeDelta>& ranges);

blink::WebMediaPlayer::NetworkState MEDIA_EXPORT PipelineErrorToNetworkState(
    PipelineStatus error);

// Report various metrics to UMA and RAPPOR.
void MEDIA_EXPORT ReportMetrics(blink::WebMediaPlayer::LoadType load_type,
                                const GURL& url,
                                const GURL& origin_url);

// Convert Initialization Data Types.
EmeInitDataType MEDIA_EXPORT
ConvertToEmeInitDataType(blink::WebEncryptedMediaInitDataType init_data_type);
blink::WebEncryptedMediaInitDataType MEDIA_EXPORT
ConvertToWebInitDataType(EmeInitDataType init_data_type);

typedef blink::WebCallbacks<void, blink::WebSetSinkIdError> WebSetSinkIdCB;

// Wraps a WebSetSinkIdCB into a media::SwitchOutputDeviceCB
// and binds it to the current thread
SwitchOutputDeviceCB MEDIA_EXPORT
ConvertToSwitchOutputDeviceCB(WebSetSinkIdCB* web_callbacks);

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_UTIL_H_
