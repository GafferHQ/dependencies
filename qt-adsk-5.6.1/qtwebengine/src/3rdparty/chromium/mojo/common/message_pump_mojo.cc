// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/message_pump_mojo.h"

#include <algorithm>
#include <vector>

#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "mojo/common/message_pump_mojo_handler.h"
#include "mojo/common/time_helper.h"

namespace mojo {
namespace common {
namespace {

base::LazyInstance<base::ThreadLocalPointer<MessagePumpMojo> >::Leaky
    g_tls_current_pump = LAZY_INSTANCE_INITIALIZER;

MojoDeadline TimeTicksToMojoDeadline(base::TimeTicks time_ticks,
                                     base::TimeTicks now) {
  // The is_null() check matches that of HandleWatcher as well as how
  // |delayed_work_time| is used.
  if (time_ticks.is_null())
    return MOJO_DEADLINE_INDEFINITE;
  const int64_t delta = (time_ticks - now).InMicroseconds();
  return delta < 0 ? static_cast<MojoDeadline>(0) :
                     static_cast<MojoDeadline>(delta);
}

}  // namespace

// State needed for one iteration of WaitMany. The first handle and flags
// corresponds to that of the control pipe.
struct MessagePumpMojo::WaitState {
  std::vector<Handle> handles;
  std::vector<MojoHandleSignals> wait_signals;
};

struct MessagePumpMojo::RunState {
  RunState() : should_quit(false) {
    CreateMessagePipe(NULL, &read_handle, &write_handle);
  }

  base::TimeTicks delayed_work_time;

  // Used to wake up WaitForWork().
  ScopedMessagePipeHandle read_handle;
  ScopedMessagePipeHandle write_handle;

  bool should_quit;
};

MessagePumpMojo::MessagePumpMojo() : run_state_(NULL), next_handler_id_(0) {
  DCHECK(!current())
      << "There is already a MessagePumpMojo instance on this thread.";
  g_tls_current_pump.Pointer()->Set(this);
}

MessagePumpMojo::~MessagePumpMojo() {
  DCHECK_EQ(this, current());
  g_tls_current_pump.Pointer()->Set(NULL);
}

// static
scoped_ptr<base::MessagePump> MessagePumpMojo::Create() {
  return scoped_ptr<MessagePump>(new MessagePumpMojo());
}

// static
MessagePumpMojo* MessagePumpMojo::current() {
  return g_tls_current_pump.Pointer()->Get();
}

void MessagePumpMojo::AddHandler(MessagePumpMojoHandler* handler,
                                 const Handle& handle,
                                 MojoHandleSignals wait_signals,
                                 base::TimeTicks deadline) {
  CHECK(handler);
  DCHECK(handle.is_valid());
  // Assume it's an error if someone tries to reregister an existing handle.
  CHECK_EQ(0u, handlers_.count(handle));
  Handler handler_data;
  handler_data.handler = handler;
  handler_data.wait_signals = wait_signals;
  handler_data.deadline = deadline;
  handler_data.id = next_handler_id_++;
  handlers_[handle] = handler_data;
}

void MessagePumpMojo::RemoveHandler(const Handle& handle) {
  handlers_.erase(handle);
}

void MessagePumpMojo::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MessagePumpMojo::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MessagePumpMojo::Run(Delegate* delegate) {
  RunState run_state;
  // TODO: better deal with error handling.
  CHECK(run_state.read_handle.is_valid());
  CHECK(run_state.write_handle.is_valid());
  RunState* old_state = NULL;
  {
    base::AutoLock auto_lock(run_state_lock_);
    old_state = run_state_;
    run_state_ = &run_state;
  }
  DoRunLoop(&run_state, delegate);
  {
    base::AutoLock auto_lock(run_state_lock_);
    run_state_ = old_state;
  }
}

void MessagePumpMojo::Quit() {
  base::AutoLock auto_lock(run_state_lock_);
  if (run_state_)
    run_state_->should_quit = true;
}

void MessagePumpMojo::ScheduleWork() {
  base::AutoLock auto_lock(run_state_lock_);
  if (run_state_)
    SignalControlPipe(*run_state_);
}

void MessagePumpMojo::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  base::AutoLock auto_lock(run_state_lock_);
  if (!run_state_)
    return;
  run_state_->delayed_work_time = delayed_work_time;
}

void MessagePumpMojo::DoRunLoop(RunState* run_state, Delegate* delegate) {
  bool more_work_is_plausible = true;
  for (;;) {
    const bool block = !more_work_is_plausible;
    more_work_is_plausible = DoInternalWork(*run_state, block);

    if (run_state->should_quit)
      break;

    more_work_is_plausible |= delegate->DoWork();
    if (run_state->should_quit)
      break;

    more_work_is_plausible |= delegate->DoDelayedWork(
        &run_state->delayed_work_time);
    if (run_state->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    more_work_is_plausible = delegate->DoIdleWork();
    if (run_state->should_quit)
      break;
  }
}

bool MessagePumpMojo::DoInternalWork(const RunState& run_state, bool block) {
  const MojoDeadline deadline = block ? GetDeadlineForWait(run_state) : 0;
  const WaitState wait_state = GetWaitState(run_state);

  const WaitManyResult wait_many_result =
      WaitMany(wait_state.handles, wait_state.wait_signals, deadline, nullptr);
  const MojoResult result = wait_many_result.result;
  bool did_work = true;
  if (result == MOJO_RESULT_OK) {
    if (wait_many_result.index == 0) {
      // Control pipe was written to.
      ReadMessageRaw(run_state.read_handle.get(), NULL, NULL, NULL, NULL,
                     MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
    } else {
      DCHECK(handlers_.find(wait_state.handles[wait_many_result.index]) !=
             handlers_.end());
      WillSignalHandler();
      handlers_[wait_state.handles[wait_many_result.index]]
          .handler->OnHandleReady(wait_state.handles[wait_many_result.index]);
      DidSignalHandler();
    }
  } else {
    switch (result) {
      case MOJO_RESULT_CANCELLED:
      case MOJO_RESULT_FAILED_PRECONDITION:
        RemoveInvalidHandle(wait_state, result, wait_many_result.index);
        break;
      case MOJO_RESULT_DEADLINE_EXCEEDED:
        did_work = false;
        break;
      default:
        base::debug::Alias(&result);
        // Unexpected result is likely fatal, crash so we can determine cause.
        CHECK(false);
    }
  }

  // Notify and remove any handlers whose time has expired. Make a copy in case
  // someone tries to add/remove new handlers from notification.
  const HandleToHandler cloned_handlers(handlers_);
  const base::TimeTicks now(internal::NowTicks());
  for (HandleToHandler::const_iterator i = cloned_handlers.begin();
       i != cloned_handlers.end(); ++i) {
    // Since we're iterating over a clone of the handlers, verify the handler is
    // still valid before notifying.
    if (!i->second.deadline.is_null() && i->second.deadline < now &&
        handlers_.find(i->first) != handlers_.end() &&
        handlers_[i->first].id == i->second.id) {
      WillSignalHandler();
      i->second.handler->OnHandleError(i->first, MOJO_RESULT_DEADLINE_EXCEEDED);
      DidSignalHandler();
      handlers_.erase(i->first);
      did_work = true;
    }
  }
  return did_work;
}

void MessagePumpMojo::RemoveInvalidHandle(const WaitState& wait_state,
                                          MojoResult result,
                                          uint32_t index) {
  // TODO(sky): deal with control pipe going bad.
  CHECK(result == MOJO_RESULT_FAILED_PRECONDITION ||
        result == MOJO_RESULT_CANCELLED);
  CHECK_NE(index, 0u);  // Indicates the control pipe went bad.

  // Remove the handle first, this way if OnHandleError() tries to remove the
  // handle our iterator isn't invalidated.
  CHECK(handlers_.find(wait_state.handles[index]) != handlers_.end());
  MessagePumpMojoHandler* handler =
      handlers_[wait_state.handles[index]].handler;
  handlers_.erase(wait_state.handles[index]);
  WillSignalHandler();
  handler->OnHandleError(wait_state.handles[index], result);
  DidSignalHandler();
}

void MessagePumpMojo::SignalControlPipe(const RunState& run_state) {
  const MojoResult result =
      WriteMessageRaw(run_state.write_handle.get(), NULL, 0, NULL, 0,
                      MOJO_WRITE_MESSAGE_FLAG_NONE);
  // If we can't write we likely won't wake up the thread and there is a strong
  // chance we'll deadlock.
  CHECK_EQ(MOJO_RESULT_OK, result);
}

MessagePumpMojo::WaitState MessagePumpMojo::GetWaitState(
    const RunState& run_state) const {
  WaitState wait_state;
  wait_state.handles.push_back(run_state.read_handle.get());
  wait_state.wait_signals.push_back(MOJO_HANDLE_SIGNAL_READABLE);

  for (HandleToHandler::const_iterator i = handlers_.begin();
       i != handlers_.end(); ++i) {
    wait_state.handles.push_back(i->first);
    wait_state.wait_signals.push_back(i->second.wait_signals);
  }
  return wait_state;
}

MojoDeadline MessagePumpMojo::GetDeadlineForWait(
    const RunState& run_state) const {
  const base::TimeTicks now(internal::NowTicks());
  MojoDeadline deadline = TimeTicksToMojoDeadline(run_state.delayed_work_time,
                                                  now);
  for (HandleToHandler::const_iterator i = handlers_.begin();
       i != handlers_.end(); ++i) {
    deadline = std::min(
        TimeTicksToMojoDeadline(i->second.deadline, now), deadline);
  }
  return deadline;
}

void MessagePumpMojo::WillSignalHandler() {
  FOR_EACH_OBSERVER(Observer, observers_, WillSignalHandler());
}

void MessagePumpMojo::DidSignalHandler() {
  FOR_EACH_OBSERVER(Observer, observers_, DidSignalHandler());
}

}  // namespace common
}  // namespace mojo
