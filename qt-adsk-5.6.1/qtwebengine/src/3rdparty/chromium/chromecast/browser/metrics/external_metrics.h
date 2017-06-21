// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_METRICS_EXTERNAL_METRICS_H_
#define CHROMECAST_BROWSER_METRICS_EXTERNAL_METRICS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace metrics {
class MetricSample;
}  // namespace metrics

namespace chromecast {
namespace metrics {

class CastStabilityMetricsProvider;

// ExternalMetrics service allows processes outside of the Chromecast browser
// process to upload metrics via reading/writing to a known shared file.
class ExternalMetrics {
 public:
  explicit ExternalMetrics(CastStabilityMetricsProvider* stability_provider);

  // Begins external data collection. Calls to RecordAction originate in the
  // File thread but are executed in the UI thread.
  void Start();

  // Destroys itself in appropriate thread.
  void StopAndDestroy();

 private:
  friend class base::DeleteHelper<ExternalMetrics>;

  ~ExternalMetrics();

  // The max length of a message (name-value pair, plus header)
  static const int kMetricsMessageMaxLength = 1024;  // be generous

  // Records an external crash of the given string description.
  void RecordCrash(const std::string& crash_kind);

  // Records a sparse histogram. |sample| is expected to be a sparse histogram.
  void RecordSparseHistogram(const ::metrics::MetricSample& sample);

  // Collects external events from metrics log file.  This is run at periodic
  // intervals.
  //
  // Returns the number of events collected.
  int CollectEvents();

  // Calls CollectEvents and reschedules a future collection.
  void CollectEventsAndReschedule();

  // Schedules a metrics event collection in the future.
  void ScheduleCollector();

  // Reference to stability metrics provider, for reporting external crashes.
  CastStabilityMetricsProvider* stability_provider_;

  // File used by libmetrics to send metrics to the browser process.
  std::string uma_events_file_;

  base::WeakPtrFactory<ExternalMetrics> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExternalMetrics);
};

}  // namespace metrics
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_METRICS_EXTERNAL_METRICS_H_
