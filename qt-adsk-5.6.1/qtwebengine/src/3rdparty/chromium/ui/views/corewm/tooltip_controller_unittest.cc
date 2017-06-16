// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller_test_helper.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "ui/wm/core/wm_state.h"
#include "ui/wm/public/tooltip_client.h"
#include "ui/wm/public/window_types.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif

using base::ASCIIToUTF16;

namespace views {
namespace corewm {
namespace test {
namespace {

views::Widget* CreateWidget(aura::Window* root) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
#if defined(OS_CHROMEOS)
  params.parent = root;
#else
  params.native_widget = new DesktopNativeWidgetAura(widget);
#endif
  params.bounds = gfx::Rect(0, 0, 200, 100);
  widget->Init(params);
  widget->Show();
  return widget;
}

TooltipController* GetController(Widget* widget) {
  return static_cast<TooltipController*>(
      aura::client::GetTooltipClient(
          widget->GetNativeWindow()->GetRootWindow()));
}

}  // namespace

class TooltipControllerTest : public aura::test::AuraTestBase {
 public:
  TooltipControllerTest() : view_(NULL) {}
  ~TooltipControllerTest() override {}

  void SetUp() override {
#if defined(OS_CHROMEOS)
    views_delegate_.reset(new TestViewsDelegate);
#else
    views_delegate_.reset(new DesktopTestViewsDelegate);
#endif

    aura::test::AuraTestBase::SetUp();
    new wm::DefaultActivationClient(root_window());
#if defined(OS_CHROMEOS)
    controller_.reset(new TooltipController(
          scoped_ptr<views::corewm::Tooltip>(
              new views::corewm::TooltipAura)));
    root_window()->AddPreTargetHandler(controller_.get());
    SetTooltipClient(root_window(), controller_.get());
#endif
    widget_.reset(CreateWidget(root_window()));
    widget_->SetContentsView(new View);
    view_ = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view_);
    view_->SetBoundsRect(widget_->GetContentsView()->GetLocalBounds());
    helper_.reset(new TooltipControllerTestHelper(
                      GetController(widget_.get())));
    generator_.reset(new ui::test::EventGenerator(GetRootWindow()));
  }

  void TearDown() override {
#if defined(OS_CHROMEOS)
    root_window()->RemovePreTargetHandler(controller_.get());
    aura::client::SetTooltipClient(root_window(), NULL);
    controller_.reset();
#endif
    generator_.reset();
    helper_.reset();
    widget_.reset();
    aura::test::AuraTestBase::TearDown();
    views_delegate_.reset();
  }

 protected:
  aura::Window* GetWindow() {
    return widget_->GetNativeWindow();
  }

  aura::Window* GetRootWindow() {
    return GetWindow()->GetRootWindow();
  }

  TooltipTestView* PrepareSecondView() {
    TooltipTestView* view2 = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view2);
    view_->SetBounds(0, 0, 100, 100);
    view2->SetBounds(100, 0, 100, 100);
    return view2;
  }

  scoped_ptr<views::Widget> widget_;
  TooltipTestView* view_;
  scoped_ptr<TooltipControllerTestHelper> helper_;
  scoped_ptr<ui::test::EventGenerator> generator_;

 private:
  scoped_ptr<TooltipController> controller_;

  scoped_ptr<views::TestViewsDelegate> views_delegate_;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest);
};

TEST_F(TooltipControllerTest, ViewTooltip) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());
  generator_->MoveMouseToCenterOf(GetWindow());

  EXPECT_EQ(GetWindow(), GetRootWindow()->GetEventHandlerForPoint(
      generator_->current_location()));
  base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  generator_->MoveMouseBy(1, 0);

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());
}

TEST_F(TooltipControllerTest, TooltipsInMultipleViews) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  PrepareSecondView();
  aura::Window* window = GetWindow();
  aura::Window* root_window = GetRootWindow();

  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_TRUE(helper_->IsTooltipVisible());
    EXPECT_EQ(window, root_window->GetEventHandlerForPoint(
            generator_->current_location()));
    base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_EQ(window, root_window->GetEventHandlerForPoint(
            generator_->current_location()));
    base::string16 expected_tooltip;  // = ""
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }
}

TEST_F(TooltipControllerTest, EnableOrDisableTooltips) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  generator_->MoveMouseRelativeTo(GetWindow(), view_->bounds().CenterPoint());
  base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // Disable tooltips and check again.
  helper_->controller()->SetTooltipsEnabled(false);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  helper_->FireTooltipTimer();
  EXPECT_FALSE(helper_->IsTooltipVisible());

  // Enable tooltips back and check again.
  helper_->controller()->SetTooltipsEnabled(true);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
}

// Verifies tooltip isn't shown if tooltip text consists entirely of whitespace.
TEST_F(TooltipControllerTest, DontShowEmptyTooltips) {
  view_->set_tooltip_text(ASCIIToUTF16("                     "));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  generator_->MoveMouseRelativeTo(GetWindow(), view_->bounds().CenterPoint());

  helper_->FireTooltipTimer();
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, TooltipHidesOnKeyPressAndStaysHiddenUntilChange) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  TooltipTestView* view2 = PrepareSecondView();
  view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 2"));

  aura::Window* window = GetWindow();

  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());

  generator_->PressKey(ui::VKEY_1, 0);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_FALSE(helper_->IsTooltipTimerRunning());
  EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; i++) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_FALSE(helper_->IsTooltipTimerRunning());
    EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());
    EXPECT_EQ(window,
              GetRootWindow()->GetEventHandlerForPoint(
                  generator_->current_location()));
    base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 1");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }

  // Now we move the mouse on to |view2|. It should re-start the tooltip timer.
  generator_->MoveMouseBy(1, 0);
  EXPECT_TRUE(helper_->IsTooltipTimerRunning());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());
  base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 2");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(window, helper_->GetTooltipWindow());
}

TEST_F(TooltipControllerTest, TooltipHidesOnTimeoutAndStaysHiddenUntilChange) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 1"));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(NULL, helper_->GetTooltipWindow());

  TooltipTestView* view2 = PrepareSecondView();
  view2->set_tooltip_text(ASCIIToUTF16("Tooltip Text for view 2"));

  aura::Window* window = GetWindow();

  // Fire tooltip timer so tooltip becomes visible.
  generator_->MoveMouseRelativeTo(window, view_->bounds().CenterPoint());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());

  helper_->FireTooltipShownTimer();
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_FALSE(helper_->IsTooltipTimerRunning());
  EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());

  // Moving the mouse inside |view1| should not change the state of the tooltip
  // or the timers.
  for (int i = 0; i < 49; ++i) {
    generator_->MoveMouseBy(1, 0);
    EXPECT_FALSE(helper_->IsTooltipVisible());
    EXPECT_FALSE(helper_->IsTooltipTimerRunning());
    EXPECT_FALSE(helper_->IsTooltipShownTimerRunning());
    EXPECT_EQ(window, GetRootWindow()->GetEventHandlerForPoint(
                  generator_->current_location()));
    base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 1");
    EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
    EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
    EXPECT_EQ(window, helper_->GetTooltipWindow());
  }

  // Now we move the mouse on to |view2|. It should re-start the tooltip timer.
  generator_->MoveMouseBy(1, 0);
  EXPECT_TRUE(helper_->IsTooltipTimerRunning());
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->IsTooltipShownTimerRunning());
  base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text for view 2");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(window));
  EXPECT_EQ(expected_tooltip, helper_->GetTooltipText());
  EXPECT_EQ(window, helper_->GetTooltipWindow());
}

// Verifies a mouse exit event hides the tooltips.
TEST_F(TooltipControllerTest, HideOnExit) {
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  generator_->MoveMouseToCenterOf(GetWindow());
  base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  generator_->SendMouseExit();
  EXPECT_FALSE(helper_->IsTooltipVisible());
}

TEST_F(TooltipControllerTest, ReshowOnClickAfterEnterExit) {
  // Owned by |view_|.
  TooltipTestView* v1 = new TooltipTestView;
  TooltipTestView* v2 = new TooltipTestView;
  view_->AddChildView(v1);
  view_->AddChildView(v2);
  gfx::Rect view_bounds(view_->GetLocalBounds());
  view_bounds.set_height(view_bounds.height() / 2);
  v1->SetBoundsRect(view_bounds);
  view_bounds.set_y(view_bounds.height());
  v2->SetBoundsRect(view_bounds);
  const base::string16 v1_tt(ASCIIToUTF16("v1"));
  const base::string16 v2_tt(ASCIIToUTF16("v2"));
  v1->set_tooltip_text(v1_tt);
  v2->set_tooltip_text(v2_tt);

  gfx::Point v1_point(1, 1);
  View::ConvertPointToWidget(v1, &v1_point);
  generator_->MoveMouseRelativeTo(GetWindow(), v1_point);

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(v1_tt, helper_->GetTooltipText());

  // Press the mouse, move to v2 and back to v1.
  generator_->ClickLeftButton();

  gfx::Point v2_point(1, 1);
  View::ConvertPointToWidget(v2, &v2_point);
  generator_->MoveMouseRelativeTo(GetWindow(), v2_point);
  generator_->MoveMouseRelativeTo(GetWindow(), v1_point);

  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(v1_tt, helper_->GetTooltipText());
}

namespace {

// Returns the index of |window| in its parent's children.
int IndexInParent(const aura::Window* window) {
  aura::Window::Windows::const_iterator i =
      std::find(window->parent()->children().begin(),
                window->parent()->children().end(),
                window);
  return i == window->parent()->children().end() ? -1 :
      static_cast<int>(i - window->parent()->children().begin());
}

}  // namespace

class TooltipControllerCaptureTest : public TooltipControllerTest {
 public:
  TooltipControllerCaptureTest() {}
  ~TooltipControllerCaptureTest() override {}

  void SetUp() override {
    TooltipControllerTest::SetUp();
    aura::client::SetScreenPositionClient(GetRootWindow(),
                                          &screen_position_client_);
#if !defined(OS_CHROMEOS)
    desktop_screen_.reset(CreateDesktopScreen());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   desktop_screen_.get());
#endif
  }

  void TearDown() override {
#if !defined(OS_CHROMEOS)
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen());
    desktop_screen_.reset();
#endif
    aura::client::SetScreenPositionClient(GetRootWindow(), NULL);
    TooltipControllerTest::TearDown();
  }

 private:
  wm::DefaultScreenPositionClient screen_position_client_;
  scoped_ptr<gfx::Screen> desktop_screen_;

  DISALLOW_COPY_AND_ASSIGN(TooltipControllerCaptureTest);
};

// Verifies when capture is released the TooltipController resets state.
// Flaky on all builders.  http://crbug.com/388268
TEST_F(TooltipControllerCaptureTest, DISABLED_CloseOnCaptureLost) {
  view_->GetWidget()->SetCapture(view_);
  RunAllPendingInMessageLoop();
  view_->set_tooltip_text(ASCIIToUTF16("Tooltip Text"));
  generator_->MoveMouseToCenterOf(GetWindow());
  base::string16 expected_tooltip = ASCIIToUTF16("Tooltip Text");
  EXPECT_EQ(expected_tooltip, aura::client::GetTooltipText(GetWindow()));
  EXPECT_EQ(base::string16(), helper_->GetTooltipText());
  EXPECT_EQ(GetWindow(), helper_->GetTooltipWindow());

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  view_->GetWidget()->ReleaseCapture();
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->GetTooltipWindow() == NULL);
}

// Disabled on linux as DesktopScreenX11::GetWindowAtScreenPoint() doesn't
// consider z-order.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
// Verifies the correct window is found for tooltips when there is a capture.
TEST_F(TooltipControllerCaptureTest, MAYBE_Capture) {
  const base::string16 tooltip_text(ASCIIToUTF16("1"));
  const base::string16 tooltip_text2(ASCIIToUTF16("2"));

  widget_->SetBounds(gfx::Rect(0, 0, 200, 200));
  view_->set_tooltip_text(tooltip_text);

  scoped_ptr<views::Widget> widget2(CreateWidget(root_window()));
  widget2->SetContentsView(new View);
  TooltipTestView* view2 = new TooltipTestView;
  widget2->GetContentsView()->AddChildView(view2);
  view2->set_tooltip_text(tooltip_text2);
  widget2->SetBounds(gfx::Rect(0, 0, 200, 200));
  view2->SetBoundsRect(widget2->GetContentsView()->GetLocalBounds());

  widget_->SetCapture(view_);
  EXPECT_TRUE(widget_->HasCapture());
  widget2->Show();
  EXPECT_GE(IndexInParent(widget2->GetNativeWindow()),
            IndexInParent(widget_->GetNativeWindow()));

  generator_->MoveMouseRelativeTo(widget_->GetNativeWindow(),
                                  view_->bounds().CenterPoint());

  EXPECT_TRUE(helper_->IsTooltipTimerRunning());
  helper_->FireTooltipTimer();
  // Even though the mouse is over a window with a tooltip it shouldn't be
  // picked up because the windows don't have the same value for
  // |TooltipManager::kGroupingPropertyKey|.
  EXPECT_TRUE(helper_->GetTooltipText().empty());

  // Now make both the windows have same transient value for
  // kGroupingPropertyKey. In this case the tooltip should be picked up from
  // |widget2| (because the mouse is over it).
  const int grouping_key = 1;
  widget_->SetNativeWindowProperty(TooltipManager::kGroupingPropertyKey,
                                   reinterpret_cast<void*>(grouping_key));
  widget2->SetNativeWindowProperty(TooltipManager::kGroupingPropertyKey,
                                   reinterpret_cast<void*>(grouping_key));
  generator_->MoveMouseBy(1, 10);
  EXPECT_TRUE(helper_->IsTooltipTimerRunning());
  helper_->FireTooltipTimer();
  EXPECT_EQ(tooltip_text2, helper_->GetTooltipText());

  widget2.reset();
}

namespace {

class TestTooltip : public Tooltip {
 public:
  TestTooltip() : is_visible_(false) {}
  ~TestTooltip() override {}

  const base::string16& tooltip_text() const { return tooltip_text_; }

  // Tooltip:
  int GetMaxWidth(const gfx::Point& location,
                  aura::Window* context) const override {
    return 100;
  }
  void SetText(aura::Window* window,
               const base::string16& tooltip_text,
               const gfx::Point& location) override {
    tooltip_text_ = tooltip_text;
    location_ = location;
  }
  void Show() override { is_visible_ = true; }
  void Hide() override { is_visible_ = false; }
  bool IsVisible() override { return is_visible_; }
  const gfx::Point& location() { return location_; }

 private:
  bool is_visible_;
  base::string16 tooltip_text_;
  gfx::Point location_;

  DISALLOW_COPY_AND_ASSIGN(TestTooltip);
};

}  // namespace

// Use for tests that don't depend upon views.
class TooltipControllerTest2 : public aura::test::AuraTestBase {
 public:
  TooltipControllerTest2() : test_tooltip_(new TestTooltip) {}
  ~TooltipControllerTest2() override {}

  void SetUp() override {
    wm_state_.reset(new wm::WMState);
    aura::test::AuraTestBase::SetUp();
    new wm::DefaultActivationClient(root_window());
    controller_.reset(new TooltipController(
                          scoped_ptr<corewm::Tooltip>(test_tooltip_)));
    root_window()->AddPreTargetHandler(controller_.get());
    SetTooltipClient(root_window(), controller_.get());
    helper_.reset(new TooltipControllerTestHelper(controller_.get()));
    generator_.reset(new ui::test::EventGenerator(root_window()));
  }

  void TearDown() override {
    root_window()->RemovePreTargetHandler(controller_.get());
    aura::client::SetTooltipClient(root_window(), NULL);
    controller_.reset();
    generator_.reset();
    helper_.reset();
    aura::test::AuraTestBase::TearDown();
    wm_state_.reset();
  }

 protected:
  // Owned by |controller_|.
  TestTooltip* test_tooltip_;
  scoped_ptr<TooltipControllerTestHelper> helper_;
  scoped_ptr<ui::test::EventGenerator> generator_;

 private:
  scoped_ptr<TooltipController> controller_;
  scoped_ptr<wm::WMState> wm_state_;

  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest2);
};

TEST_F(TooltipControllerTest2, VerifyLeadingTrailingWhitespaceStripped) {
  aura::test::TestWindowDelegate test_delegate;
  scoped_ptr<aura::Window> window(
      CreateNormalWindow(100, root_window(), &test_delegate));
  window->SetBounds(gfx::Rect(0, 0, 300, 300));
  base::string16 tooltip_text(ASCIIToUTF16(" \nx  "));
  aura::client::SetTooltipText(window.get(), &tooltip_text);
  generator_->MoveMouseToCenterOf(window.get());
  helper_->FireTooltipTimer();
  EXPECT_EQ(ASCIIToUTF16("x"), test_tooltip_->tooltip_text());
}

// Verifies that tooltip is hidden and tooltip window closed upon cancel mode.
TEST_F(TooltipControllerTest2, CloseOnCancelMode) {
  aura::test::TestWindowDelegate test_delegate;
  scoped_ptr<aura::Window> window(
      CreateNormalWindow(100, root_window(), &test_delegate));
  window->SetBounds(gfx::Rect(0, 0, 300, 300));
  base::string16 tooltip_text(ASCIIToUTF16("Tooltip Text"));
  aura::client::SetTooltipText(window.get(), &tooltip_text);
  generator_->MoveMouseToCenterOf(window.get());

  // Fire tooltip timer so tooltip becomes visible.
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());

  // Send OnCancelMode event and verify that tooltip becomes invisible and
  // the tooltip window is closed.
  ui::CancelModeEvent event;
  helper_->controller()->OnCancelMode(&event);
  EXPECT_FALSE(helper_->IsTooltipVisible());
  EXPECT_TRUE(helper_->GetTooltipWindow() == NULL);
}

// Use for tests that need both views and a TestTooltip.
class TooltipControllerTest3 : public aura::test::AuraTestBase {
 public:
  TooltipControllerTest3() : test_tooltip_(new TestTooltip) {}
  ~TooltipControllerTest3() override {}

  void SetUp() override {
    wm_state_.reset(new wm::WMState);
    aura::test::AuraTestBase::SetUp();
    new wm::DefaultActivationClient(root_window());

    widget_.reset(CreateWidget(root_window()));
    widget_->SetContentsView(new View);
    view_ = new TooltipTestView;
    widget_->GetContentsView()->AddChildView(view_);
    view_->SetBoundsRect(widget_->GetContentsView()->GetLocalBounds());

    generator_.reset(new ui::test::EventGenerator(GetRootWindow()));
    controller_.reset(new TooltipController(
        scoped_ptr<views::corewm::Tooltip>(test_tooltip_)));
    GetRootWindow()->RemovePreTargetHandler(
        static_cast<TooltipController*>(aura::client::GetTooltipClient(
            widget_->GetNativeWindow()->GetRootWindow())));
    GetRootWindow()->AddPreTargetHandler(controller_.get());
    helper_.reset(new TooltipControllerTestHelper(controller_.get()));
    SetTooltipClient(GetRootWindow(), controller_.get());
  }

  void TearDown() override {
    GetRootWindow()->RemovePreTargetHandler(controller_.get());
    aura::client::SetTooltipClient(GetRootWindow(), NULL);

    controller_.reset();
    generator_.reset();
    helper_.reset();
    widget_.reset();
    aura::test::AuraTestBase::TearDown();
    wm_state_.reset();
  }

  aura::Window* GetWindow() { return widget_->GetNativeWindow(); }

 protected:
  // Owned by |controller_|.
  TestTooltip* test_tooltip_;
  scoped_ptr<TooltipControllerTestHelper> helper_;
  scoped_ptr<ui::test::EventGenerator> generator_;
  scoped_ptr<views::Widget> widget_;
  TooltipTestView* view_;

 private:
  scoped_ptr<TooltipController> controller_;
  scoped_ptr<wm::WMState> wm_state_;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  aura::Window* GetRootWindow() { return GetWindow()->GetRootWindow(); }

  DISALLOW_COPY_AND_ASSIGN(TooltipControllerTest3);
};

TEST_F(TooltipControllerTest3, TooltipPositionChangesOnTwoViewsWithSameLabel) {
  // Owned by |view_|.
  // These two views have the same tooltip text
  TooltipTestView* v1 = new TooltipTestView;
  TooltipTestView* v2 = new TooltipTestView;
  // v1_1 is a view inside v1 that has an identical tooltip text to that of v1
  // and v2
  TooltipTestView* v1_1 = new TooltipTestView;
  // v2_1 is a view inside v2 that has an identical tooltip text to that of v1
  // and v2
  TooltipTestView* v2_1 = new TooltipTestView;
  // v2_2 is a view inside v2 with the tooltip text different from all the
  // others
  TooltipTestView* v2_2 = new TooltipTestView;

  // Setup all the views' relations
  view_->AddChildView(v1);
  view_->AddChildView(v2);
  v1->AddChildView(v1_1);
  v2->AddChildView(v2_1);
  v2->AddChildView(v2_2);
  const base::string16 reference_string(
      base::ASCIIToUTF16("Identical Tooltip Text"));
  const base::string16 alternative_string(
      base::ASCIIToUTF16("Another Shrubbery"));
  v1->set_tooltip_text(reference_string);
  v2->set_tooltip_text(reference_string);
  v1_1->set_tooltip_text(reference_string);
  v2_1->set_tooltip_text(reference_string);
  v2_2->set_tooltip_text(alternative_string);

  // Set views' bounds
  gfx::Rect view_bounds(view_->GetLocalBounds());
  view_bounds.set_height(view_bounds.height() / 2);
  v1->SetBoundsRect(view_bounds);
  v1_1->SetBounds(0, 0, 3, 3);
  view_bounds.set_y(view_bounds.height());
  v2->SetBoundsRect(view_bounds);
  v2_2->SetBounds(view_bounds.width() - 3, view_bounds.height() - 3, 3, 3);
  v2_1->SetBounds(0, 0, 3, 3);

  // Test whether a toolbar appears on v1
  gfx::Point center = v1->bounds().CenterPoint();
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(reference_string, helper_->GetTooltipText());
  gfx::Point tooltip_bounds1 = test_tooltip_->location();

  // Test whether the toolbar changes position on mouse over v2
  center = v2->bounds().CenterPoint();
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  helper_->FireTooltipTimer();
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(reference_string, helper_->GetTooltipText());
  gfx::Point tooltip_bounds2 = test_tooltip_->location();

  EXPECT_NE(tooltip_bounds1, gfx::Point());
  EXPECT_NE(tooltip_bounds2, gfx::Point());
  EXPECT_NE(tooltip_bounds1, tooltip_bounds2);

  // Test if the toolbar does not change position on encountering a contained
  // view with the same tooltip text
  center = v2_1->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(v2_1, view_, &center);
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  helper_->FireTooltipTimer();
  gfx::Point tooltip_bounds2_1 = test_tooltip_->location();

  EXPECT_NE(tooltip_bounds2, tooltip_bounds2_1);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(reference_string, helper_->GetTooltipText());

  // Test if the toolbar changes position on encountering a contained
  // view with a different tooltip text
  center = v2_2->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(v2_2, view_, &center);
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  helper_->FireTooltipTimer();
  gfx::Point tooltip_bounds2_2 = test_tooltip_->location();

  EXPECT_NE(tooltip_bounds2_1, tooltip_bounds2_2);
  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(alternative_string, helper_->GetTooltipText());

  // Test if moving from a view that is contained by a larger view, both with
  // the same tooltip text, does not change tooltip's position.
  center = v1_1->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(v1_1, view_, &center);
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  helper_->FireTooltipTimer();
  gfx::Point tooltip_bounds1_1 = test_tooltip_->location();

  EXPECT_TRUE(helper_->IsTooltipVisible());
  EXPECT_EQ(reference_string, helper_->GetTooltipText());

  center = v1->bounds().CenterPoint();
  generator_->MoveMouseRelativeTo(GetWindow(), center);
  helper_->FireTooltipTimer();
  tooltip_bounds1 = test_tooltip_->location();

  EXPECT_NE(tooltip_bounds1_1, tooltip_bounds1);
  EXPECT_EQ(reference_string, helper_->GetTooltipText());
}

}  // namespace test
}  // namespace corewm
}  // namespace views
