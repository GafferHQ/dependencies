// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/thread_proxy.h"

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/input/input_handler.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/output/swap_promise.h"
#include "cc/quads/draw_quad.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/compositor_timing_history.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scoped_abort_remaining_swap_promises.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace cc {

namespace {

// Measured in seconds.
const double kSmoothnessTakesPriorityExpirationDelay = 0.25;

unsigned int nextBeginFrameId = 0;

}  // namespace

struct ThreadProxy::SchedulerStateRequest {
  CompletionEvent completion;
  scoped_ptr<base::Value> state;
};

scoped_ptr<Proxy> ThreadProxy::Create(
    LayerTreeHost* layer_tree_host,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  return make_scoped_ptr(new ThreadProxy(layer_tree_host,
                                         main_task_runner,
                                         impl_task_runner,
                                         external_begin_frame_source.Pass()));
}

ThreadProxy::ThreadProxy(
    LayerTreeHost* layer_tree_host,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    scoped_ptr<BeginFrameSource> external_begin_frame_source)
    : Proxy(main_task_runner, impl_task_runner),
      main_thread_only_vars_unsafe_(this, layer_tree_host->id()),
      main_thread_or_blocked_vars_unsafe_(layer_tree_host),
      compositor_thread_vars_unsafe_(
          this,
          layer_tree_host->id(),
          layer_tree_host->rendering_stats_instrumentation(),
          external_begin_frame_source.Pass()) {
  TRACE_EVENT0("cc", "ThreadProxy::ThreadProxy");
  DCHECK(IsMainThread());
  DCHECK(this->layer_tree_host());
}

ThreadProxy::MainThreadOnly::MainThreadOnly(ThreadProxy* proxy,
                                            int layer_tree_host_id)
    : layer_tree_host_id(layer_tree_host_id),
      animate_requested(false),
      commit_requested(false),
      commit_request_sent_to_impl_thread(false),
      started(false),
      prepare_tiles_pending(false),
      can_cancel_commit(true),
      defer_commits(false),
      weak_factory(proxy) {
}

ThreadProxy::MainThreadOnly::~MainThreadOnly() {}

ThreadProxy::MainThreadOrBlockedMainThread::MainThreadOrBlockedMainThread(
    LayerTreeHost* host)
    : layer_tree_host(host),
      commit_waits_for_activation(false),
      main_thread_inside_commit(false) {}

ThreadProxy::MainThreadOrBlockedMainThread::~MainThreadOrBlockedMainThread() {}

ThreadProxy::CompositorThreadOnly::CompositorThreadOnly(
    ThreadProxy* proxy,
    int layer_tree_host_id,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    scoped_ptr<BeginFrameSource> external_begin_frame_source)
    : layer_tree_host_id(layer_tree_host_id),
      commit_completion_event(NULL),
      completion_event_for_commit_held_on_tree_activation(NULL),
      next_frame_is_newly_committed_frame(false),
      inside_draw(false),
      input_throttled_until_commit(false),
      smoothness_priority_expiration_notifier(
          proxy->ImplThreadTaskRunner(),
          base::Bind(&ThreadProxy::RenewTreePriority, base::Unretained(proxy)),
          base::TimeDelta::FromMilliseconds(
              kSmoothnessTakesPriorityExpirationDelay * 1000)),
      external_begin_frame_source(external_begin_frame_source.Pass()),
      rendering_stats_instrumentation(rendering_stats_instrumentation),
      weak_factory(proxy) {
}

ThreadProxy::CompositorThreadOnly::~CompositorThreadOnly() {}

ThreadProxy::~ThreadProxy() {
  TRACE_EVENT0("cc", "ThreadProxy::~ThreadProxy");
  DCHECK(IsMainThread());
  DCHECK(!main().started);
}

void ThreadProxy::FinishAllRendering() {
  DCHECK(Proxy::IsMainThread());
  DCHECK(!main().defer_commits);

  // Make sure all GL drawing is finished on the impl thread.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::FinishAllRenderingOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

bool ThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return main().started;
}

bool ThreadProxy::CommitToActiveTree() const {
  // With ThreadProxy, we use a pending tree and activate it once it's ready to
  // draw to allow input to modify the active tree and draw during raster.
  return false;
}

void ThreadProxy::SetLayerTreeHostClientReady() {
  TRACE_EVENT0("cc", "ThreadProxy::SetLayerTreeHostClientReady");
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetLayerTreeHostClientReadyOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::SetLayerTreeHostClientReadyOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetLayerTreeHostClientReadyOnImplThread");
  impl().scheduler->SetCanStart();
}

void ThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "ThreadProxy::SetVisible", "visible", visible);
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);

  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetVisibleOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion,
                 visible));
  completion.Wait();
}

void ThreadProxy::SetVisibleOnImplThread(CompletionEvent* completion,
                                         bool visible) {
  TRACE_EVENT1("cc", "ThreadProxy::SetVisibleOnImplThread", "visible", visible);
  impl().layer_tree_host_impl->SetVisible(visible);
  impl().scheduler->SetVisible(visible);
  completion->Signal();
}

void ThreadProxy::SetThrottleFrameProduction(bool throttle) {
  TRACE_EVENT1("cc", "ThreadProxy::SetThrottleFrameProduction", "throttle",
               throttle);
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetThrottleFrameProductionOnImplThread,
                 impl_thread_weak_ptr_, throttle));
}

void ThreadProxy::SetThrottleFrameProductionOnImplThread(bool throttle) {
  TRACE_EVENT1("cc", "ThreadProxy::SetThrottleFrameProductionOnImplThread",
               "throttle", throttle);
  impl().scheduler->SetThrottleFrameProduction(throttle);
}

void ThreadProxy::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::DidLoseOutputSurface");
  DCHECK(IsMainThread());
  layer_tree_host()->DidLoseOutputSurface();
}

void ThreadProxy::RequestNewOutputSurface() {
  DCHECK(IsMainThread());
  layer_tree_host()->RequestNewOutputSurface();
}

void ThreadProxy::SetOutputSurface(scoped_ptr<OutputSurface> output_surface) {
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::InitializeOutputSurfaceOnImplThread,
                 impl_thread_weak_ptr_, base::Passed(&output_surface)));
}

void ThreadProxy::DidInitializeOutputSurface(
    bool success,
    const RendererCapabilities& capabilities) {
  TRACE_EVENT0("cc", "ThreadProxy::DidInitializeOutputSurface");
  DCHECK(IsMainThread());

  if (!success) {
    layer_tree_host()->DidFailToInitializeOutputSurface();
    return;
  }
  main().renderer_capabilities_main_thread_copy = capabilities;
  layer_tree_host()->DidInitializeOutputSurface();
}

void ThreadProxy::SetRendererCapabilitiesMainThreadCopy(
    const RendererCapabilities& capabilities) {
  main().renderer_capabilities_main_thread_copy = capabilities;
}

void ThreadProxy::SendCommitRequestToImplThreadIfNeeded() {
  DCHECK(IsMainThread());
  if (main().commit_request_sent_to_impl_thread)
    return;
  main().commit_request_sent_to_impl_thread = true;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetNeedsCommitOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::DidCompletePageScaleAnimation() {
  DCHECK(IsMainThread());
  layer_tree_host()->DidCompletePageScaleAnimation();
}

const RendererCapabilities& ThreadProxy::GetRendererCapabilities() const {
  DCHECK(IsMainThread());
  DCHECK(!layer_tree_host()->output_surface_lost());
  return main().renderer_capabilities_main_thread_copy;
}

void ThreadProxy::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  if (main().animate_requested)
    return;

  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsAnimate");
  main().animate_requested = true;
  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::SetNeedsUpdateLayers() {
  DCHECK(IsMainThread());

  if (main().commit_request_sent_to_impl_thread)
    return;
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsUpdateLayers");

  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::SetNeedsCommit() {
  DCHECK(IsMainThread());
  // Unconditionally set here to handle SetNeedsCommit calls during a commit.
  main().can_cancel_commit = false;

  if (main().commit_requested)
    return;
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommit");
  main().commit_requested = true;

  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::UpdateRendererCapabilitiesOnImplThread() {
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetRendererCapabilitiesMainThreadCopy,
                 main_thread_weak_ptr_,
                 impl()
                     .layer_tree_host_impl->GetRendererCapabilities()
                     .MainThreadCapabilities()));
}

void ThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::DidLoseOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::DidLoseOutputSurface, main_thread_weak_ptr_));
  impl().scheduler->DidLoseOutputSurface();
}

void ThreadProxy::CommitVSyncParameters(base::TimeTicks timebase,
                                        base::TimeDelta interval) {
  impl().scheduler->CommitVSyncParameters(timebase, interval);
}

void ThreadProxy::SetEstimatedParentDrawTime(base::TimeDelta draw_time) {
  impl().scheduler->SetEstimatedParentDrawTime(draw_time);
}

void ThreadProxy::SetMaxSwapsPendingOnImplThread(int max) {
  impl().scheduler->SetMaxSwapsPending(max);
}

void ThreadProxy::DidSwapBuffersOnImplThread() {
  impl().scheduler->DidSwapBuffers();
}

void ThreadProxy::DidSwapBuffersCompleteOnImplThread() {
  TRACE_EVENT0("cc,benchmark",
               "ThreadProxy::DidSwapBuffersCompleteOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->DidSwapBuffersComplete();
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::DidCompleteSwapBuffers, main_thread_weak_ptr_));
}

void ThreadProxy::WillBeginImplFrame(const BeginFrameArgs& args) {
  impl().layer_tree_host_impl->WillBeginImplFrame(args);
  if (impl().last_processed_begin_main_frame_args.IsValid()) {
    // Last processed begin main frame args records the frame args that we sent
    // to the main thread for the last frame that we've processed. If that is
    // set, that means the current frame is one past the frame in which we've
    // finished the processing.
    impl().layer_tree_host_impl->RecordMainFrameTiming(
        impl().last_processed_begin_main_frame_args,
        impl().layer_tree_host_impl->CurrentBeginFrameArgs());
    impl().last_processed_begin_main_frame_args = BeginFrameArgs();
  }
}

void ThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1(
      "cc", "ThreadProxy::OnCanDrawStateChanged", "can_draw", can_draw);
  DCHECK(IsImplThread());
  impl().scheduler->SetCanDraw(can_draw);
}

void ThreadProxy::NotifyReadyToActivate() {
  TRACE_EVENT0("cc", "ThreadProxy::NotifyReadyToActivate");
  impl().scheduler->NotifyReadyToActivate();
}

void ThreadProxy::NotifyReadyToDraw() {
  TRACE_EVENT0("cc", "ThreadProxy::NotifyReadyToDraw");
  impl().scheduler->NotifyReadyToDraw();
}

void ThreadProxy::SetNeedsCommitOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommitOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsCommit();
}

void ThreadProxy::SetVideoNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("cc", "ThreadProxy::SetVideoNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  DCHECK(IsImplThread());
  // In tests the layer tree is destroyed after the scheduler is.
  if (impl().scheduler)
    impl().scheduler->SetVideoNeedsBeginFrames(needs_begin_frames);
}

void ThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events) {
  TRACE_EVENT0("cc",
               "ThreadProxy::PostAnimationEventsToMainThreadOnImplThread");
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetAnimationEvents,
                 main_thread_weak_ptr_,
                 base::Passed(&events)));
}

bool ThreadProxy::IsInsideDraw() { return impl().inside_draw; }

void ThreadProxy::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsRedraw");
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetNeedsRedrawRectOnImplThread,
                 impl_thread_weak_ptr_,
                 damage_rect));
}

void ThreadProxy::SetNextCommitWaitsForActivation() {
  DCHECK(IsMainThread());
  DCHECK(!blocked_main().main_thread_inside_commit);
  blocked_main().commit_waits_for_activation = true;
}

void ThreadProxy::SetDeferCommits(bool defer_commits) {
  DCHECK(IsMainThread());
  if (main().defer_commits == defer_commits)
    return;

  main().defer_commits = defer_commits;
  if (main().defer_commits)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ThreadProxy::SetDeferCommits", this);
  else
    TRACE_EVENT_ASYNC_END0("cc", "ThreadProxy::SetDeferCommits", this);

  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetDeferCommitsOnImplThread,
                 impl_thread_weak_ptr_,
                 defer_commits));
}

void ThreadProxy::SetDeferCommitsOnImplThread(bool defer_commits) const {
  DCHECK(IsImplThread());
  impl().scheduler->SetDeferCommits(defer_commits);
}

bool ThreadProxy::CommitRequested() const {
  DCHECK(IsMainThread());
  return main().commit_requested;
}

bool ThreadProxy::BeginMainFrameRequested() const {
  DCHECK(IsMainThread());
  return main().commit_request_sent_to_impl_thread;
}

void ThreadProxy::SetNeedsRedrawOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsRedrawOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsRedraw();
}

void ThreadProxy::SetNeedsAnimateOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsAnimateOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsAnimate();
}

void ThreadProxy::SetNeedsPrepareTilesOnImplThread() {
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsPrepareTiles();
}

void ThreadProxy::SetNeedsRedrawRectOnImplThread(const gfx::Rect& damage_rect) {
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->SetViewportDamage(damage_rect);
  SetNeedsRedrawOnImplThread();
}

void ThreadProxy::MainThreadHasStoppedFlinging() {
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::MainThreadHasStoppedFlingingOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::MainThreadHasStoppedFlingingOnImplThread() {
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->MainThreadHasStoppedFlinging();
}

void ThreadProxy::NotifyInputThrottledUntilCommit() {
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetInputThrottledUntilCommitOnImplThread,
                 impl_thread_weak_ptr_,
                 true));
}

void ThreadProxy::SetInputThrottledUntilCommitOnImplThread(bool is_throttled) {
  DCHECK(IsImplThread());
  if (is_throttled == impl().input_throttled_until_commit)
    return;
  impl().input_throttled_until_commit = is_throttled;
  RenewTreePriority();
}

LayerTreeHost* ThreadProxy::layer_tree_host() {
  return blocked_main().layer_tree_host;
}

const LayerTreeHost* ThreadProxy::layer_tree_host() const {
  return blocked_main().layer_tree_host;
}

ThreadProxy::MainThreadOnly& ThreadProxy::main() {
  DCHECK(IsMainThread());
  return main_thread_only_vars_unsafe_;
}
const ThreadProxy::MainThreadOnly& ThreadProxy::main() const {
  DCHECK(IsMainThread());
  return main_thread_only_vars_unsafe_;
}

ThreadProxy::MainThreadOrBlockedMainThread& ThreadProxy::blocked_main() {
  DCHECK(IsMainThread() || IsMainThreadBlocked());
  return main_thread_or_blocked_vars_unsafe_;
}

const ThreadProxy::MainThreadOrBlockedMainThread& ThreadProxy::blocked_main()
    const {
  DCHECK(IsMainThread() || IsMainThreadBlocked());
  return main_thread_or_blocked_vars_unsafe_;
}

ThreadProxy::CompositorThreadOnly& ThreadProxy::impl() {
  DCHECK(IsImplThread());
  return compositor_thread_vars_unsafe_;
}

const ThreadProxy::CompositorThreadOnly& ThreadProxy::impl() const {
  DCHECK(IsImplThread());
  return compositor_thread_vars_unsafe_;
}

void ThreadProxy::Start() {
  DCHECK(IsMainThread());
  DCHECK(Proxy::HasImplThread());

  // Create LayerTreeHostImpl.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::InitializeImplOnImplThread,
                 base::Unretained(this),
                 &completion));
  completion.Wait();

  main_thread_weak_ptr_ = main().weak_factory.GetWeakPtr();

  main().started = true;
}

void ThreadProxy::Stop() {
  TRACE_EVENT0("cc", "ThreadProxy::Stop");
  DCHECK(IsMainThread());
  DCHECK(main().started);

  // Synchronously finishes pending GL operations and deletes the impl.
  // The two steps are done as separate post tasks, so that tasks posted
  // by the GL implementation due to the Finish can be executed by the
  // renderer before shutting it down.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::FinishGLOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion));
    completion.Wait();
  }
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::LayerTreeHostClosedOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion));
    completion.Wait();
  }

  main().weak_factory.InvalidateWeakPtrs();
  blocked_main().layer_tree_host = NULL;
  main().started = false;
}

void ThreadProxy::ForceSerializeOnSwapBuffers() {
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

void ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread(
    CompletionEvent* completion) {
  if (impl().layer_tree_host_impl->renderer())
    impl().layer_tree_host_impl->renderer()->DoNoOp();
  completion->Signal();
}

bool ThreadProxy::SupportsImplScrolling() const {
  return true;
}

void ThreadProxy::SetDebugState(const LayerTreeDebugState& debug_state) {
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetDebugStateOnImplThread,
                 impl_thread_weak_ptr_,
                 debug_state));
}

void ThreadProxy::SetDebugStateOnImplThread(
    const LayerTreeDebugState& debug_state) {
  DCHECK(IsImplThread());
  impl().scheduler->SetContinuousPainting(debug_state.continuous_painting);
}

void ThreadProxy::FinishAllRenderingOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::FinishAllRenderingOnImplThread");
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->FinishAllRendering();
  completion->Signal();
}

void ThreadProxy::ScheduledActionSendBeginMainFrame() {
  unsigned int begin_frame_id = nextBeginFrameId++;
  benchmark_instrumentation::ScopedBeginFrameTask begin_frame_task(
      benchmark_instrumentation::kSendBeginFrame, begin_frame_id);
  scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state(
      new BeginMainFrameAndCommitState);
  begin_main_frame_state->begin_frame_id = begin_frame_id;
  begin_main_frame_state->begin_frame_args =
      impl().layer_tree_host_impl->CurrentBeginFrameArgs();
  begin_main_frame_state->scroll_info =
      impl().layer_tree_host_impl->ProcessScrollDeltas();
  begin_main_frame_state->memory_allocation_limit_bytes =
      impl().layer_tree_host_impl->memory_allocation_limit_bytes();
  begin_main_frame_state->evicted_ui_resources =
      impl().layer_tree_host_impl->EvictedUIResourcesExist();
  // TODO(vmpstr): This needs to be fixed if
  // main_frame_before_activation_enabled is set, since we might run this code
  // twice before recording a duration. crbug.com/469824
  impl().last_begin_main_frame_args = begin_main_frame_state->begin_frame_args;
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::BeginMainFrame,
                 main_thread_weak_ptr_,
                 base::Passed(&begin_main_frame_state)));
  devtools_instrumentation::DidRequestMainThreadFrame(
      impl().layer_tree_host_id);
}

void ThreadProxy::SendBeginMainFrameNotExpectedSoon() {
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ThreadProxy::BeginMainFrameNotExpectedSoon,
                            main_thread_weak_ptr_));
}

void ThreadProxy::BeginMainFrame(
    scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  benchmark_instrumentation::ScopedBeginFrameTask begin_frame_task(
      benchmark_instrumentation::kDoBeginFrame,
      begin_main_frame_state->begin_frame_id);
  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("cc.BeginMainFrame");
  DCHECK(IsMainThread());

  if (main().defer_commits) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit",
                         TRACE_EVENT_SCOPE_THREAD);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ThreadProxy::BeginMainFrameAbortedOnImplThread,
                              impl_thread_weak_ptr_,
                              CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT));
    return;
  }

  // If the commit finishes, LayerTreeHost will transfer its swap promises to
  // LayerTreeImpl. The destructor of ScopedSwapPromiseChecker aborts the
  // remaining swap promises.
  ScopedAbortRemainingSwapPromises swap_promise_checker(layer_tree_host());

  main().commit_requested = false;
  main().commit_request_sent_to_impl_thread = false;
  main().animate_requested = false;

  if (!layer_tree_host()->visible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ThreadProxy::BeginMainFrameAbortedOnImplThread,
                              impl_thread_weak_ptr_,
                              CommitEarlyOutReason::ABORTED_NOT_VISIBLE));
    return;
  }

  if (layer_tree_host()->output_surface_lost()) {
    TRACE_EVENT_INSTANT0(
        "cc", "EarlyOut_OutputSurfaceLost", TRACE_EVENT_SCOPE_THREAD);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginMainFrameAbortedOnImplThread,
                   impl_thread_weak_ptr_,
                   CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST));
    return;
  }

  // Do not notify the impl thread of commit requests that occur during
  // the apply/animate/layout part of the BeginMainFrameAndCommit process since
  // those commit requests will get painted immediately. Once we have done
  // the paint, main().commit_requested will be set to false to allow new commit
  // requests to be scheduled.
  // On the other hand, the animate_requested flag should remain cleared
  // here so that any animation requests generated by the apply or animate
  // callbacks will trigger another frame.
  main().commit_requested = true;
  main().commit_request_sent_to_impl_thread = true;

  layer_tree_host()->ApplyScrollAndScale(
      begin_main_frame_state->scroll_info.get());

  layer_tree_host()->WillBeginMainFrame();

  layer_tree_host()->BeginMainFrame(begin_main_frame_state->begin_frame_args);
  layer_tree_host()->AnimateLayers(
      begin_main_frame_state->begin_frame_args.frame_time);

  // Recreate all UI resources if there were evicted UI resources when the impl
  // thread initiated the commit.
  if (begin_main_frame_state->evicted_ui_resources)
    layer_tree_host()->RecreateUIResources();

  layer_tree_host()->Layout();
  TRACE_EVENT_SYNTHETIC_DELAY_END("cc.BeginMainFrame");

  // Clear the commit flag after updating animations and layout here --- objects
  // that only layout when painted will trigger another SetNeedsCommit inside
  // UpdateLayers.
  main().commit_requested = false;
  main().commit_request_sent_to_impl_thread = false;
  bool can_cancel_this_commit =
      main().can_cancel_commit && !begin_main_frame_state->evicted_ui_resources;
  main().can_cancel_commit = true;

  bool updated = layer_tree_host()->UpdateLayers();

  layer_tree_host()->WillCommit();
  devtools_instrumentation::ScopedCommitTrace commit_task(
      layer_tree_host()->id());

  // Before calling animate, we set main().animate_requested to false. If it is
  // true now, it means SetNeedAnimate was called again, but during a state when
  // main().commit_request_sent_to_impl_thread = true. We need to force that
  // call to happen again now so that the commit request is sent to the impl
  // thread.
  if (main().animate_requested) {
    // Forces SetNeedsAnimate to consider posting a commit task.
    main().animate_requested = false;
    SetNeedsAnimate();
  }

  if (!updated && can_cancel_this_commit) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoUpdates", TRACE_EVENT_SCOPE_THREAD);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ThreadProxy::BeginMainFrameAbortedOnImplThread,
                              impl_thread_weak_ptr_,
                              CommitEarlyOutReason::FINISHED_NO_UPDATES));

    // Although the commit is internally aborted, this is because it has been
    // detected to be a no-op.  From the perspective of an embedder, this commit
    // went through, and input should no longer be throttled, etc.
    layer_tree_host()->CommitComplete();
    layer_tree_host()->DidBeginMainFrame();
    layer_tree_host()->BreakSwapPromises(SwapPromise::COMMIT_NO_UPDATE);
    return;
  }

  // Notify the impl thread that the main thread is ready to commit. This will
  // begin the commit process, which is blocking from the main thread's
  // point of view, but asynchronously performed on the impl thread,
  // coordinated by the Scheduler.
  {
    TRACE_EVENT0("cc", "ThreadProxy::BeginMainFrame::commit");

    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    // This CapturePostTasks should be destroyed before CommitComplete() is
    // called since that goes out to the embedder, and we want the embedder
    // to receive its callbacks before that.
    BlockingTaskRunner::CapturePostTasks blocked(
        blocking_main_thread_task_runner());

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ThreadProxy::StartCommitOnImplThread,
                              impl_thread_weak_ptr_, &completion));
    completion.Wait();
  }

  layer_tree_host()->CommitComplete();
  layer_tree_host()->DidBeginMainFrame();
}

void ThreadProxy::BeginMainFrameNotExpectedSoon() {
  TRACE_EVENT0("cc", "ThreadProxy::BeginMainFrameNotExpectedSoon");
  DCHECK(IsMainThread());
  layer_tree_host()->BeginMainFrameNotExpectedSoon();
}

void ThreadProxy::StartCommitOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::StartCommitOnImplThread");
  DCHECK(!impl().commit_completion_event);
  DCHECK(IsImplThread() && IsMainThreadBlocked());
  DCHECK(impl().scheduler);
  DCHECK(impl().scheduler->CommitPending());

  if (!impl().layer_tree_host_impl) {
    TRACE_EVENT_INSTANT0(
        "cc", "EarlyOut_NoLayerTree", TRACE_EVENT_SCOPE_THREAD);
    completion->Signal();
    return;
  }

  // Ideally, we should inform to impl thread when BeginMainFrame is started.
  // But, we can avoid a PostTask in here.
  impl().scheduler->NotifyBeginMainFrameStarted();
  impl().commit_completion_event = completion;
  impl().scheduler->NotifyReadyToCommit();
}

void ThreadProxy::BeginMainFrameAbortedOnImplThread(
    CommitEarlyOutReason reason) {
  TRACE_EVENT1("cc", "ThreadProxy::BeginMainFrameAbortedOnImplThread", "reason",
               CommitEarlyOutReasonToString(reason));
  DCHECK(IsImplThread());
  DCHECK(impl().scheduler);
  DCHECK(impl().scheduler->CommitPending());
  DCHECK(!impl().layer_tree_host_impl->pending_tree());

  if (CommitEarlyOutHandledCommit(reason)) {
    SetInputThrottledUntilCommitOnImplThread(false);
    impl().last_processed_begin_main_frame_args =
        impl().last_begin_main_frame_args;
  }
  impl().layer_tree_host_impl->BeginMainFrameAborted(reason);
  impl().scheduler->BeginMainFrameAborted(reason);
}

void ThreadProxy::ScheduledActionAnimate() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionAnimate");
  DCHECK(IsImplThread());

  // Don't animate if there is no root layer.
  // TODO(mithro): Both Animate and UpdateAnimationState already have a
  // "!active_tree_->root_layer()" check?
  if (!impl().layer_tree_host_impl->active_tree()->root_layer()) {
    return;
  }

  impl().animation_time =
      impl().layer_tree_host_impl->CurrentBeginFrameArgs().frame_time;
  impl().layer_tree_host_impl->Animate(impl().animation_time);

  // If animations are not visible, update the state now as
  // ScheduledActionDrawAndSwapIfPossible will never be called.
  if (!impl().layer_tree_host_impl->AnimationsAreVisible()) {
    impl().layer_tree_host_impl->UpdateAnimationState(true);
  }
}

void ThreadProxy::ScheduledActionCommit() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionCommit");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  DCHECK(impl().commit_completion_event);

  blocked_main().main_thread_inside_commit = true;
  impl().layer_tree_host_impl->BeginCommit();
  layer_tree_host()->BeginCommitOnImplThread(impl().layer_tree_host_impl.get());
  layer_tree_host()->FinishCommitOnImplThread(
      impl().layer_tree_host_impl.get());
  blocked_main().main_thread_inside_commit = false;

  bool hold_commit = blocked_main().commit_waits_for_activation;
  blocked_main().commit_waits_for_activation = false;

  if (hold_commit) {
    // For some layer types in impl-side painting, the commit is held until
    // the sync tree is activated.  It's also possible that the
    // sync tree has already activated if there was no work to be done.
    TRACE_EVENT_INSTANT0("cc", "HoldCommit", TRACE_EVENT_SCOPE_THREAD);
    impl().completion_event_for_commit_held_on_tree_activation =
        impl().commit_completion_event;
    impl().commit_completion_event = NULL;
  } else {
    impl().commit_completion_event->Signal();
    impl().commit_completion_event = NULL;
  }

  impl().scheduler->DidCommit();

  // Delay this step until afer the main thread has been released as it's
  // often a good bit of work to update the tree and prepare the new frame.
  impl().layer_tree_host_impl->CommitComplete();

  SetInputThrottledUntilCommitOnImplThread(false);

  impl().next_frame_is_newly_committed_frame = true;
}

void ThreadProxy::ScheduledActionActivateSyncTree() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionActivateSyncTree");
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->ActivateSyncTree();
}

void ThreadProxy::ScheduledActionBeginOutputSurfaceCreation() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionBeginOutputSurfaceCreation");
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::RequestNewOutputSurface, main_thread_weak_ptr_));
}

DrawResult ThreadProxy::DrawSwapInternal(bool forced_draw) {
  TRACE_EVENT_SYNTHETIC_DELAY("cc.DrawAndSwap");
  DrawResult result;

  DCHECK(IsImplThread());
  DCHECK(impl().layer_tree_host_impl.get());

  base::AutoReset<bool> mark_inside(&impl().inside_draw, true);

  if (impl().layer_tree_host_impl->pending_tree()) {
    bool update_lcd_text = false;
    impl().layer_tree_host_impl->pending_tree()->UpdateDrawProperties(
        update_lcd_text);
  }

  // This method is called on a forced draw, regardless of whether we are able
  // to produce a frame, as the calling site on main thread is blocked until its
  // request completes, and we signal completion here. If CanDraw() is false, we
  // will indicate success=false to the caller, but we must still signal
  // completion to avoid deadlock.

  // We guard PrepareToDraw() with CanDraw() because it always returns a valid
  // frame, so can only be used when such a frame is possible. Since
  // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
  // CanDraw() as well.

  LayerTreeHostImpl::FrameData frame;
  bool draw_frame = false;

  if (impl().layer_tree_host_impl->CanDraw()) {
    result = impl().layer_tree_host_impl->PrepareToDraw(&frame);
    draw_frame = forced_draw || result == DRAW_SUCCESS;
  } else {
    result = DRAW_ABORTED_CANT_DRAW;
  }

  if (draw_frame) {
    impl().layer_tree_host_impl->DrawLayers(&frame);
    result = DRAW_SUCCESS;
  } else {
    DCHECK_NE(DRAW_SUCCESS, result);
  }
  impl().layer_tree_host_impl->DidDrawAllLayers(frame);

  bool start_ready_animations = draw_frame;
  impl().layer_tree_host_impl->UpdateAnimationState(start_ready_animations);

  if (draw_frame)
    impl().layer_tree_host_impl->SwapBuffers(frame);

  // Tell the main thread that the the newly-commited frame was drawn.
  if (impl().next_frame_is_newly_committed_frame) {
    impl().next_frame_is_newly_committed_frame = false;
    Proxy::MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::DidCommitAndDrawFrame, main_thread_weak_ptr_));
  }

  DCHECK_NE(INVALID_RESULT, result);
  return result;
}

void ThreadProxy::ScheduledActionPrepareTiles() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionPrepareTiles");
  impl().layer_tree_host_impl->PrepareTiles();
}

DrawResult ThreadProxy::ScheduledActionDrawAndSwapIfPossible() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndSwap");

  // SchedulerStateMachine::DidDrawIfPossibleCompleted isn't set up to
  // handle DRAW_ABORTED_CANT_DRAW.  Moreover, the scheduler should
  // never generate this call when it can't draw.
  DCHECK(impl().layer_tree_host_impl->CanDraw());

  bool forced_draw = false;
  return DrawSwapInternal(forced_draw);
}

DrawResult ThreadProxy::ScheduledActionDrawAndSwapForced() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndSwapForced");
  bool forced_draw = true;
  return DrawSwapInternal(forced_draw);
}

void ThreadProxy::ScheduledActionInvalidateOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionInvalidateOutputSurface");
  DCHECK(impl().layer_tree_host_impl->output_surface());
  impl().layer_tree_host_impl->output_surface()->Invalidate();
}

void ThreadProxy::DidFinishImplFrame() {
  impl().layer_tree_host_impl->DidFinishImplFrame();
}

void ThreadProxy::SendBeginFramesToChildren(const BeginFrameArgs& args) {
  NOTREACHED() << "Only used by SingleThreadProxy";
}

void ThreadProxy::SetAuthoritativeVSyncInterval(
    const base::TimeDelta& interval) {
  NOTREACHED() << "Only used by SingleThreadProxy";
}

void ThreadProxy::DidCommitAndDrawFrame() {
  DCHECK(IsMainThread());
  layer_tree_host()->DidCommitAndDrawFrame();
}

void ThreadProxy::DidCompleteSwapBuffers() {
  DCHECK(IsMainThread());
  layer_tree_host()->DidCompleteSwapBuffers();
}

void ThreadProxy::SetAnimationEvents(scoped_ptr<AnimationEventsVector> events) {
  TRACE_EVENT0("cc", "ThreadProxy::SetAnimationEvents");
  DCHECK(IsMainThread());
  layer_tree_host()->SetAnimationEvents(events.Pass());
}

void ThreadProxy::InitializeImplOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeImplOnImplThread");
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl =
      layer_tree_host()->CreateLayerTreeHostImpl(this);

  SchedulerSettings scheduler_settings(
      layer_tree_host()->settings().ToSchedulerSettings());

  scoped_ptr<CompositorTimingHistory> compositor_timing_history(
      new CompositorTimingHistory(impl().rendering_stats_instrumentation));

  impl().scheduler = Scheduler::Create(
      this, scheduler_settings, impl().layer_tree_host_id,
      ImplThreadTaskRunner(), impl().external_begin_frame_source.get(),
      compositor_timing_history.Pass());

  impl().scheduler->SetVisible(impl().layer_tree_host_impl->visible());
  impl_thread_weak_ptr_ = impl().weak_factory.GetWeakPtr();
  completion->Signal();
}

void ThreadProxy::InitializeOutputSurfaceOnImplThread(
    scoped_ptr<OutputSurface> output_surface) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());

  LayerTreeHostImpl* host_impl = impl().layer_tree_host_impl.get();
  bool success = host_impl->InitializeRenderer(output_surface.Pass());
  RendererCapabilities capabilities;
  if (success) {
    capabilities =
        host_impl->GetRendererCapabilities().MainThreadCapabilities();
  }

  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::DidInitializeOutputSurface,
                 main_thread_weak_ptr_,
                 success,
                 capabilities));

  if (success)
    impl().scheduler->DidCreateAndInitializeOutputSurface();
}

void ThreadProxy::FinishGLOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::FinishGLOnImplThread");
  DCHECK(IsImplThread());
  if (impl().layer_tree_host_impl->output_surface()) {
    ContextProvider* context_provider =
        impl().layer_tree_host_impl->output_surface()->context_provider();
    if (context_provider)
      context_provider->ContextGL()->Finish();
  }
  completion->Signal();
}

void ThreadProxy::LayerTreeHostClosedOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::LayerTreeHostClosedOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  impl().scheduler = nullptr;
  impl().external_begin_frame_source = nullptr;
  impl().layer_tree_host_impl = nullptr;
  impl().weak_factory.InvalidateWeakPtrs();
  // We need to explicitly shutdown the notifier to destroy any weakptrs it is
  // holding while still on the compositor thread. This also ensures any
  // callbacks holding a ThreadProxy pointer are cancelled.
  impl().smoothness_priority_expiration_notifier.Shutdown();
  completion->Signal();
}

ThreadProxy::BeginMainFrameAndCommitState::BeginMainFrameAndCommitState()
    : memory_allocation_limit_bytes(0),
      evicted_ui_resources(false) {}

ThreadProxy::BeginMainFrameAndCommitState::~BeginMainFrameAndCommitState() {}

bool ThreadProxy::MainFrameWillHappenForTesting() {
  DCHECK(IsMainThread());
  CompletionEvent completion;
  bool main_frame_will_happen = false;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::MainFrameWillHappenOnImplThreadForTesting,
                   impl_thread_weak_ptr_,
                   &completion,
                   &main_frame_will_happen));
    completion.Wait();
  }
  return main_frame_will_happen;
}

void ThreadProxy::SetChildrenNeedBeginFrames(bool children_need_begin_frames) {
  NOTREACHED() << "Only used by SingleThreadProxy";
}

void ThreadProxy::MainFrameWillHappenOnImplThreadForTesting(
    CompletionEvent* completion,
    bool* main_frame_will_happen) {
  DCHECK(IsImplThread());
  if (impl().layer_tree_host_impl->output_surface()) {
    *main_frame_will_happen = impl().scheduler->MainFrameForTestingWillHappen();
  } else {
    *main_frame_will_happen = false;
  }
  completion->Signal();
}

void ThreadProxy::RenewTreePriority() {
  DCHECK(IsImplThread());
  bool smoothness_takes_priority =
      impl().layer_tree_host_impl->pinch_gesture_active() ||
      impl().layer_tree_host_impl->page_scale_animation_active() ||
      impl().layer_tree_host_impl->IsActivelyScrolling();

  // Schedule expiration if smoothness currently takes priority.
  if (smoothness_takes_priority)
    impl().smoothness_priority_expiration_notifier.Schedule();

  // We use the same priority for both trees by default.
  TreePriority priority = SAME_PRIORITY_FOR_BOTH_TREES;

  // Smoothness takes priority if we have an expiration for it scheduled.
  if (impl().smoothness_priority_expiration_notifier.HasPendingNotification())
    priority = SMOOTHNESS_TAKES_PRIORITY;

  // New content always takes priority when there is an invalid viewport size or
  // ui resources have been evicted.
  if (impl().layer_tree_host_impl->active_tree()->ViewportSizeInvalid() ||
      impl().layer_tree_host_impl->EvictedUIResourcesExist() ||
      impl().input_throttled_until_commit) {
    // Once we enter NEW_CONTENTS_TAKES_PRIORITY mode, visible tiles on active
    // tree might be freed. We need to set RequiresHighResToDraw to ensure that
    // high res tiles will be required to activate pending tree.
    impl().layer_tree_host_impl->SetRequiresHighResToDraw();
    priority = NEW_CONTENT_TAKES_PRIORITY;
  }

  impl().layer_tree_host_impl->SetTreePriority(priority);

  // Only put the scheduler in impl latency prioritization mode if we don't
  // have a scroll listener. This gives the scroll listener a better chance of
  // handling scroll updates within the same frame. The tree itself is still
  // kept in prefer smoothness mode to allow checkerboarding.
  impl().scheduler->SetImplLatencyTakesPriority(
      priority == SMOOTHNESS_TAKES_PRIORITY &&
      !impl().layer_tree_host_impl->scroll_affects_scroll_handler());

  // Notify the the client of this compositor via the output surface.
  // TODO(epenner): Route this to compositor-thread instead of output-surface
  // after GTFO refactor of compositor-thread (http://crbug/170828).
  if (impl().layer_tree_host_impl->output_surface()) {
    impl()
        .layer_tree_host_impl->output_surface()
        ->UpdateSmoothnessTakesPriority(priority == SMOOTHNESS_TAKES_PRIORITY);
  }
}

void ThreadProxy::PostDelayedAnimationTaskOnImplThread(
    const base::Closure& task,
    base::TimeDelta delay) {
  Proxy::ImplThreadTaskRunner()->PostDelayedTask(FROM_HERE, task, delay);
}

void ThreadProxy::DidActivateSyncTree() {
  TRACE_EVENT0("cc", "ThreadProxy::DidActivateSyncTreeOnImplThread");
  DCHECK(IsImplThread());

  if (impl().completion_event_for_commit_held_on_tree_activation) {
    TRACE_EVENT_INSTANT0(
        "cc", "ReleaseCommitbyActivation", TRACE_EVENT_SCOPE_THREAD);
    impl().completion_event_for_commit_held_on_tree_activation->Signal();
    impl().completion_event_for_commit_held_on_tree_activation = NULL;
  }

  impl().last_processed_begin_main_frame_args =
      impl().last_begin_main_frame_args;
}

void ThreadProxy::WillPrepareTiles() {
  DCHECK(IsImplThread());
  impl().scheduler->WillPrepareTiles();
}

void ThreadProxy::DidPrepareTiles() {
  DCHECK(IsImplThread());
  impl().scheduler->DidPrepareTiles();
}

void ThreadProxy::DidCompletePageScaleAnimationOnImplThread() {
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ThreadProxy::DidCompletePageScaleAnimation,
                            main_thread_weak_ptr_));
}

void ThreadProxy::OnDrawForOutputSurface() {
  DCHECK(IsImplThread());
  impl().scheduler->OnDrawForOutputSurface();
}

void ThreadProxy::PostFrameTimingEventsOnImplThread(
    scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events) {
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::PostFrameTimingEvents, main_thread_weak_ptr_,
                 base::Passed(composite_events.Pass()),
                 base::Passed(main_frame_events.Pass())));
}

void ThreadProxy::PostFrameTimingEvents(
    scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events) {
  DCHECK(IsMainThread());
  layer_tree_host()->RecordFrameTimingEvents(composite_events.Pass(),
                                             main_frame_events.Pass());
}

}  // namespace cc
