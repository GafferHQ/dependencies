// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_menu_runner_views.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace views {
namespace {

const int kMenuCommands[] = {IDS_APP_CUT, IDS_APP_COPY, IDS_APP_PASTE};
const int kSpacingBetweenButtons = 2;
const int kButtonSeparatorColor = SkColorSetARGB(13, 0, 0, 0);
const int kMenuButtonMinHeight = 38;
const int kMenuButtonMinWidth = 63;
const int kMenuMargin = 1;

const char kEllipsesButtonText[] = "...";
const int kEllipsesButtonTag = -1;

}  // namespace

// A bubble that contains actions available for the selected text. An object of
// this type, as a BubbleDelegateView, manages its own lifetime.
class TouchSelectionMenuRunnerViews::Menu : public BubbleDelegateView,
                                            public ButtonListener {
 public:
  // Closes the menu. This will eventually self-destroy the object.
  void Close();

  // Returns a new instance of Menu if there is any command available;
  // otherwise, returns |nullptr|.
  static Menu* Create(TouchSelectionMenuRunnerViews* owner,
                      ui::TouchSelectionMenuClient* client,
                      const gfx::Rect& anchor_rect,
                      const gfx::Size& handle_image_size,
                      aura::Window* context);

 private:
  Menu(TouchSelectionMenuRunnerViews* owner,
       ui::TouchSelectionMenuClient* client,
       const gfx::Rect& anchor_rect,
       const gfx::Size& handle_image_size,
       aura::Window* context);

  ~Menu() override;

  // Queries the |client_| for what commands to show in the menu and sizes the
  // menu appropriately.
  void CreateButtons();

  // Helper method to create a single button.
  Button* CreateButton(const base::string16& title, int tag);

  // BubbleDelegateView:
  void OnPaint(gfx::Canvas* canvas) override;
  void WindowClosing() override;

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  TouchSelectionMenuRunnerViews* owner_;
  ui::TouchSelectionMenuClient* const client_;

  DISALLOW_COPY_AND_ASSIGN(Menu);
};

TouchSelectionMenuRunnerViews::Menu*
TouchSelectionMenuRunnerViews::Menu::Create(
    TouchSelectionMenuRunnerViews* owner,
    ui::TouchSelectionMenuClient* client,
    const gfx::Rect& anchor_rect,
    const gfx::Size& handle_image_size,
    aura::Window* context) {
  DCHECK(client);

  for (size_t i = 0; i < arraysize(kMenuCommands); i++) {
    if (client->IsCommandIdEnabled(kMenuCommands[i]))
      return new Menu(owner, client, anchor_rect, handle_image_size, context);
  }

  // No command is available, so return |nullptr|.
  return nullptr;
}

TouchSelectionMenuRunnerViews::Menu::Menu(TouchSelectionMenuRunnerViews* owner,
                                          ui::TouchSelectionMenuClient* client,
                                          const gfx::Rect& anchor_rect,
                                          const gfx::Size& handle_image_size,
                                          aura::Window* context)
    : BubbleDelegateView(nullptr, BubbleBorder::BOTTOM_CENTER),
      owner_(owner),
      client_(client) {
  DCHECK(owner_);
  DCHECK(client_);

  set_shadow(BubbleBorder::SMALL_SHADOW);
  set_parent_window(context);
  set_margins(gfx::Insets(kMenuMargin, kMenuMargin, kMenuMargin, kMenuMargin));
  set_can_activate(false);
  set_adjust_if_offscreen(true);
  EnableCanvasFlippingForRTLUI(true);

  SetLayoutManager(
      new BoxLayout(BoxLayout::kHorizontal, 0, 0, kSpacingBetweenButtons));
  CreateButtons();

  // After buttons are created, check if there is enough room between handles to
  // show the menu and adjust anchor rect properly if needed, just in case the
  // menu is needed to be shown under the selection.
  gfx::Rect adjusted_anchor_rect(anchor_rect);
  int menu_width = GetPreferredSize().width();
  // TODO(mfomitchev): This assumes that the handles are center-aligned to the
  // |achor_rect| edges, which is not true. We should fix this, perhaps by
  // passing down the cumulative width occupied by the handles within
  // |anchor_rect| plus the handle image height instead of |handle_image_size|.
  // Perhaps we should also allow for some minimum padding.
  if (menu_width > anchor_rect.width() - handle_image_size.width())
    adjusted_anchor_rect.Inset(0, 0, 0, -handle_image_size.height());
  SetAnchorRect(adjusted_anchor_rect);

  BubbleDelegateView::CreateBubble(this);
  GetWidget()->Show();
}

TouchSelectionMenuRunnerViews::Menu::~Menu() {
}

void TouchSelectionMenuRunnerViews::Menu::CreateButtons() {
  for (size_t i = 0; i < arraysize(kMenuCommands); i++) {
    int command_id = kMenuCommands[i];
    if (!client_->IsCommandIdEnabled(command_id))
      continue;

    Button* button =
        CreateButton(l10n_util::GetStringUTF16(command_id), command_id);
    AddChildView(button);
  }

  // Finally, add ellipses button.
  AddChildView(
      CreateButton(base::UTF8ToUTF16(kEllipsesButtonText), kEllipsesButtonTag));
  Layout();
}

Button* TouchSelectionMenuRunnerViews::Menu::CreateButton(
    const base::string16& title,
    int tag) {
  base::string16 label =
      gfx::RemoveAcceleratorChar(title, '&', nullptr, nullptr);
  LabelButton* button = new LabelButton(this, label);
  button->SetMinSize(gfx::Size(kMenuButtonMinWidth, kMenuButtonMinHeight));
  button->SetFocusable(true);
  button->set_request_focus_on_press(false);
  const gfx::FontList& font_list =
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont);
  button->SetFontList(font_list);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->set_tag(tag);
  return button;
}

void TouchSelectionMenuRunnerViews::Menu::Close() {
  // Closing the widget will self-destroy this object.
  Widget* widget = GetWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
  owner_ = nullptr;
}

void TouchSelectionMenuRunnerViews::Menu::OnPaint(gfx::Canvas* canvas) {
  BubbleDelegateView::OnPaint(canvas);

  // Draw separator bars.
  for (int i = 0; i < child_count() - 1; ++i) {
    View* child = child_at(i);
    int x = child->bounds().right() + kSpacingBetweenButtons / 2;
    canvas->FillRect(gfx::Rect(x, 0, 1, child->height()),
                     kButtonSeparatorColor);
  }
}

void TouchSelectionMenuRunnerViews::Menu::WindowClosing() {
  DCHECK_IMPLIES(owner_, owner_->menu_ == this);
  BubbleDelegateView::WindowClosing();
  if (owner_)
    owner_->menu_ = nullptr;
}

void TouchSelectionMenuRunnerViews::Menu::ButtonPressed(
    Button* sender,
    const ui::Event& event) {
  Close();
  if (sender->tag() != kEllipsesButtonTag)
    client_->ExecuteCommand(sender->tag(), event.flags());
  else
    client_->RunContextMenu();
}

TouchSelectionMenuRunnerViews::TouchSelectionMenuRunnerViews()
    : menu_(nullptr) {
}

TouchSelectionMenuRunnerViews::~TouchSelectionMenuRunnerViews() {
  CloseMenu();
}

gfx::Rect TouchSelectionMenuRunnerViews::GetAnchorRectForTest() const {
  return menu_ ? menu_->GetAnchorRect() : gfx::Rect();
}

void TouchSelectionMenuRunnerViews::OpenMenu(
    ui::TouchSelectionMenuClient* client,
    const gfx::Rect& anchor_rect,
    const gfx::Size& handle_image_size,
    aura::Window* context) {
  CloseMenu();

  menu_ = Menu::Create(this, client, anchor_rect, handle_image_size, context);
}

void TouchSelectionMenuRunnerViews::CloseMenu() {
  if (!menu_)
    return;

  // Closing the menu will eventually delete the object.
  menu_->Close();
  menu_ = nullptr;
}

bool TouchSelectionMenuRunnerViews::IsRunning() const {
  return menu_ != nullptr;
}

}  // namespace views
