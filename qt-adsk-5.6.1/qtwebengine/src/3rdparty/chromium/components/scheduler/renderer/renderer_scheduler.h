// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_

#include "base/message_loop/message_loop.h"
#include "components/scheduler/child/child_scheduler.h"
#include "components/scheduler/child/single_thread_idle_task_runner.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace cc {
struct BeginFrameArgs;
}

namespace scheduler {

class SCHEDULER_EXPORT RendererScheduler : public ChildScheduler {
 public:
  ~RendererScheduler() override;
  static scoped_ptr<RendererScheduler> Create();

  // Returns the compositor task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  CompositorTaskRunner() = 0;

  // Returns the loading task runner.  This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  virtual scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() = 0;

  // Returns the timer task runner.  This queue is intended for DOM Timers.
  virtual scoped_refptr<TaskQueue> TimerTaskRunner() = 0;

  // Called to notify about the start of an extended period where no frames
  // need to be drawn. Must be called from the main thread.
  virtual void BeginFrameNotExpectedSoon() = 0;

  // Called to notify about the start of a new frame.  Must be called from the
  // main thread.
  virtual void WillBeginFrame(const cc::BeginFrameArgs& args) = 0;

  // Called to notify that a previously begun frame was committed. Must be
  // called from the main thread.
  virtual void DidCommitFrameToCompositor() = 0;

  enum class InputEventState {
    EVENT_CONSUMED_BY_COMPOSITOR,
    EVENT_FORWARDED_TO_MAIN_THREAD,
  };

  // Tells the scheduler that the system processed an input event. Called by the
  // compositor (impl) thread.  Note it's expected that every call to
  // DidHandleInputEventOnCompositorThread where |event_state| is
  // EVENT_FORWARDED_TO_MAIN_THREAD will be followed by a corresponding call
  // to DidHandleInputEventOnMainThread.
  virtual void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) = 0;

  // Tells the scheduler that the system processed an input event. Must be
  // called from the main thread.
  virtual void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) = 0;

  // Tells the scheduler that the system is displaying an input animation (e.g.
  // a fling). Called by the compositor (impl) thread.
  virtual void DidAnimateForInputOnCompositorThread() = 0;

  // Tells the scheduler that all render widgets managed by this renderer
  // process have been hidden. The renderer is assumed to be visible when the
  // scheduler is constructed. Must be called on the main thread.
  virtual void OnRendererHidden() = 0;

  // Tells the scheduler that at least one render widget managed by this
  // renderer process has become visible and the renderer is no longer hidden.
  // The renderer is assumed to be visible when the scheduler is constructed.
  // Must be called on the main thread.
  virtual void OnRendererVisible() = 0;

  // Tells the scheduler that a page load has started.  The scheduler will
  // prioritize loading tasks for a short duration afterwards.
  // Must be called from the main thread.
  virtual void OnPageLoadStarted() = 0;

  // Returns true if the scheduler has reason to believe that high priority work
  // may soon arrive on the main thread, e.g., if gesture events were observed
  // recently.
  // Must be called from the main thread.
  virtual bool IsHighPriorityWorkAnticipated() = 0;

  // Suspends the timer queue and increments the timer queue suspension count.
  // May only be called from the main thread.
  virtual void SuspendTimerQueue() = 0;

  // Decrements the timer queue suspension count and re-enables the timer queue
  // if the suspension count is zero and the current schduler policy allows it.
  virtual void ResumeTimerQueue() = 0;

 protected:
  RendererScheduler();
  DISALLOW_COPY_AND_ASSIGN(RendererScheduler);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
