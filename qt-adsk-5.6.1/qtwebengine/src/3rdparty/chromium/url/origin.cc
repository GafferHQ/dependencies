// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url/origin.h"

#include "base/strings/pattern.h"
#include "base/strings/string_util.h"

namespace url {

Origin::Origin() : string_("null") {}

Origin::Origin(const std::string& origin) : string_(origin) {
  DCHECK(origin == "null" || base::MatchPattern(origin, "?*://?*"));
  DCHECK_GT(origin.size(), 0u);
#ifdef TOOLKIT_QT
  DCHECK(origin == "qrc://" || origin == "file://" || origin[origin.size() - 1] != '/');
#else
  DCHECK(origin == "file://" || origin[origin.size() - 1] != '/');
#endif
}

}  // namespace url
