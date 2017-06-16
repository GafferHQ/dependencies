// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_

#include "ui/views/controls/menu/menu_runner_impl_interface.h"

namespace views {
namespace internal {

class MenuRunnerImpl;

// Given a MenuModel, adapts MenuRunnerImpl which expects a MenuItemView.
class MenuRunnerImplAdapter : public MenuRunnerImplInterface {
 public:
  explicit MenuRunnerImplAdapter(ui::MenuModel* menu_model);

  // MenuRunnerImplInterface:
  bool IsRunning() const override;
  void Release() override;
  MenuRunner::RunResult RunMenuAt(Widget* parent,
                                  MenuButton* button,
                                  const gfx::Rect& bounds,
                                  MenuAnchorPosition anchor,
                                  int32 types) override;
  void Cancel() override;
  base::TimeDelta GetClosingEventTime() const override;

 private:
  ~MenuRunnerImplAdapter() override;

  scoped_ptr<MenuModelAdapter> menu_model_adapter_;
  MenuRunnerImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerImplAdapter);
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_ADAPTER_H_
