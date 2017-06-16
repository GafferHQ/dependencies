/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/random.h"

#include <math.h>

namespace webrtc {

Random::Random(uint32_t seed) : a_(0x531FDB97 ^ seed), b_(0x6420ECA8 + seed) {
}

float Random::Rand() {
  const float kScale = 1.0f / 0xffffffff;
  float result = kScale * b_;
  a_ ^= b_;
  b_ += a_;
  return result;
}

int Random::Gaussian(int mean, int standard_deviation) {
  // Creating a Normal distribution variable from two independent uniform
  // variables based on the Box-Muller transform, which is defined on the
  // interval (0, 1], hence the mask+add below.
  const double kPi = 3.14159265358979323846;
  const double kScale = 1.0 / 0x80000000ul;
  double u1 = kScale * ((a_ & 0x7ffffffful) + 1);
  double u2 = kScale * ((b_ & 0x7ffffffful) + 1);
  a_ ^= b_;
  b_ += a_;
  return static_cast<int>(
      mean + standard_deviation * sqrt(-2 * log(u1)) * cos(2 * kPi * u2));
}
}  // namespace webrtc
