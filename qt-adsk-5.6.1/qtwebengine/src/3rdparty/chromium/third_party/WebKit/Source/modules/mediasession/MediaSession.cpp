// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/mediasession/MediaSession.h"

namespace blink {

MediaSession* MediaSession::create()
{
    return new MediaSession;
}

void MediaSession::activate()
{
}

void MediaSession::deactivate()
{
}

} // namespace blink
