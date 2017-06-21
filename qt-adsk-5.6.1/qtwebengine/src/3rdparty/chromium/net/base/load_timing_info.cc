// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_timing_info.h"

#include "net/log/net_log.h"

namespace net {

LoadTimingInfo::ConnectTiming::ConnectTiming() {}

LoadTimingInfo::ConnectTiming::~ConnectTiming() {}

LoadTimingInfo::LoadTimingInfo() : socket_reused(false),
                                   socket_log_id(NetLog::Source::kInvalidId) {
}

LoadTimingInfo::~LoadTimingInfo() {}

}  // namespace net

