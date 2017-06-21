// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/request_info.h"

namespace content {

RequestInfo::RequestInfo()
    : load_flags(0),
      requestor_pid(0),
      request_type(RESOURCE_TYPE_MAIN_FRAME),
      fetch_request_context_type(REQUEST_CONTEXT_TYPE_UNSPECIFIED),
      fetch_frame_type(REQUEST_CONTEXT_FRAME_TYPE_NONE),
      priority(net::LOW),
      request_context(0),
      appcache_host_id(0),
      routing_id(0),
      download_to_file(false),
      has_user_gesture(false),
      skip_service_worker(false),
      should_reset_appcache(false),
      fetch_request_mode(FETCH_REQUEST_MODE_NO_CORS),
      fetch_credentials_mode(FETCH_CREDENTIALS_MODE_OMIT),
      enable_load_timing(false),
      enable_upload_progress(false),
      do_not_prompt_for_login(false),
      extra_data(NULL) {
}

RequestInfo::~RequestInfo() {}

}  // namespace content
