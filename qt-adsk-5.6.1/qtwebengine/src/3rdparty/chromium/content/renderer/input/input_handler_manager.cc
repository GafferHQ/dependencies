// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/input/input_handler.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "content/renderer/input/input_handler_wrapper.h"
#include "content/renderer/input/input_scroll_elasticity_controller.h"

using blink::WebInputEvent;
using scheduler::RendererScheduler;

namespace content {

namespace {

InputEventAckState InputEventDispositionToAck(
    InputHandlerProxy::EventDisposition disposition) {
  switch (disposition) {
    case InputHandlerProxy::DID_HANDLE:
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    case InputHandlerProxy::DID_NOT_HANDLE:
      return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    case InputHandlerProxy::DROP_EVENT:
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
  }
  NOTREACHED();
  return INPUT_EVENT_ACK_STATE_UNKNOWN;
}

} // namespace

InputHandlerManager::InputHandlerManager(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    InputHandlerManagerClient* client,
    scheduler::RendererScheduler* renderer_scheduler)
    : task_runner_(task_runner),
      client_(client),
      renderer_scheduler_(renderer_scheduler) {
  DCHECK(client_);
  client_->SetBoundHandler(base::Bind(&InputHandlerManager::HandleInputEvent,
                                      base::Unretained(this)));
}

InputHandlerManager::~InputHandlerManager() {
  client_->SetBoundHandler(InputHandlerManagerClient::Handler());
}

void InputHandlerManager::AddInputHandler(
    int routing_id,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  if (task_runner_->BelongsToCurrentThread()) {
    AddInputHandlerOnCompositorThread(routing_id,
                                      base::ThreadTaskRunnerHandle::Get(),
                                      input_handler, render_view_impl);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&InputHandlerManager::AddInputHandlerOnCompositorThread,
                   base::Unretained(this), routing_id,
                   base::ThreadTaskRunnerHandle::Get(), input_handler,
                   render_view_impl));
  }
}

void InputHandlerManager::AddInputHandlerOnCompositorThread(
    int routing_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // The handler could be gone by this point if the compositor has shut down.
  if (!input_handler)
    return;

  // The same handler may be registered for a route multiple times.
  if (input_handlers_.count(routing_id) != 0)
    return;

  TRACE_EVENT1("input",
      "InputHandlerManager::AddInputHandlerOnCompositorThread",
      "result", "AddingRoute");
  client_->DidAddInputHandler(routing_id, input_handler.get());
  input_handlers_.add(routing_id, make_scoped_ptr(new InputHandlerWrapper(
                                      this, routing_id, main_task_runner,
                                      input_handler, render_view_impl)));
}

void InputHandlerManager::RemoveInputHandler(int routing_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(input_handlers_.contains(routing_id));

  TRACE_EVENT0("input", "InputHandlerManager::RemoveInputHandler");

  client_->DidRemoveInputHandler(routing_id);
  input_handlers_.erase(routing_id);
}

void InputHandlerManager::ObserveWheelEventAndResultOnMainThread(
    int routing_id,
    const blink::WebMouseWheelEvent& wheel_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &InputHandlerManager::ObserveWheelEventAndResultOnCompositorThread,
          base::Unretained(this), routing_id, wheel_event, scroll_result));
}

void InputHandlerManager::ObserveWheelEventAndResultOnCompositorThread(
    int routing_id,
    const blink::WebMouseWheelEvent& wheel_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  auto it = input_handlers_.find(routing_id);
  if (it == input_handlers_.end())
    return;

  InputHandlerProxy* proxy = it->second->input_handler_proxy();
  DCHECK(proxy->scroll_elasticity_controller());
  proxy->scroll_elasticity_controller()->ObserveWheelEventAndResult(
      wheel_event, scroll_result);
}

InputEventAckState InputHandlerManager::HandleInputEvent(
    int routing_id,
    const WebInputEvent* input_event,
    ui::LatencyInfo* latency_info) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto it = input_handlers_.find(routing_id);
  if (it == input_handlers_.end()) {
    TRACE_EVENT1("input", "InputHandlerManager::HandleInputEvent",
                 "result", "NoInputHandlerFound");
    // Oops, we no longer have an interested input handler..
    return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  }

  InputHandlerProxy* proxy = it->second->input_handler_proxy();
  InputEventAckState input_event_ack_state = InputEventDispositionToAck(
      proxy->HandleInputEventWithLatencyInfo(*input_event, latency_info));
  switch (input_event_ack_state) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
      renderer_scheduler_->DidHandleInputEventOnCompositorThread(
          *input_event,
          RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
      renderer_scheduler_->DidHandleInputEventOnCompositorThread(
          *input_event,
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      break;
    default:
      break;
  }
  return input_event_ack_state;
}

void InputHandlerManager::DidOverscroll(int routing_id,
                                        const DidOverscrollParams& params) {
  client_->DidOverscroll(routing_id, params);
}

void InputHandlerManager::DidStopFlinging(int routing_id) {
  client_->DidStopFlinging(routing_id);
}

void InputHandlerManager::DidAnimateForInput() {
  renderer_scheduler_->DidAnimateForInputOnCompositorThread();
}

}  // namespace content
