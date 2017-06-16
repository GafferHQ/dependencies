// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_WIN_H_

#include <wrl/client.h>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/drag_drop_client.h"

namespace ui {
class DragSourceWin;
}

namespace views {
class DesktopDropTargetWin;

class VIEWS_EXPORT DesktopDragDropClientWin
    : public aura::client::DragDropClient {
 public:
  DesktopDragDropClientWin(aura::Window* root_window, HWND window);
  ~DesktopDragDropClientWin() override;

  // Overridden from aura::client::DragDropClient:
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragUpdate(aura::Window* target, const ui::LocatedEvent& event) override;
  void Drop(aura::Window* target, const ui::LocatedEvent& event) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;

  void OnNativeWidgetDestroying(HWND window);

 private:
  bool drag_drop_in_progress_;

  int drag_operation_;

  Microsoft::WRL::ComPtr<ui::DragSourceWin> drag_source_;

  scoped_refptr<DesktopDropTargetWin> drop_target_;

  base::WeakPtrFactory<DesktopDragDropClientWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientWin);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_WIN_H_
