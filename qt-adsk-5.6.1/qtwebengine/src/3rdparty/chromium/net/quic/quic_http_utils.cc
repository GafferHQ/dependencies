// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_utils.h"

namespace net {

QuicPriority ConvertRequestPriorityToQuicPriority(
    const RequestPriority priority) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);
  return static_cast<QuicPriority>(HIGHEST - priority);
}

NET_EXPORT_PRIVATE RequestPriority ConvertQuicPriorityToRequestPriority(
    QuicPriority priority) {
  // Handle invalid values gracefully.
  return (priority >= 5) ?
      IDLE : static_cast<RequestPriority>(HIGHEST - priority);
}

scoped_ptr<base::Value> QuicRequestNetLogCallback(
    QuicStreamId stream_id,
    const SpdyHeaderBlock* headers,
    QuicPriority priority,
    NetLogCaptureMode capture_mode) {
  scoped_ptr<base::DictionaryValue> dict(static_cast<base::DictionaryValue*>(
      SpdyHeaderBlockNetLogCallback(headers, capture_mode).release()));
  dict->SetInteger("quic_priority", static_cast<int>(priority));
  dict->SetInteger("quic_stream_id", static_cast<int>(stream_id));
  return dict.Pass();
}

}  // namespace net
