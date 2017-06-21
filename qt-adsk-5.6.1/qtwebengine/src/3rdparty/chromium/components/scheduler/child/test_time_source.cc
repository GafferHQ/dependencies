// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/test_time_source.h"

namespace scheduler {

TestTimeSource::TestTimeSource(base::SimpleTestTickClock* time_source)
    : time_source_(time_source) {
}

TestTimeSource::~TestTimeSource() {
}

base::TimeTicks TestTimeSource::NowTicks() {
  return time_source_->NowTicks();
}

}  // namespace scheduler
