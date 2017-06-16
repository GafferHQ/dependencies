// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_COMPOSITOR_TIMING_HISTORY_H_
#define CC_SCHEDULER_COMPOSITOR_TIMING_HISTORY_H_

#include "cc/base/rolling_time_delta_history.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

class RenderingStatsInstrumentation;

class CC_EXPORT CompositorTimingHistory {
 public:
  explicit CompositorTimingHistory(
      RenderingStatsInstrumentation* rendering_stats_instrumentation);
  virtual ~CompositorTimingHistory();

  void AsValueInto(base::trace_event::TracedValue* state) const;

  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() const;
  virtual base::TimeDelta CommitToReadyToActivateDurationEstimate() const;
  virtual base::TimeDelta PrepareTilesDurationEstimate() const;
  virtual base::TimeDelta ActivateDurationEstimate() const;
  virtual base::TimeDelta DrawDurationEstimate() const;

  void SetRecordingEnabled(bool enabled);

  void WillBeginMainFrame();
  void BeginMainFrameAborted();
  void DidCommit();
  void WillPrepareTiles();
  void DidPrepareTiles();
  void ReadyToActivate();
  void WillActivate();
  void DidActivate();
  void WillDraw();
  void DidDraw();

 protected:
  virtual base::TimeTicks Now() const;

  void AddDrawDurationUMA(base::TimeDelta draw_duration,
                          base::TimeDelta draw_duration_estimate);

  bool enabled_;

  RollingTimeDeltaHistory begin_main_frame_to_commit_duration_history_;
  RollingTimeDeltaHistory commit_to_ready_to_activate_duration_history_;
  RollingTimeDeltaHistory prepare_tiles_duration_history_;
  RollingTimeDeltaHistory activate_duration_history_;
  RollingTimeDeltaHistory draw_duration_history_;

  base::TimeTicks begin_main_frame_sent_time_;
  base::TimeTicks commit_time_;
  base::TimeTicks start_prepare_tiles_time_;
  base::TimeTicks start_activate_time_;
  base::TimeTicks start_draw_time_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorTimingHistory);
};

}  // namespace cc

#endif  // CC_SCHEDULER_COMPOSITOR_TIMING_HISTORY_H_
