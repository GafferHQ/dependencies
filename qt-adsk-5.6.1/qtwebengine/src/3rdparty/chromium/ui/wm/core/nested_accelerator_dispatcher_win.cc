// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/nested_accelerator_dispatcher.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "base/run_loop.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/nested_accelerator_delegate.h"

using base::MessagePumpDispatcher;

namespace wm {

namespace {

bool IsKeyEvent(const MSG& msg) {
  return msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
         msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP;
}

}  // namespace

class NestedAcceleratorDispatcherWin : public NestedAcceleratorDispatcher,
                                       public MessagePumpDispatcher {
 public:
  NestedAcceleratorDispatcherWin(NestedAcceleratorDelegate* delegate,
                                 MessagePumpDispatcher* nested)
      : NestedAcceleratorDispatcher(delegate), nested_dispatcher_(nested) {}
  ~NestedAcceleratorDispatcherWin() override {}

 private:
  // NestedAcceleratorDispatcher:
  scoped_ptr<base::RunLoop> CreateRunLoop() override {
    return make_scoped_ptr(new base::RunLoop(this));
  }

  // MessagePumpDispatcher:
  uint32_t Dispatch(const MSG& event) override {
    if (IsKeyEvent(event)) {
      ui::Accelerator accelerator((ui::KeyEvent(event)));

      switch (delegate_->ProcessAccelerator(accelerator)) {
        case NestedAcceleratorDelegate::RESULT_PROCESS_LATER:
          return POST_DISPATCH_QUIT_LOOP;
        case NestedAcceleratorDelegate::RESULT_PROCESSED:
          return POST_DISPATCH_NONE;
        case NestedAcceleratorDelegate::RESULT_NOT_PROCESSED:
          break;
      }
    }

    return nested_dispatcher_ ? nested_dispatcher_->Dispatch(event)
                              : POST_DISPATCH_PERFORM_DEFAULT;
  }

  MessagePumpDispatcher* nested_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(NestedAcceleratorDispatcherWin);
};

scoped_ptr<NestedAcceleratorDispatcher> NestedAcceleratorDispatcher::Create(
    NestedAcceleratorDelegate* delegate,
    MessagePumpDispatcher* nested_dispatcher) {
  return make_scoped_ptr(
      new NestedAcceleratorDispatcherWin(delegate, nested_dispatcher));
}

}  // namespace wm
