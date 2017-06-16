// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/histograms.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/numerics/safe_conversions.h"

namespace cc {

// Minimum elapsed time of 1us to limit weighting of fast calls.
static const int64 kMinimumTimeMicroseconds = 1;

ScopedUMAHistogramAreaTimerBase::ScopedUMAHistogramAreaTimerBase() : area_(0) {
}

ScopedUMAHistogramAreaTimerBase::~ScopedUMAHistogramAreaTimerBase() {
}

bool ScopedUMAHistogramAreaTimerBase::GetHistogramValues(
    Sample* time_microseconds,
    Sample* pixels_per_ms) const {
  return GetHistogramValues(
      timer_.Elapsed(), area_.ValueOrDefault(std::numeric_limits<int>::max()),
      time_microseconds, pixels_per_ms);
}

// static
bool ScopedUMAHistogramAreaTimerBase::GetHistogramValues(
    base::TimeDelta elapsed,
    int area,
    Sample* time_microseconds,
    Sample* pixels_per_ms) {
  elapsed = std::max(
      elapsed, base::TimeDelta::FromMicroseconds(kMinimumTimeMicroseconds));
  double area_per_time = area / elapsed.InMillisecondsF();
  // It is not clear how NaN can get here, but we've gotten crashes from
  // saturated_cast. http://crbug.com/486214
  if (std::isnan(area_per_time))
    return false;
  *time_microseconds = base::saturated_cast<Sample>(elapsed.InMicroseconds());
  *pixels_per_ms = base::saturated_cast<Sample>(area_per_time);
  return true;
}

}  // namespace cc
