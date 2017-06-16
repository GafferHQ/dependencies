// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/tree/tree_view_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class MenuRunner;
class TreeView;

namespace examples {

class VIEWS_EXAMPLES_EXPORT TreeViewExample
    : public ExampleBase,
      public ButtonListener,
      public TreeViewController,
      public ContextMenuController,
      public ui::SimpleMenuModel::Delegate {
 public:
  TreeViewExample();
  ~TreeViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // IDs used by the context menu.
  enum MenuIDs {
    ID_EDIT,
    ID_REMOVE,
    ID_ADD
  };

  // Adds a new node.
  void AddNewNode();

  // Non-const version of IsCommandIdEnabled.
  bool IsCommandIdEnabled(int command_id);

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // TreeViewController:
  void OnTreeViewSelectionChanged(TreeView* tree_view) override;
  bool CanEdit(TreeView* tree_view, ui::TreeModelNode* node) override;

  // ContextMenuController:
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // The tree view to be tested.
  TreeView* tree_view_;

  // Control buttons to modify the model.
  Button* add_;
  Button* remove_;
  Button* change_title_;

  typedef ui::TreeNodeWithValue<int> NodeType;

  ui::TreeNodeModel<NodeType> model_;

  scoped_ptr<MenuRunner> context_menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(TreeViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TREE_VIEW_EXAMPLE_H_
