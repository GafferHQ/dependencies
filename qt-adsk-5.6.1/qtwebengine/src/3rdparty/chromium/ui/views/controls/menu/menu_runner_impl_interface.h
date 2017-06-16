// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_INTERFACE_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_INTERFACE_H_

#include "ui/views/controls/menu/menu_runner.h"

namespace views {

class MenuItemView;

namespace internal {

// An abstract interface for menu runner implementations.
// Invoke Release() to destroy. Release() deletes immediately if the menu isn't
// showing. If the menu is showing Release() cancels the menu and when the
// nested RunMenuAt() call returns deletes itself and the menu.
class MenuRunnerImplInterface {
 public:
  // Creates a concrete instance for running |menu_model|.
  // |run_types| is a bitmask of MenuRunner::RunTypes.
  static MenuRunnerImplInterface* Create(ui::MenuModel* menu_model,
                                         int32 run_types);

  // Returns true if we're in a nested message loop running the menu.
  virtual bool IsRunning() const = 0;

  // See description above class for details.
  virtual void Release() = 0;

  // Runs the menu. See MenuRunner::RunMenuAt for more details.
  virtual MenuRunner::RunResult RunMenuAt(Widget* parent,
                                          MenuButton* button,
                                          const gfx::Rect& bounds,
                                          MenuAnchorPosition anchor,
                                          int32 run_types)
      WARN_UNUSED_RESULT = 0;

  // Hides and cancels the menu.
  virtual void Cancel() = 0;

  // Returns the time from the event which closed the menu - or 0.
  virtual base::TimeDelta GetClosingEventTime() const = 0;

 protected:
  // Call Release() to delete.
  virtual ~MenuRunnerImplInterface() {}
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_INTERFACE_H_
