/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/app/webrtc/fakemetricsobserver.h"
#include "webrtc/base/checks.h"

namespace webrtc {

FakeMetricsObserver::FakeMetricsObserver() {
  Reset();
}

void FakeMetricsObserver::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  memset(counters_, 0, sizeof(counters_));
  memset(int_histogram_samples_, 0, sizeof(int_histogram_samples_));
  for (std::string& type : string_histogram_samples_) {
    type.clear();
  }
}

void FakeMetricsObserver::IncrementCounter(PeerConnectionMetricsCounter type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ++counters_[type];
}

void FakeMetricsObserver::AddHistogramSample(PeerConnectionMetricsName type,
    int value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(int_histogram_samples_[type], 0);
  int_histogram_samples_[type] = value;
}

void FakeMetricsObserver::AddHistogramSample(PeerConnectionMetricsName type,
    const std::string& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  string_histogram_samples_[type].assign(value);
}

int FakeMetricsObserver::GetCounter(PeerConnectionMetricsCounter type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return counters_[type];
}

int FakeMetricsObserver::GetIntHistogramSample(
    PeerConnectionMetricsName type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return int_histogram_samples_[type];
}

const std::string& FakeMetricsObserver::GetStringHistogramSample(
    PeerConnectionMetricsName type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return string_histogram_samples_[type];
}

}  // namespace webrtc
