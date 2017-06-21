// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <set>

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/combobox_model.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/test/menu_runner_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {

namespace {

// An dummy implementation of MenuRunnerHandler to check if the dropdown menu is
// shown or not.
class TestMenuRunnerHandler : public MenuRunnerHandler {
 public:
  TestMenuRunnerHandler() : executed_(false) {}

  bool executed() const { return executed_; }

  MenuRunner::RunResult RunMenuAt(Widget* parent,
                                  MenuButton* button,
                                  const gfx::Rect& bounds,
                                  MenuAnchorPosition anchor,
                                  ui::MenuSourceType source_type,
                                  int32 types) override {
    executed_ = true;
    return MenuRunner::NORMAL_EXIT;
  }

 private:
  bool executed_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuRunnerHandler);
};

// A wrapper of Combobox to intercept the result of OnKeyPressed() and
// OnKeyReleased() methods.
class TestCombobox : public Combobox {
 public:
  explicit TestCombobox(ui::ComboboxModel* model)
      : Combobox(model),
        key_handled_(false),
        key_received_(false) {}

  bool OnKeyPressed(const ui::KeyEvent& e) override {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyPressed(e);
    return key_handled_;
  }

  bool OnKeyReleased(const ui::KeyEvent& e) override {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() {
    key_received_ = key_handled_ = false;
  }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestCombobox);
};

// A concrete class is needed to test the combobox.
class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() {}
  ~TestComboboxModel() override {}

  static const int kItemCount = 10;

  // ui::ComboboxModel:
  int GetItemCount() const override { return kItemCount; }
  base::string16 GetItemAt(int index) override {
    if (IsItemSeparatorAt(index)) {
      NOTREACHED();
      return ASCIIToUTF16("SEPARATOR");
    }
    return ASCIIToUTF16(index % 2 == 0 ? "PEANUT BUTTER" : "JELLY");
  }
  bool IsItemSeparatorAt(int index) override {
    return separators_.find(index) != separators_.end();
  }

  int GetDefaultIndex() const override {
    // Return the first index that is not a separator.
    for (int index = 0; index < kItemCount; ++index) {
      if (separators_.find(index) == separators_.end())
        return index;
    }
    NOTREACHED();
    return 0;
  }

  void SetSeparators(const std::set<int>& separators) {
    separators_ = separators;
  }

 private:
  std::set<int> separators_;

  DISALLOW_COPY_AND_ASSIGN(TestComboboxModel);
};

// A combobox model which refers to a vector.
class VectorComboboxModel : public ui::ComboboxModel {
 public:
  explicit VectorComboboxModel(std::vector<std::string>* values)
      : values_(values) {}
  ~VectorComboboxModel() override {}

  // ui::ComboboxModel:
  int GetItemCount() const override { return (int)values_->size(); }
  base::string16 GetItemAt(int index) override {
    return ASCIIToUTF16(values_->at(index));
  }
  bool IsItemSeparatorAt(int index) override { return false; }

 private:
  std::vector<std::string>* values_;
};

class EvilListener : public ComboboxListener {
 public:
  EvilListener() : deleted_(false) {}
  ~EvilListener() override{};

  // ComboboxListener:
  void OnPerformAction(Combobox* combobox) override {
    delete combobox;
    deleted_ = true;
  }

  bool deleted() const { return deleted_; }

 private:
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(EvilListener);
};

class TestComboboxListener : public views::ComboboxListener {
 public:
  TestComboboxListener() : perform_action_index_(-1), actions_performed_(0) {}
  ~TestComboboxListener() override {}

  void OnPerformAction(views::Combobox* combobox) override {
    perform_action_index_ = combobox->selected_index();
    actions_performed_++;
  }

  int perform_action_index() const {
    return perform_action_index_;
  }

  bool on_perform_action_called() const {
    return actions_performed_ > 0;
  }

  int actions_performed() const {
    return actions_performed_;
  }

 private:
  int perform_action_index_;
  int actions_performed_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestComboboxListener);
};

}  // namespace

class ComboboxTest : public ViewsTestBase {
 public:
  ComboboxTest() : widget_(NULL), combobox_(NULL) {}

  void TearDown() override {
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  void InitCombobox(const std::set<int>* separators) {
    model_.reset(new TestComboboxModel());

    if (separators)
      model_->SetSeparators(*separators);

    ASSERT_FALSE(combobox_);
    combobox_ = new TestCombobox(model_.get());
    combobox_->set_id(1);

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(200, 200, 200, 200);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(combobox_);

    combobox_->RequestFocus();
    combobox_->SizeToPreferredSize();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEventWithType(key_code, ui::ET_KEY_PRESSED);
  }

  void SendKeyEventWithType(ui::KeyboardCode key_code, ui::EventType type) {
    ui::KeyEvent event(type, key_code, ui::EF_NONE);
    FocusManager* focus_manager = widget_->GetFocusManager();
    widget_->OnKeyEvent(&event);
    if (!event.handled() && focus_manager)
      focus_manager->OnKeyEvent(event);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  void PerformClick(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&pressed_event);
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&released_event);
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  // |combobox_| will be allocated InitCombobox() and then owned by |widget_|.
  TestCombobox* combobox_;

  // Combobox does not take ownership of the model, hence it needs to be scoped.
  scoped_ptr<TestComboboxModel> model_;
};

TEST_F(ComboboxTest, KeyTest) {
  InitCombobox(NULL);
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(combobox_->selected_index() + 1, model_->GetItemCount());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(combobox_->selected_index(), 1);
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(combobox_->selected_index(), model_->GetItemCount() - 1);
}

// Check that if a combobox is disabled before it has a native wrapper, then the
// native wrapper inherits the disabled state when it gets created.
TEST_F(ComboboxTest, DisabilityTest) {
  model_.reset(new TestComboboxModel());

  ASSERT_FALSE(combobox_);
  combobox_ = new TestCombobox(model_.get());
  combobox_->SetEnabled(false);

  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget_->Init(params);
  View* container = new View();
  widget_->SetContentsView(container);
  container->AddChildView(combobox_);
  EXPECT_FALSE(combobox_->enabled());
}

// Verifies that we don't select a separator line in combobox when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipSeparatorSimple) {
  std::set<int> separators;
  separators.insert(2);
  InitCombobox(&separators);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the beginning of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorBeginning) {
  std::set<int> separators;
  separators.insert(0);
  InitCombobox(&separators);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(2, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(2, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the end of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorEnd) {
  std::set<int> separators;
  separators.insert(TestComboboxModel::kItemCount - 1);
  InitCombobox(&separators);
  combobox_->SetSelectedIndex(8);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(8, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(8, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the beginning of the combobox list when
// navigating through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtBeginning) {
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(2);
  InitCombobox(&separators);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(4, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the middle of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleAdjacentSeparatorsAtMiddle) {
  std::set<int> separators;
  separators.insert(4);
  separators.insert(5);
  separators.insert(6);
  InitCombobox(&separators);
  combobox_->SetSelectedIndex(3);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the end of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtEnd) {
  std::set<int> separators;
  separators.insert(7);
  separators.insert(8);
  separators.insert(9);
  InitCombobox(&separators);
  combobox_->SetSelectedIndex(6);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(5, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(6, combobox_->selected_index());
}

TEST_F(ComboboxTest, GetTextForRowTest) {
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(9);
  InitCombobox(&separators);
  for (int i = 0; i < combobox_->GetRowCount(); ++i) {
    if (separators.count(i) != 0) {
      EXPECT_TRUE(combobox_->GetTextForRow(i).empty()) << i;
    } else {
      EXPECT_EQ(ASCIIToUTF16(i % 2 == 0 ? "PEANUT BUTTER" : "JELLY"),
                combobox_->GetTextForRow(i)) << i;
    }
  }
}

// Verifies selecting the first matching value (and returning whether found).
TEST_F(ComboboxTest, SelectValue) {
  InitCombobox(NULL);
  ASSERT_EQ(model_->GetDefaultIndex(), combobox_->selected_index());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("PEANUT BUTTER")));
  EXPECT_EQ(0, combobox_->selected_index());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("JELLY")));
  EXPECT_EQ(1, combobox_->selected_index());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("BANANAS")));
  EXPECT_EQ(1, combobox_->selected_index());

  // With the action style, the selected index is always 0.
  combobox_->SetStyle(Combobox::STYLE_ACTION);
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("PEANUT BUTTER")));
  EXPECT_EQ(0, combobox_->selected_index());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("JELLY")));
  EXPECT_EQ(0, combobox_->selected_index());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("BANANAS")));
  EXPECT_EQ(0, combobox_->selected_index());
}

TEST_F(ComboboxTest, SelectIndexActionStyle) {
  InitCombobox(NULL);

  // With the action style, the selected index is always 0.
  combobox_->SetStyle(Combobox::STYLE_ACTION);
  combobox_->SetSelectedIndex(1);
  EXPECT_EQ(0, combobox_->selected_index());
  combobox_->SetSelectedIndex(2);
  EXPECT_EQ(0, combobox_->selected_index());
  combobox_->SetSelectedIndex(3);
  EXPECT_EQ(0, combobox_->selected_index());
}

TEST_F(ComboboxTest, ListenerHandlesDelete) {
  TestComboboxModel model;

  // |combobox| will be deleted on change.
  TestCombobox* combobox = new TestCombobox(&model);
  scoped_ptr<EvilListener> evil_listener(new EvilListener());
  combobox->set_listener(evil_listener.get());
  ASSERT_NO_FATAL_FAILURE(combobox->ExecuteCommand(2));
  EXPECT_TRUE(evil_listener->deleted());

  // With STYLE_ACTION
  // |combobox| will be deleted on change.
  combobox = new TestCombobox(&model);
  evil_listener.reset(new EvilListener());
  combobox->set_listener(evil_listener.get());
  combobox->SetStyle(Combobox::STYLE_ACTION);
  ASSERT_NO_FATAL_FAILURE(combobox->ExecuteCommand(2));
  EXPECT_TRUE(evil_listener->deleted());
}

TEST_F(ComboboxTest, Click) {
  InitCombobox(NULL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();

  // Click the left side. The menu is shown.
  TestMenuRunnerHandler* test_menu_runner_handler = new TestMenuRunnerHandler();
  scoped_ptr<MenuRunnerHandler> menu_runner_handler(test_menu_runner_handler);
  test::MenuRunnerTestAPI test_api(
      combobox_->dropdown_list_menu_runner_.get());
  test_api.SetMenuRunnerHandler(menu_runner_handler.Pass());
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_TRUE(test_menu_runner_handler->executed());
}

TEST_F(ComboboxTest, ClickButDisabled) {
  InitCombobox(NULL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();
  combobox_->SetEnabled(false);

  // Click the left side, but nothing happens since the combobox is disabled.
  TestMenuRunnerHandler* test_menu_runner_handler = new TestMenuRunnerHandler();
  scoped_ptr<MenuRunnerHandler> menu_runner_handler(test_menu_runner_handler);
  test::MenuRunnerTestAPI test_api(
      combobox_->dropdown_list_menu_runner_.get());
  test_api.SetMenuRunnerHandler(menu_runner_handler.Pass());
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_FALSE(test_menu_runner_handler->executed());
}

TEST_F(ComboboxTest, NotifyOnClickWithReturnKey) {
  InitCombobox(NULL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_NORMAL, the click event is ignored.
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_FALSE(listener.on_perform_action_called());

  // With STYLE_ACTION, the click event is notified.
  combobox_->SetStyle(Combobox::STYLE_ACTION);
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_TRUE(listener.on_perform_action_called());
  EXPECT_EQ(0, listener.perform_action_index());
}

TEST_F(ComboboxTest, NotifyOnClickWithSpaceKey) {
  InitCombobox(NULL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_NORMAL, the click event is ignored.
  SendKeyEvent(ui::VKEY_SPACE);
  EXPECT_FALSE(listener.on_perform_action_called());
  SendKeyEventWithType(ui::VKEY_SPACE, ui::ET_KEY_RELEASED);
  EXPECT_FALSE(listener.on_perform_action_called());

  // With STYLE_ACTION, the click event is notified after releasing.
  combobox_->SetStyle(Combobox::STYLE_ACTION);
  SendKeyEvent(ui::VKEY_SPACE);
  EXPECT_FALSE(listener.on_perform_action_called());
  SendKeyEventWithType(ui::VKEY_SPACE, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(listener.on_perform_action_called());
  EXPECT_EQ(0, listener.perform_action_index());
}

TEST_F(ComboboxTest, NotifyOnClickWithMouse) {
  InitCombobox(NULL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->SetStyle(Combobox::STYLE_ACTION);
  combobox_->Layout();

  // Click the right side (arrow button). The menu is shown.
  TestMenuRunnerHandler* test_menu_runner_handler = new TestMenuRunnerHandler();
  scoped_ptr<MenuRunnerHandler> menu_runner_handler(test_menu_runner_handler);
  scoped_ptr<test::MenuRunnerTestAPI> test_api(
      new test::MenuRunnerTestAPI(combobox_->dropdown_list_menu_runner_.get()));
  test_api->SetMenuRunnerHandler(menu_runner_handler.Pass());

  PerformClick(gfx::Point(combobox_->x() + combobox_->width() - 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_perform_action_called());
  EXPECT_TRUE(test_menu_runner_handler->executed());

  // Click the left side (text button). The click event is notified.
  test_menu_runner_handler = new TestMenuRunnerHandler();
  menu_runner_handler.reset(test_menu_runner_handler);
  test_api.reset(
      new test::MenuRunnerTestAPI(combobox_->dropdown_list_menu_runner_.get()));
  test_api->SetMenuRunnerHandler(menu_runner_handler.Pass());
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_TRUE(listener.on_perform_action_called());
  EXPECT_FALSE(test_menu_runner_handler->executed());
  EXPECT_EQ(0, listener.perform_action_index());
}

TEST_F(ComboboxTest, ConsumingPressKeyEvents) {
  InitCombobox(NULL);

  EXPECT_FALSE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE)));
  EXPECT_FALSE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE)));

  // When the combobox's style is STYLE_ACTION, pressing events of a space key
  // or an enter key will be consumed.
  combobox_->SetStyle(Combobox::STYLE_ACTION);
  EXPECT_TRUE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE)));
  EXPECT_TRUE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE)));
}

TEST_F(ComboboxTest, ContentWidth) {
  std::vector<std::string> values;
  VectorComboboxModel model(&values);
  TestCombobox combobox(&model);

  std::string long_item = "this is the long item";
  std::string short_item = "s";

  values.resize(1);
  values[0] = long_item;
  combobox.ModelChanged();

  const int long_item_width = combobox.content_size_.width();

  values[0] = short_item;
  combobox.ModelChanged();

  const int short_item_width = combobox.content_size_.width();

  values.resize(2);
  values[0] = short_item;
  values[1] = long_item;
  combobox.ModelChanged();

  // When the style is STYLE_NORMAL, the width will fit with the longest item.
  combobox.SetStyle(Combobox::STYLE_NORMAL);
  EXPECT_EQ(long_item_width, combobox.content_size_.width());

  // When the style is STYLE_ACTION, the width will fit with the first items'
  // width.
  combobox.SetStyle(Combobox::STYLE_ACTION);
  EXPECT_EQ(short_item_width, combobox.content_size_.width());
}

TEST_F(ComboboxTest, TypingPrefixNotifiesListener) {
  InitCombobox(NULL);

  TestComboboxListener listener;
  combobox_->set_listener(&listener);
  ui::TextInputClient* input_client =
      widget_->GetInputMethod()->GetTextInputClient();

  // Type the first character of the second menu item ("JELLY").
  input_client->InsertChar('J', ui::EF_NONE);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1, listener.perform_action_index());

  // Type the second character of "JELLY", item shouldn't change and
  // OnPerformAction() shouldn't be re-called.
  input_client->InsertChar('E', ui::EF_NONE);
  EXPECT_EQ(1, listener.actions_performed());
  EXPECT_EQ(1, listener.perform_action_index());

  // Clears the typed text.
  combobox_->OnBlur();
  combobox_->RequestFocus();

  // Type the first character of "PEANUT BUTTER", which should change the
  // selected index and perform an action.
  input_client->InsertChar('P', ui::EF_NONE);
  EXPECT_EQ(2, listener.actions_performed());
  EXPECT_EQ(2, listener.perform_action_index());
}

}  // namespace views
