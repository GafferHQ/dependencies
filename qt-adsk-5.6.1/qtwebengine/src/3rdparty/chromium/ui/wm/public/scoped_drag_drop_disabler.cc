// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/scoped_drag_drop_disabler.h"

#include "ui/aura/window.h"
#include "ui/wm/public/drag_drop_client.h"

namespace aura {
namespace client {

class NopDragDropClient : public DragDropClient {
 public:
  ~NopDragDropClient() override {}
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override {
    return 0;
  }
  void DragUpdate(aura::Window* target,
                  const ui::LocatedEvent& event) override {}
  void Drop(aura::Window* target, const ui::LocatedEvent& event) override {}
  void DragCancel() override {}
  bool IsDragDropInProgress() override {
    return false;
  }
};

ScopedDragDropDisabler::ScopedDragDropDisabler(Window* window)
    : window_(window),
      old_client_(GetDragDropClient(window)),
      new_client_(new NopDragDropClient()) {
  SetDragDropClient(window_, new_client_.get());
  window_->AddObserver(this);
}

ScopedDragDropDisabler::~ScopedDragDropDisabler() {
  if (window_) {
    window_->RemoveObserver(this);
    SetDragDropClient(window_, old_client_);
  }
}

void ScopedDragDropDisabler::OnWindowDestroyed(Window* window) {
  CHECK_EQ(window_, window);
  window_ = NULL;
  new_client_.reset();
}

}  // namespace client
}  // namespace aura
