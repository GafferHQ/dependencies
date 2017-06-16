// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_state_machine.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"

namespace cc {

SchedulerStateMachine::SchedulerStateMachine(const SchedulerSettings& settings)
    : settings_(settings),
      output_surface_state_(OUTPUT_SURFACE_LOST),
      begin_impl_frame_state_(BEGIN_IMPL_FRAME_STATE_IDLE),
      commit_state_(COMMIT_STATE_IDLE),
      forced_redraw_state_(FORCED_REDRAW_STATE_IDLE),
      commit_count_(0),
      current_frame_number_(0),
      last_frame_number_animate_performed_(-1),
      last_frame_number_swap_performed_(-1),
      last_frame_number_swap_requested_(-1),
      last_frame_number_begin_main_frame_sent_(-1),
      last_frame_number_invalidate_output_surface_performed_(-1),
      animate_funnel_(false),
      request_swap_funnel_(false),
      send_begin_main_frame_funnel_(true),
      invalidate_output_surface_funnel_(false),
      prepare_tiles_funnel_(0),
      consecutive_checkerboard_animations_(0),
      max_pending_swaps_(1),
      pending_swaps_(0),
      swaps_with_current_output_surface_(0),
      needs_redraw_(false),
      needs_animate_(false),
      needs_prepare_tiles_(false),
      needs_commit_(false),
      visible_(false),
      can_start_(false),
      can_draw_(false),
      has_pending_tree_(false),
      pending_tree_is_ready_for_activation_(false),
      active_tree_needs_first_draw_(false),
      did_create_and_initialize_first_output_surface_(false),
      impl_latency_takes_priority_(false),
      skip_next_begin_main_frame_to_reduce_latency_(false),
      continuous_painting_(false),
      children_need_begin_frames_(false),
      defer_commits_(false),
      video_needs_begin_frames_(false),
      last_commit_had_no_updates_(false),
      wait_for_active_tree_ready_to_draw_(false),
      did_request_swap_in_last_frame_(false),
      did_perform_swap_in_last_draw_(false) {
}

const char* SchedulerStateMachine::OutputSurfaceStateToString(
    OutputSurfaceState state) {
  switch (state) {
    case OUTPUT_SURFACE_ACTIVE:
      return "OUTPUT_SURFACE_ACTIVE";
    case OUTPUT_SURFACE_LOST:
      return "OUTPUT_SURFACE_LOST";
    case OUTPUT_SURFACE_CREATING:
      return "OUTPUT_SURFACE_CREATING";
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT:
      return "OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT";
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION:
      return "OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::BeginImplFrameStateToString(
    BeginImplFrameState state) {
  switch (state) {
    case BEGIN_IMPL_FRAME_STATE_IDLE:
      return "BEGIN_IMPL_FRAME_STATE_IDLE";
    case BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING:
      return "BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING";
    case BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME:
      return "BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME";
    case BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE:
      return "BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::BeginImplFrameDeadlineModeToString(
    BeginImplFrameDeadlineMode mode) {
  switch (mode) {
    case BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE:
      return "BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE";
    case BEGIN_IMPL_FRAME_DEADLINE_MODE_IMMEDIATE:
      return "BEGIN_IMPL_FRAME_DEADLINE_MODE_IMMEDIATE";
    case BEGIN_IMPL_FRAME_DEADLINE_MODE_REGULAR:
      return "BEGIN_IMPL_FRAME_DEADLINE_MODE_REGULAR";
    case BEGIN_IMPL_FRAME_DEADLINE_MODE_LATE:
      return "BEGIN_IMPL_FRAME_DEADLINE_MODE_LATE";
    case BEGIN_IMPL_FRAME_DEADLINE_MODE_BLOCKED_ON_READY_TO_DRAW:
      return "BEGIN_IMPL_FRAME_DEADLINE_MODE_BLOCKED_ON_READY_TO_DRAW";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::CommitStateToString(CommitState state) {
  switch (state) {
    case COMMIT_STATE_IDLE:
      return "COMMIT_STATE_IDLE";
    case COMMIT_STATE_BEGIN_MAIN_FRAME_SENT:
      return "COMMIT_STATE_BEGIN_MAIN_FRAME_SENT";
    case COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED:
      return "COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED";
    case COMMIT_STATE_READY_TO_COMMIT:
      return "COMMIT_STATE_READY_TO_COMMIT";
    case COMMIT_STATE_WAITING_FOR_ACTIVATION:
      return "COMMIT_STATE_WAITING_FOR_ACTIVATION";
    case COMMIT_STATE_WAITING_FOR_DRAW:
      return "COMMIT_STATE_WAITING_FOR_DRAW";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::ForcedRedrawOnTimeoutStateToString(
    ForcedRedrawOnTimeoutState state) {
  switch (state) {
    case FORCED_REDRAW_STATE_IDLE:
      return "FORCED_REDRAW_STATE_IDLE";
    case FORCED_REDRAW_STATE_WAITING_FOR_COMMIT:
      return "FORCED_REDRAW_STATE_WAITING_FOR_COMMIT";
    case FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION:
      return "FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION";
    case FORCED_REDRAW_STATE_WAITING_FOR_DRAW:
      return "FORCED_REDRAW_STATE_WAITING_FOR_DRAW";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::ActionToString(Action action) {
  switch (action) {
    case ACTION_NONE:
      return "ACTION_NONE";
    case ACTION_ANIMATE:
      return "ACTION_ANIMATE";
    case ACTION_SEND_BEGIN_MAIN_FRAME:
      return "ACTION_SEND_BEGIN_MAIN_FRAME";
    case ACTION_COMMIT:
      return "ACTION_COMMIT";
    case ACTION_ACTIVATE_SYNC_TREE:
      return "ACTION_ACTIVATE_SYNC_TREE";
    case ACTION_DRAW_AND_SWAP_IF_POSSIBLE:
      return "ACTION_DRAW_AND_SWAP_IF_POSSIBLE";
    case ACTION_DRAW_AND_SWAP_FORCED:
      return "ACTION_DRAW_AND_SWAP_FORCED";
    case ACTION_DRAW_AND_SWAP_ABORT:
      return "ACTION_DRAW_AND_SWAP_ABORT";
    case ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
      return "ACTION_BEGIN_OUTPUT_SURFACE_CREATION";
    case ACTION_PREPARE_TILES:
      return "ACTION_PREPARE_TILES";
    case ACTION_INVALIDATE_OUTPUT_SURFACE:
      return "ACTION_INVALIDATE_OUTPUT_SURFACE";
  }
  NOTREACHED();
  return "???";
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
SchedulerStateMachine::AsValue() const {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();
  AsValueInto(state.get());
  return state;
}

void SchedulerStateMachine::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->BeginDictionary("major_state");
  state->SetString("next_action", ActionToString(NextAction()));
  state->SetString("begin_impl_frame_state",
                   BeginImplFrameStateToString(begin_impl_frame_state_));
  state->SetString("commit_state", CommitStateToString(commit_state_));
  state->SetString("output_surface_state_",
                   OutputSurfaceStateToString(output_surface_state_));
  state->SetString("forced_redraw_state",
                   ForcedRedrawOnTimeoutStateToString(forced_redraw_state_));
  state->EndDictionary();

  state->BeginDictionary("minor_state");
  state->SetInteger("commit_count", commit_count_);
  state->SetInteger("current_frame_number", current_frame_number_);
  state->SetInteger("last_frame_number_animate_performed",
                    last_frame_number_animate_performed_);
  state->SetInteger("last_frame_number_swap_performed",
                    last_frame_number_swap_performed_);
  state->SetInteger("last_frame_number_swap_requested",
                    last_frame_number_swap_requested_);
  state->SetInteger("last_frame_number_begin_main_frame_sent",
                    last_frame_number_begin_main_frame_sent_);
  state->SetBoolean("funnel: animate_funnel", animate_funnel_);
  state->SetBoolean("funnel: request_swap_funnel", request_swap_funnel_);
  state->SetBoolean("funnel: send_begin_main_frame_funnel",
                    send_begin_main_frame_funnel_);
  state->SetInteger("funnel: prepare_tiles_funnel", prepare_tiles_funnel_);
  state->SetBoolean("funnel: invalidate_output_surface_funnel",
                    invalidate_output_surface_funnel_);
  state->SetInteger("consecutive_checkerboard_animations",
                    consecutive_checkerboard_animations_);
  state->SetInteger("max_pending_swaps_", max_pending_swaps_);
  state->SetInteger("pending_swaps_", pending_swaps_);
  state->SetInteger("swaps_with_current_output_surface",
                    swaps_with_current_output_surface_);
  state->SetBoolean("needs_redraw", needs_redraw_);
  state->SetBoolean("needs_animate_", needs_animate_);
  state->SetBoolean("needs_prepare_tiles", needs_prepare_tiles_);
  state->SetBoolean("needs_commit", needs_commit_);
  state->SetBoolean("visible", visible_);
  state->SetBoolean("can_start", can_start_);
  state->SetBoolean("can_draw", can_draw_);
  state->SetBoolean("has_pending_tree", has_pending_tree_);
  state->SetBoolean("pending_tree_is_ready_for_activation",
                    pending_tree_is_ready_for_activation_);
  state->SetBoolean("active_tree_needs_first_draw",
                    active_tree_needs_first_draw_);
  state->SetBoolean("wait_for_active_tree_ready_to_draw",
                    wait_for_active_tree_ready_to_draw_);
  state->SetBoolean("did_create_and_initialize_first_output_surface",
                    did_create_and_initialize_first_output_surface_);
  state->SetBoolean("impl_latency_takes_priority",
                    impl_latency_takes_priority_);
  state->SetBoolean("main_thread_is_in_high_latency_mode",
                    MainThreadIsInHighLatencyMode());
  state->SetBoolean("skip_next_begin_main_frame_to_reduce_latency",
                    skip_next_begin_main_frame_to_reduce_latency_);
  state->SetBoolean("continuous_painting", continuous_painting_);
  state->SetBoolean("children_need_begin_frames", children_need_begin_frames_);
  state->SetBoolean("video_needs_begin_frames", video_needs_begin_frames_);
  state->SetBoolean("defer_commits", defer_commits_);
  state->SetBoolean("last_commit_had_no_updates", last_commit_had_no_updates_);
  state->SetBoolean("did_request_swap_in_last_frame",
                    did_request_swap_in_last_frame_);
  state->SetBoolean("did_perform_swap_in_last_draw",
                    did_perform_swap_in_last_draw_);
  state->EndDictionary();
}

bool SchedulerStateMachine::PendingDrawsShouldBeAborted() const {
  // These are all the cases where we normally cannot or do not want to draw
  // but, if needs_redraw_ is true and we do not draw to make forward progress,
  // we might deadlock with the main thread.
  // This should be a superset of PendingActivationsShouldBeForced() since
  // activation of the pending tree is blocked by drawing of the active tree and
  // the main thread might be blocked on activation of the most recent commit.
  if (PendingActivationsShouldBeForced())
    return true;

  // Additional states where we should abort draws.
  if (!can_draw_)
    return true;
  return false;
}

bool SchedulerStateMachine::PendingActivationsShouldBeForced() const {
  // There is no output surface to trigger our activations.
  // If we do not force activations to make forward progress, we might deadlock
  // with the main thread.
  if (output_surface_state_ == OUTPUT_SURFACE_LOST)
    return true;

  // If we're not visible, we should force activation.
  // Since we set RequiresHighResToDraw when becoming visible, we ensure that we
  // don't checkerboard until all visible resources are done. Furthermore, if we
  // do keep the pending tree around, when becoming visible we might activate
  // prematurely causing RequiresHighResToDraw flag to be reset. In all cases,
  // we can simply activate on becoming invisible since we don't need to draw
  // the active tree when we're in this state.
  if (!visible_)
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldBeginOutputSurfaceCreation() const {
  // Don't try to initialize too early.
  if (!can_start_)
    return false;

  // We only want to start output surface initialization after the
  // previous commit is complete.
  if (commit_state_ != COMMIT_STATE_IDLE)
    return false;

  // Make sure the BeginImplFrame from any previous OutputSurfaces
  // are complete before creating the new OutputSurface.
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_IDLE)
    return false;

  // We want to clear the pipeline of any pending draws and activations
  // before starting output surface initialization. This allows us to avoid
  // weird corner cases where we abort draws or force activation while we
  // are initializing the output surface.
  if (active_tree_needs_first_draw_ || has_pending_tree_)
    return false;

  // We need to create the output surface if we don't have one and we haven't
  // started creating one yet.
  return output_surface_state_ == OUTPUT_SURFACE_LOST;
}

bool SchedulerStateMachine::ShouldDraw() const {
  // If we need to abort draws, we should do so ASAP since the draw could
  // be blocking other important actions (like output surface initialization),
  // from occurring. If we are waiting for the first draw, then perform the
  // aborted draw to keep things moving. If we are not waiting for the first
  // draw however, we don't want to abort for no reason.
  if (PendingDrawsShouldBeAborted())
    return active_tree_needs_first_draw_;

  // Do not draw too many times in a single frame. It's okay that we don't check
  // this before checking for aborted draws because aborted draws do not request
  // a swap.
  if (request_swap_funnel_)
    return false;

  // Don't draw if we are waiting on the first commit after a surface.
  if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE)
    return false;

  // Do not queue too many swaps.
  if (SwapThrottled())
    return false;

  // Except for the cases above, do not draw outside of the BeginImplFrame
  // deadline.
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)
    return false;

  // Only handle forced redraws due to timeouts on the regular deadline.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
    return true;

  return needs_redraw_;
}

bool SchedulerStateMachine::ShouldActivatePendingTree() const {
  // There is nothing to activate.
  if (!has_pending_tree_)
    return false;

  // We should not activate a second tree before drawing the first one.
  // Even if we need to force activation of the pending tree, we should abort
  // drawing the active tree first.
  if (active_tree_needs_first_draw_)
    return false;

  // If we want to force activation, do so ASAP.
  if (PendingActivationsShouldBeForced())
    return true;

  // At this point, only activate if we are ready to activate.
  return pending_tree_is_ready_for_activation_;
}

bool SchedulerStateMachine::ShouldAnimate() const {
  // Do not animate too many times in a single frame.
  if (animate_funnel_)
    return false;

  // Don't animate if we are waiting on the first commit after a surface.
  if (output_surface_state_ != OUTPUT_SURFACE_ACTIVE)
    return false;

  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING &&
      begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)
    return false;

  return needs_redraw_ || needs_animate_;
}

bool SchedulerStateMachine::CouldSendBeginMainFrame() const {
  if (!needs_commit_)
    return false;

  // We can not perform commits if we are not visible.
  if (!visible_)
    return false;

  // Do not make a new commits when it is deferred.
  if (defer_commits_)
    return false;

  return true;
}

bool SchedulerStateMachine::SendingBeginMainFrameMightCauseDeadlock() const {
  // NPAPI is the only case where the UI thread makes synchronous calls to the
  // Renderer main thread. During that synchronous call, we may not get a
  // SwapAck for the UI thread, which may prevent BeginMainFrame's from
  // completing if there's enough back pressure. If the BeginMainFrame can't
  // make progress, the Renderer can't service the UI thread's synchronous call
  // and we have deadlock.
  // This returns true if there's too much backpressure to finish a commit
  // if we were to initiate a BeginMainFrame.
  return has_pending_tree_ && active_tree_needs_first_draw_ && SwapThrottled();
}

bool SchedulerStateMachine::ShouldSendBeginMainFrame() const {
  if (!CouldSendBeginMainFrame())
    return false;

  // Do not send begin main frame too many times in a single frame or before
  // the first BeginFrame.
  if (send_begin_main_frame_funnel_)
    return false;

  // Only send BeginMainFrame when there isn't another commit pending already.
  // Other parts of the state machine indirectly defer the BeginMainFrame
  // by transitioning to WAITING commit states rather than going
  // immediately to IDLE.
  if (commit_state_ != COMMIT_STATE_IDLE)
    return false;

  // Don't send BeginMainFrame early if we are prioritizing the active tree
  // because of impl_latency_takes_priority_.
  if (impl_latency_takes_priority_ &&
      (has_pending_tree_ || active_tree_needs_first_draw_)) {
    return false;
  }

  // We should not send BeginMainFrame while we are in the idle state since we
  // might have new user input arriving soon. It's okay to send BeginMainFrame
  // for the synchronous compositor because the main thread is always high
  // latency in that case.
  // TODO(brianderson): Allow sending BeginMainFrame while idle when the main
  // thread isn't consuming user input for non-synchronous compositor.
  if (!settings_.using_synchronous_renderer_compositor &&
      begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_IDLE) {
    return false;
  }

  // We need a new commit for the forced redraw. This honors the
  // single commit per interval because the result will be swapped to screen.
  // TODO(brianderson): Remove this or move it below the
  // SendingBeginMainFrameMightCauseDeadlock check since  we want to avoid
  // ever returning true from this method if we might cause deadlock.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_COMMIT)
    return true;

  // We shouldn't normally accept commits if there isn't an OutputSurface.
  if (!HasInitializedOutputSurface())
    return false;

  // Make sure the BeginMainFrame can finish eventually if we start it.
  if (SendingBeginMainFrameMightCauseDeadlock())
    return false;

  if (!settings_.main_frame_while_swap_throttled_enabled) {
    // SwapAck throttle the BeginMainFrames unless we just swapped to
    // potentially improve impl-thread latency over main-thread throughput.
    // TODO(brianderson): Remove this restriction to improve throughput or
    // make it conditional on impl_latency_takes_priority_.
    bool just_swapped_in_deadline =
        begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE &&
        did_perform_swap_in_last_draw_;
    if (SwapThrottled() && !just_swapped_in_deadline)
      return false;
  }

  if (skip_next_begin_main_frame_to_reduce_latency_)
    return false;

  return true;
}

bool SchedulerStateMachine::ShouldCommit() const {
  if (commit_state_ != COMMIT_STATE_READY_TO_COMMIT)
    return false;

  // We must not finish the commit until the pending tree is free.
  if (has_pending_tree_) {
    DCHECK(settings_.main_frame_before_activation_enabled);
    return false;
  }

  // If we only have an active tree, it is incorrect to replace it
  // before we've drawn it.
  DCHECK_IMPLIES(settings_.commit_to_active_tree,
                 !active_tree_needs_first_draw_);

  return true;
}

bool SchedulerStateMachine::ShouldPrepareTiles() const {
  // PrepareTiles only really needs to be called immediately after commit
  // and then periodically after that. Use a funnel to make sure we average
  // one PrepareTiles per BeginImplFrame in the long run.
  if (prepare_tiles_funnel_ > 0)
    return false;

  // Limiting to once per-frame is not enough, since we only want to
  // prepare tiles _after_ draws.
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)
    return false;

  return needs_prepare_tiles_;
}

bool SchedulerStateMachine::ShouldInvalidateOutputSurface() const {
  // Do not invalidate too many times in a frame.
  if (invalidate_output_surface_funnel_)
    return false;

  // Only the synchronous compositor requires invalidations.
  if (!settings_.using_synchronous_renderer_compositor)
    return false;

  // Invalidations are only performed inside a BeginFrame.
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING)
    return false;

  // TODO(sunnyps): needs_prepare_tiles_ is needed here because PrepareTiles is
  // called only inside the deadline / draw phase. We could remove this if we
  // allowed PrepareTiles to happen in OnBeginImplFrame.
  return needs_redraw_ || needs_prepare_tiles_;
}

SchedulerStateMachine::Action SchedulerStateMachine::NextAction() const {
  if (ShouldActivatePendingTree())
    return ACTION_ACTIVATE_SYNC_TREE;
  if (ShouldCommit())
    return ACTION_COMMIT;
  if (ShouldAnimate())
    return ACTION_ANIMATE;
  if (ShouldDraw()) {
    if (PendingDrawsShouldBeAborted())
      return ACTION_DRAW_AND_SWAP_ABORT;
    else if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
      return ACTION_DRAW_AND_SWAP_FORCED;
    else
      return ACTION_DRAW_AND_SWAP_IF_POSSIBLE;
  }
  if (ShouldPrepareTiles())
    return ACTION_PREPARE_TILES;
  if (ShouldSendBeginMainFrame())
    return ACTION_SEND_BEGIN_MAIN_FRAME;
  if (ShouldInvalidateOutputSurface())
    return ACTION_INVALIDATE_OUTPUT_SURFACE;
  if (ShouldBeginOutputSurfaceCreation())
    return ACTION_BEGIN_OUTPUT_SURFACE_CREATION;
  return ACTION_NONE;
}

void SchedulerStateMachine::UpdateState(Action action) {
  switch (action) {
    case ACTION_NONE:
      return;

    case ACTION_ACTIVATE_SYNC_TREE:
      UpdateStateOnActivation();
      return;

    case ACTION_ANIMATE:
      UpdateStateOnAnimate();
      return;

    case ACTION_SEND_BEGIN_MAIN_FRAME:
      UpdateStateOnSendBeginMainFrame();
      return;

    case ACTION_COMMIT: {
      bool commit_has_no_updates = false;
      UpdateStateOnCommit(commit_has_no_updates);
      return;
    }

    case ACTION_DRAW_AND_SWAP_FORCED:
    case ACTION_DRAW_AND_SWAP_IF_POSSIBLE: {
      bool did_request_swap = true;
      UpdateStateOnDraw(did_request_swap);
      return;
    }

    case ACTION_DRAW_AND_SWAP_ABORT: {
      bool did_request_swap = false;
      UpdateStateOnDraw(did_request_swap);
      return;
    }

    case ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
      UpdateStateOnBeginOutputSurfaceCreation();
      return;

    case ACTION_PREPARE_TILES:
      UpdateStateOnPrepareTiles();
      return;

    case ACTION_INVALIDATE_OUTPUT_SURFACE:
      UpdateStateOnInvalidateOutputSurface();
      return;
  }
}

void SchedulerStateMachine::UpdateStateOnAnimate() {
  DCHECK(!animate_funnel_);
  last_frame_number_animate_performed_ = current_frame_number_;
  animate_funnel_ = true;
  needs_animate_ = false;
  // TODO(skyostil): Instead of assuming this, require the client to tell us.
  SetNeedsRedraw();
}

void SchedulerStateMachine::UpdateStateOnSendBeginMainFrame() {
  DCHECK(!has_pending_tree_ || settings_.main_frame_before_activation_enabled);
  DCHECK(visible_);
  DCHECK(!send_begin_main_frame_funnel_);
  commit_state_ = COMMIT_STATE_BEGIN_MAIN_FRAME_SENT;
  needs_commit_ = false;
  send_begin_main_frame_funnel_ = true;
  last_frame_number_begin_main_frame_sent_ = current_frame_number_;
}

void SchedulerStateMachine::UpdateStateOnCommit(bool commit_has_no_updates) {
  commit_count_++;

  // Animate after commit even if we've already animated.
  if (!commit_has_no_updates)
    animate_funnel_ = false;

  if (commit_has_no_updates || settings_.main_frame_before_activation_enabled) {
    commit_state_ = COMMIT_STATE_IDLE;
  } else {
    commit_state_ = COMMIT_STATE_WAITING_FOR_ACTIVATION;
  }

  // If the commit was aborted, then there is no pending tree.
  has_pending_tree_ = !commit_has_no_updates;

  // Update state related to forced draws.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_COMMIT) {
    forced_redraw_state_ = has_pending_tree_
                               ? FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION
                               : FORCED_REDRAW_STATE_WAITING_FOR_DRAW;
  }

  // Update the output surface state.
  DCHECK_NE(output_surface_state_, OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION);
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT) {
    if (has_pending_tree_) {
      output_surface_state_ = OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION;
    } else {
      output_surface_state_ = OUTPUT_SURFACE_ACTIVE;
      needs_redraw_ = true;
    }
  }

  // Update state if we have a new active tree to draw, or if the active tree
  // was unchanged but we need to do a forced draw.
  if (!has_pending_tree_ &&
      (!commit_has_no_updates ||
       forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)) {
    needs_redraw_ = true;
    active_tree_needs_first_draw_ = true;
  }

  // This post-commit work is common to both completed and aborted commits.
  pending_tree_is_ready_for_activation_ = false;

  if (continuous_painting_)
    needs_commit_ = true;
  last_commit_had_no_updates_ = commit_has_no_updates;
}

void SchedulerStateMachine::UpdateStateOnActivation() {
  if (commit_state_ == COMMIT_STATE_WAITING_FOR_ACTIVATION) {
    commit_state_ = settings_.commit_to_active_tree
                        ? COMMIT_STATE_WAITING_FOR_DRAW
                        : COMMIT_STATE_IDLE;
  }

  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION)
    output_surface_state_ = OUTPUT_SURFACE_ACTIVE;

  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION)
    forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_DRAW;

  has_pending_tree_ = false;
  pending_tree_is_ready_for_activation_ = false;
  active_tree_needs_first_draw_ = true;
  needs_redraw_ = true;
}

void SchedulerStateMachine::UpdateStateOnDraw(bool did_request_swap) {
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
    forced_redraw_state_ = FORCED_REDRAW_STATE_IDLE;

  if (commit_state_ == COMMIT_STATE_WAITING_FOR_DRAW)
    commit_state_ = COMMIT_STATE_IDLE;

  needs_redraw_ = false;
  active_tree_needs_first_draw_ = false;

  if (did_request_swap) {
    DCHECK(!request_swap_funnel_);
    request_swap_funnel_ = true;
    did_request_swap_in_last_frame_ = true;
    last_frame_number_swap_requested_ = current_frame_number_;
  }
}

void SchedulerStateMachine::UpdateStateOnPrepareTiles() {
  needs_prepare_tiles_ = false;
}

void SchedulerStateMachine::UpdateStateOnBeginOutputSurfaceCreation() {
  DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_LOST);
  output_surface_state_ = OUTPUT_SURFACE_CREATING;

  // The following DCHECKs make sure we are in the proper quiescent state.
  // The pipeline should be flushed entirely before we start output
  // surface creation to avoid complicated corner cases.
  DCHECK_EQ(commit_state_, COMMIT_STATE_IDLE);
  DCHECK(!has_pending_tree_);
  DCHECK(!active_tree_needs_first_draw_);
}

void SchedulerStateMachine::UpdateStateOnInvalidateOutputSurface() {
  DCHECK(!invalidate_output_surface_funnel_);
  invalidate_output_surface_funnel_ = true;
  last_frame_number_invalidate_output_surface_performed_ =
      current_frame_number_;

  // The synchronous compositor makes no guarantees about a draw coming in after
  // an invalidate so clear any flags that would cause the compositor's pipeline
  // to stall.
  active_tree_needs_first_draw_ = false;  // blocks commit if true
}

void SchedulerStateMachine::SetSkipNextBeginMainFrameToReduceLatency() {
  TRACE_EVENT_INSTANT0("cc",
                       "Scheduler: SkipNextBeginMainFrameToReduceLatency",
                       TRACE_EVENT_SCOPE_THREAD);
  skip_next_begin_main_frame_to_reduce_latency_ = true;
}

bool SchedulerStateMachine::BeginFrameRequiredForChildren() const {
  return children_need_begin_frames_;
}
bool SchedulerStateMachine::BeginFrameNeededForVideo() const {
  return video_needs_begin_frames_;
}

bool SchedulerStateMachine::BeginFrameNeeded() const {
  // We can't handle BeginFrames when output surface isn't initialized.
  // TODO(brianderson): Support output surface creation inside a BeginFrame.
  if (!HasInitializedOutputSurface())
    return false;

  // If we are not visible, we don't need BeginFrame messages.
  if (!visible_)
    return false;

  return (BeginFrameRequiredForAction() || BeginFrameRequiredForChildren() ||
          BeginFrameNeededForVideo() || ProactiveBeginFrameWanted());
}

void SchedulerStateMachine::SetChildrenNeedBeginFrames(
    bool children_need_begin_frames) {
  children_need_begin_frames_ = children_need_begin_frames;
}

void SchedulerStateMachine::SetVideoNeedsBeginFrames(
    bool video_needs_begin_frames) {
  video_needs_begin_frames_ = video_needs_begin_frames;
}

void SchedulerStateMachine::SetDeferCommits(bool defer_commits) {
  defer_commits_ = defer_commits;
}

// These are the cases where we require a BeginFrame message to make progress
// on requested actions.
bool SchedulerStateMachine::BeginFrameRequiredForAction() const {
  // The forced draw respects our normal draw scheduling, so we need to
  // request a BeginImplFrame for it.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
    return true;

  return needs_animate_ || needs_redraw_ || (needs_commit_ && !defer_commits_);
}

// These are cases where we are very likely want a BeginFrame message in the
// near future. Proactively requesting the BeginImplFrame helps hide the round
// trip latency of the SetNeedsBeginFrame request that has to go to the
// Browser.
// This includes things like drawing soon, but might not actually have a new
// frame to draw when we receive the next BeginImplFrame.
bool SchedulerStateMachine::ProactiveBeginFrameWanted() const {
  // Do not be proactive when invisible.
  if (!visible_)
    return false;

  // We should proactively request a BeginImplFrame if a commit is pending
  // because we will want to draw if the commit completes quickly. Do not
  // request frames when commits are disabled, because the frame requests will
  // not provide the needed commit (and will wake up the process when it could
  // stay idle).
  if ((commit_state_ != COMMIT_STATE_IDLE) && !defer_commits_)
    return true;

  // If the pending tree activates quickly, we'll want a BeginImplFrame soon
  // to draw the new active tree.
  if (has_pending_tree_)
    return true;

  // Changing priorities may allow us to activate (given the new priorities),
  // which may result in a new frame.
  if (needs_prepare_tiles_)
    return true;

  // If we just sent a swap request, it's likely that we are going to produce
  // another frame soon. This helps avoid negative glitches in our
  // SetNeedsBeginFrame requests, which may propagate to the BeginImplFrame
  // provider and get sampled at an inopportune time, delaying the next
  // BeginImplFrame.
  if (did_request_swap_in_last_frame_)
    return true;

  // If the last commit was aborted because of early out (no updates), we should
  // still want a begin frame in case there is a commit coming again.
  if (last_commit_had_no_updates_)
    return true;

  return false;
}

void SchedulerStateMachine::OnBeginImplFrame() {
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING;
  current_frame_number_++;

  last_commit_had_no_updates_ = false;
  did_request_swap_in_last_frame_ = false;

  // Clear funnels for any actions we perform during the frame.
  animate_funnel_ = false;
  send_begin_main_frame_funnel_ = false;
  invalidate_output_surface_funnel_ = false;

  // "Drain" the PrepareTiles funnel.
  if (prepare_tiles_funnel_ > 0)
    prepare_tiles_funnel_--;
}

void SchedulerStateMachine::OnBeginImplFrameDeadlinePending() {
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME;
}

void SchedulerStateMachine::OnBeginImplFrameDeadline() {
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE;

  did_perform_swap_in_last_draw_ = false;

  // Clear funnels for any actions we perform during the deadline.
  request_swap_funnel_ = false;

  // Allow one PrepareTiles per draw for synchronous compositor.
  if (settings_.using_synchronous_renderer_compositor) {
    if (prepare_tiles_funnel_ > 0)
      prepare_tiles_funnel_--;
  }
}

void SchedulerStateMachine::OnBeginImplFrameIdle() {
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_IDLE;

  skip_next_begin_main_frame_to_reduce_latency_ = false;

  // If we're entering a state where we won't get BeginFrames set all the
  // funnels so that we don't perform any actions that we shouldn't.
  if (!BeginFrameNeeded())
    send_begin_main_frame_funnel_ = true;
}

SchedulerStateMachine::BeginImplFrameDeadlineMode
SchedulerStateMachine::CurrentBeginImplFrameDeadlineMode() const {
  if (settings_.using_synchronous_renderer_compositor) {
    // No deadline for synchronous compositor.
    return BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE;
  } else if (wait_for_active_tree_ready_to_draw_) {
    // When we are waiting for ready to draw signal, we do not wait to post a
    // deadline yet.
    return BEGIN_IMPL_FRAME_DEADLINE_MODE_BLOCKED_ON_READY_TO_DRAW;
  } else if (ShouldTriggerBeginImplFrameDeadlineImmediately()) {
    return BEGIN_IMPL_FRAME_DEADLINE_MODE_IMMEDIATE;
  } else if (needs_redraw_ && !SwapThrottled()) {
    // We have an animation or fast input path on the impl thread that wants
    // to draw, so don't wait too long for a new active tree.
    // If we are swap throttled we should wait until we are unblocked.
    return BEGIN_IMPL_FRAME_DEADLINE_MODE_REGULAR;
  } else {
    // The impl thread doesn't have anything it wants to draw and we are just
    // waiting for a new active tree or we are swap throttled. In short we are
    // blocked.
    return BEGIN_IMPL_FRAME_DEADLINE_MODE_LATE;
  }
}

bool SchedulerStateMachine::ShouldTriggerBeginImplFrameDeadlineImmediately()
    const {
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME)
    return false;

  // If we just forced activation, we should end the deadline right now.
  if (PendingActivationsShouldBeForced() && !has_pending_tree_)
    return true;

  // SwapAck throttle the deadline since we wont draw and swap anyway.
  if (SwapThrottled())
    return false;

  if (active_tree_needs_first_draw_)
    return true;

  if (!needs_redraw_)
    return false;

  // This is used to prioritize impl-thread draws when the main thread isn't
  // producing anything, e.g., after an aborted commit. We also check that we
  // don't have a pending tree -- otherwise we should give it a chance to
  // activate.
  // TODO(skyostil): Revisit this when we have more accurate deadline estimates.
  if (commit_state_ == COMMIT_STATE_IDLE && !has_pending_tree_)
    return true;

  // Prioritize impl-thread draws in impl_latency_takes_priority_ mode.
  if (impl_latency_takes_priority_)
    return true;

  return false;
}

bool SchedulerStateMachine::MainThreadIsInHighLatencyMode() const {
  // If a commit is pending before the previous commit has been drawn, we
  // are definitely in a high latency mode.
  if (CommitPending() && (active_tree_needs_first_draw_ || has_pending_tree_))
    return true;

  // If we just sent a BeginMainFrame and haven't hit the deadline yet, the main
  // thread is in a low latency mode.
  if (send_begin_main_frame_funnel_ &&
      (begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING ||
       begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME))
    return false;

  // If there's a commit in progress it must either be from the previous frame
  // or it started after the impl thread's deadline. In either case the main
  // thread is in high latency mode.
  if (CommitPending())
    return true;

  // Similarly, if there's a pending tree the main thread is in high latency
  // mode, because either
  //   it's from the previous frame
  // or
  //   we're currently drawing the active tree and the pending tree will thus
  //   only be drawn in the next frame.
  if (has_pending_tree_)
    return true;

  if (begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE) {
    // Even if there's a new active tree to draw at the deadline or we've just
    // swapped it, it may have been triggered by a previous BeginImplFrame, in
    // which case the main thread is in a high latency mode.
    return (active_tree_needs_first_draw_ || did_perform_swap_in_last_draw_) &&
           !send_begin_main_frame_funnel_;
  }

  // If the active tree needs its first draw in any other state, we know the
  // main thread is in a high latency mode.
  return active_tree_needs_first_draw_;
}

bool SchedulerStateMachine::SwapThrottled() const {
  return pending_swaps_ >= max_pending_swaps_;
}

void SchedulerStateMachine::SetVisible(bool visible) {
  visible_ = visible;
  // TODO(sunnyps): Change the funnel to a bool to avoid hacks like this.
  prepare_tiles_funnel_ = 0;
}

void SchedulerStateMachine::SetCanDraw(bool can_draw) { can_draw_ = can_draw; }

void SchedulerStateMachine::SetNeedsRedraw() { needs_redraw_ = true; }

void SchedulerStateMachine::SetNeedsAnimate() {
  needs_animate_ = true;
}

void SchedulerStateMachine::SetWaitForReadyToDraw() {
  wait_for_active_tree_ready_to_draw_ = true;
}

bool SchedulerStateMachine::OnlyImplSideUpdatesExpected() const {
  bool has_impl_updates = needs_redraw_ || needs_animate_;
  bool main_updates_expected =
      needs_commit_ || commit_state_ != COMMIT_STATE_IDLE || has_pending_tree_;
  return has_impl_updates && !main_updates_expected;
}

void SchedulerStateMachine::SetNeedsPrepareTiles() {
  if (!needs_prepare_tiles_) {
    TRACE_EVENT0("cc", "SchedulerStateMachine::SetNeedsPrepareTiles");
    needs_prepare_tiles_ = true;
  }
}

void SchedulerStateMachine::SetMaxSwapsPending(int max) {
  max_pending_swaps_ = max;
}

void SchedulerStateMachine::DidSwapBuffers() {
  pending_swaps_++;
  swaps_with_current_output_surface_++;

  DCHECK_LE(pending_swaps_, max_pending_swaps_);

  did_perform_swap_in_last_draw_ = true;
  last_frame_number_swap_performed_ = current_frame_number_;
}

void SchedulerStateMachine::DidSwapBuffersComplete() {
  pending_swaps_--;
}

void SchedulerStateMachine::SetImplLatencyTakesPriority(
    bool impl_latency_takes_priority) {
  impl_latency_takes_priority_ = impl_latency_takes_priority;
}

void SchedulerStateMachine::DidDrawIfPossibleCompleted(DrawResult result) {
  switch (result) {
    case INVALID_RESULT:
      NOTREACHED() << "Uninitialized DrawResult.";
      break;
    case DRAW_ABORTED_CANT_DRAW:
    case DRAW_ABORTED_CONTEXT_LOST:
      NOTREACHED() << "Invalid return value from DrawAndSwapIfPossible:"
                   << result;
      break;
    case DRAW_SUCCESS:
      consecutive_checkerboard_animations_ = 0;
      forced_redraw_state_ = FORCED_REDRAW_STATE_IDLE;
      break;
    case DRAW_ABORTED_CHECKERBOARD_ANIMATIONS:
      needs_redraw_ = true;

      // If we're already in the middle of a redraw, we don't need to
      // restart it.
      if (forced_redraw_state_ != FORCED_REDRAW_STATE_IDLE)
        return;

      needs_commit_ = true;
      consecutive_checkerboard_animations_++;
      if (settings_.timeout_and_draw_when_animation_checkerboards &&
          consecutive_checkerboard_animations_ >=
              settings_.maximum_number_of_failed_draws_before_draw_is_forced) {
        consecutive_checkerboard_animations_ = 0;
        // We need to force a draw, but it doesn't make sense to do this until
        // we've committed and have new textures.
        forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_COMMIT;
      }
      break;
    case DRAW_ABORTED_MISSING_HIGH_RES_CONTENT:
      // It's not clear whether this missing content is because of missing
      // pictures (which requires a commit) or because of memory pressure
      // removing textures (which might not).  To be safe, request a commit
      // anyway.
      needs_commit_ = true;
      break;
  }
}

void SchedulerStateMachine::SetNeedsCommit() {
  needs_commit_ = true;
}

void SchedulerStateMachine::NotifyReadyToCommit() {
  DCHECK(commit_state_ == COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED)
      << AsValue()->ToString();
  commit_state_ = COMMIT_STATE_READY_TO_COMMIT;
  // In commit_to_active_tree mode, commit should happen right after
  // BeginFrame, meaning when this function is called, next action should be
  // commit.
  if (settings_.commit_to_active_tree)
    DCHECK(ShouldCommit());
}

void SchedulerStateMachine::BeginMainFrameAborted(CommitEarlyOutReason reason) {
  DCHECK_EQ(commit_state_, COMMIT_STATE_BEGIN_MAIN_FRAME_SENT);
  switch (reason) {
    case CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST:
    case CommitEarlyOutReason::ABORTED_NOT_VISIBLE:
    case CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT:
      commit_state_ = COMMIT_STATE_IDLE;
      SetNeedsCommit();
      return;
    case CommitEarlyOutReason::FINISHED_NO_UPDATES:
      bool commit_has_no_updates = true;
      UpdateStateOnCommit(commit_has_no_updates);
      return;
  }
}

void SchedulerStateMachine::DidPrepareTiles() {
  needs_prepare_tiles_ = false;
  // "Fill" the PrepareTiles funnel.
  prepare_tiles_funnel_++;
}

void SchedulerStateMachine::DidLoseOutputSurface() {
  if (output_surface_state_ == OUTPUT_SURFACE_LOST ||
      output_surface_state_ == OUTPUT_SURFACE_CREATING)
    return;
  output_surface_state_ = OUTPUT_SURFACE_LOST;
  needs_redraw_ = false;
  wait_for_active_tree_ready_to_draw_ = false;
}

void SchedulerStateMachine::NotifyReadyToActivate() {
  if (has_pending_tree_)
    pending_tree_is_ready_for_activation_ = true;
}

void SchedulerStateMachine::NotifyReadyToDraw() {
  wait_for_active_tree_ready_to_draw_ = false;
}

void SchedulerStateMachine::DidCreateAndInitializeOutputSurface() {
  DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_CREATING);
  output_surface_state_ = OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT;

  if (did_create_and_initialize_first_output_surface_) {
    // TODO(boliu): See if we can remove this when impl-side painting is always
    // on. Does anything on the main thread need to update after recreate?
    needs_commit_ = true;
  }
  did_create_and_initialize_first_output_surface_ = true;
  pending_swaps_ = 0;
  swaps_with_current_output_surface_ = 0;
}

void SchedulerStateMachine::NotifyBeginMainFrameStarted() {
  DCHECK_EQ(commit_state_, COMMIT_STATE_BEGIN_MAIN_FRAME_SENT);
  commit_state_ = COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED;
}

bool SchedulerStateMachine::HasInitializedOutputSurface() const {
  switch (output_surface_state_) {
    case OUTPUT_SURFACE_LOST:
    case OUTPUT_SURFACE_CREATING:
      return false;

    case OUTPUT_SURFACE_ACTIVE:
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT:
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION:
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace cc
