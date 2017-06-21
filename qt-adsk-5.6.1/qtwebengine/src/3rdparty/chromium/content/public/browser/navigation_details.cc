// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_details.h"

namespace content {

LoadCommittedDetails::LoadCommittedDetails()
    : entry(nullptr),
      type(content::NAVIGATION_TYPE_UNKNOWN),
      previous_entry_index(-1),
      did_replace_entry(false),
      is_in_page(false),
      is_main_frame(true),
      http_status_code(0) {
}

}  // namespace content
