// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "base/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/screen.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/dispatcher_client.h"

namespace views {
namespace test {

typedef ViewsTestBase DesktopNativeWidgetAuraTest;

// Verifies creating a Widget with a parent that is not in a RootWindow doesn't
// crash.
TEST_F(DesktopNativeWidgetAuraTest, CreateWithParentNotInRootWindow) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->Init(ui::LAYER_NOT_DRAWN);
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.parent = window.get();
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(params);
}

// Verifies that the Aura windows making up a widget instance have the correct
// bounds after the widget is resized.
TEST_F(DesktopNativeWidgetAuraTest, DesktopAuraWindowSizeTest) {
  Widget widget;

  // On Linux we test this with popup windows because the WM may ignore the size
  // suggestion for normal windows.
#if defined(OS_LINUX)
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
#else
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
#endif

  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);

  gfx::Rect bounds(0, 0, 100, 100);
  widget.SetBounds(bounds);
  widget.Show();

  EXPECT_EQ(bounds.ToString(),
            widget.GetNativeView()->GetRootWindow()->bounds().ToString());
  EXPECT_EQ(bounds.ToString(), widget.GetNativeView()->bounds().ToString());
  EXPECT_EQ(bounds.ToString(),
            widget.GetNativeView()->parent()->bounds().ToString());

  gfx::Rect new_bounds(0, 0, 200, 200);
  widget.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds.ToString(),
            widget.GetNativeView()->GetRootWindow()->bounds().ToString());
  EXPECT_EQ(new_bounds.ToString(), widget.GetNativeView()->bounds().ToString());
  EXPECT_EQ(new_bounds.ToString(),
            widget.GetNativeView()->parent()->bounds().ToString());
}

// Verifies GetNativeView() is initially hidden. If the native view is initially
// shown then animations can not be disabled.
TEST_F(DesktopNativeWidgetAuraTest, NativeViewInitiallyHidden) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);
  EXPECT_FALSE(widget.GetNativeView()->IsVisible());
}

// Verify that the cursor state is shared between two native widgets.
TEST_F(DesktopNativeWidgetAuraTest, GlobalCursorState) {
  // Create two native widgets, each owning different root windows.
  Widget widget_a;
  Widget::InitParams init_params_a =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params_a.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  DesktopNativeWidgetAura* desktop_native_widget_aura_a =
      new DesktopNativeWidgetAura(&widget_a);
  init_params_a.native_widget = desktop_native_widget_aura_a;
  widget_a.Init(init_params_a);

  Widget widget_b;
  Widget::InitParams init_params_b =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params_b.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  DesktopNativeWidgetAura* desktop_native_widget_aura_b =
      new DesktopNativeWidgetAura(&widget_b);
  init_params_b.native_widget = desktop_native_widget_aura_b;
  widget_b.Init(init_params_b);

  aura::client::CursorClient* cursor_client_a = aura::client::GetCursorClient(
      desktop_native_widget_aura_a->host()->window());
  aura::client::CursorClient* cursor_client_b = aura::client::GetCursorClient(
      desktop_native_widget_aura_b->host()->window());

  // Verify the cursor can be locked using one client and unlocked using
  // another.
  EXPECT_FALSE(cursor_client_a->IsCursorLocked());
  EXPECT_FALSE(cursor_client_b->IsCursorLocked());

  cursor_client_a->LockCursor();
  EXPECT_TRUE(cursor_client_a->IsCursorLocked());
  EXPECT_TRUE(cursor_client_b->IsCursorLocked());

  cursor_client_b->UnlockCursor();
  EXPECT_FALSE(cursor_client_a->IsCursorLocked());
  EXPECT_FALSE(cursor_client_b->IsCursorLocked());

  // Verify that mouse events can be disabled using one client and then
  // re-enabled using another. Note that disabling mouse events should also
  // have the side effect of making the cursor invisible.
  EXPECT_TRUE(cursor_client_a->IsCursorVisible());
  EXPECT_TRUE(cursor_client_b->IsCursorVisible());
  EXPECT_TRUE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client_b->IsMouseEventsEnabled());

  cursor_client_b->DisableMouseEvents();
  EXPECT_FALSE(cursor_client_a->IsCursorVisible());
  EXPECT_FALSE(cursor_client_b->IsCursorVisible());
  EXPECT_FALSE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client_b->IsMouseEventsEnabled());

  cursor_client_a->EnableMouseEvents();
  EXPECT_TRUE(cursor_client_a->IsCursorVisible());
  EXPECT_TRUE(cursor_client_b->IsCursorVisible());
  EXPECT_TRUE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client_b->IsMouseEventsEnabled());

  // Verify that setting the cursor using one cursor client
  // will set it for all root windows.
  EXPECT_EQ(ui::kCursorNone, cursor_client_a->GetCursor().native_type());
  EXPECT_EQ(ui::kCursorNone, cursor_client_b->GetCursor().native_type());

  cursor_client_b->SetCursor(ui::kCursorPointer);
  EXPECT_EQ(ui::kCursorPointer, cursor_client_a->GetCursor().native_type());
  EXPECT_EQ(ui::kCursorPointer, cursor_client_b->GetCursor().native_type());

  // Verify that hiding the cursor using one cursor client will
  // hide it for all root windows. Note that hiding the cursor
  // should not disable mouse events.
  cursor_client_a->HideCursor();
  EXPECT_FALSE(cursor_client_a->IsCursorVisible());
  EXPECT_FALSE(cursor_client_b->IsCursorVisible());
  EXPECT_TRUE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client_b->IsMouseEventsEnabled());

  // Verify that the visibility state cannot be changed using one
  // cursor client when the cursor was locked using another.
  cursor_client_b->LockCursor();
  cursor_client_a->ShowCursor();
  EXPECT_FALSE(cursor_client_a->IsCursorVisible());
  EXPECT_FALSE(cursor_client_b->IsCursorVisible());

  // Verify the cursor becomes visible on unlock (since a request
  // to make it visible was queued up while the cursor was locked).
  cursor_client_b->UnlockCursor();
  EXPECT_TRUE(cursor_client_a->IsCursorVisible());
  EXPECT_TRUE(cursor_client_b->IsCursorVisible());
}

// Verifies FocusController doesn't attempt to access |content_window_| during
// destruction. Previously the FocusController was destroyed after the window.
// This could be problematic as FocusController references |content_window_| and
// could attempt to use it after |content_window_| was destroyed. This test
// verifies this doesn't happen. Note that this test only failed under ASAN.
TEST_F(DesktopNativeWidgetAuraTest, DontAccessContentWindowDuringDestruction) {
  aura::test::TestWindowDelegate delegate;
  {
    Widget widget;
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    DesktopNativeWidgetAura* desktop_native_widget_aura =
        new DesktopNativeWidgetAura(&widget);
    init_params.native_widget = desktop_native_widget_aura;
    widget.Init(init_params);

    // Owned by |widget|.
    aura::Window* window = new aura::Window(&delegate);
    window->Init(ui::LAYER_NOT_DRAWN);
    window->Show();
    widget.GetNativeWindow()->parent()->AddChild(window);

    widget.Show();
  }
}

void QuitNestedLoopAndCloseWidget(scoped_ptr<Widget> widget,
                                  base::Closure* quit_runloop) {
  quit_runloop->Run();
}

// Verifies that a widget can be destroyed when running a nested message-loop.
TEST_F(DesktopNativeWidgetAuraTest, WidgetCanBeDestroyedFromNestedLoop) {
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  widget->Init(params);
  widget->Show();

  aura::Window* window = widget->GetNativeView();
  aura::Window* root = window->GetRootWindow();
  aura::client::DispatcherClient* client =
      aura::client::GetDispatcherClient(root);

  // Post a task that terminates the nested loop and destroyes the widget. This
  // task will be executed from the nested loop initiated with the call to
  // |RunWithDispatcher()| below.
  aura::client::DispatcherRunLoop run_loop(client, NULL);
  base::Closure quit_runloop = run_loop.QuitClosure();
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&QuitNestedLoopAndCloseWidget,
                                      base::Passed(&widget),
                                      base::Unretained(&quit_runloop)));
  run_loop.Run();
}

// This class provides functionality to create fullscreen and top level popup
// windows. It additionally tests whether the destruction of these windows
// occurs correctly in desktop AURA without crashing.
// It provides facilities to test the following cases:-
// 1. Child window destroyed which should lead to the destruction of the
//    parent.
// 2. Parent window destroyed which should lead to the child being destroyed.
class DesktopAuraTopLevelWindowTest : public aura::WindowObserver {
 public:
  DesktopAuraTopLevelWindowTest()
      : top_level_widget_(NULL),
        owned_window_(NULL),
        owner_destroyed_(false),
        owned_window_destroyed_(false),
        use_async_mode_(true) {}

  ~DesktopAuraTopLevelWindowTest() override {
    EXPECT_TRUE(owner_destroyed_);
    EXPECT_TRUE(owned_window_destroyed_);
    top_level_widget_ = NULL;
    owned_window_ = NULL;
  }

  void CreateTopLevelWindow(const gfx::Rect& bounds, bool fullscreen) {
    Widget::InitParams init_params;
    init_params.type = Widget::InitParams::TYPE_WINDOW;
    init_params.bounds = bounds;
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.layer_type = ui::LAYER_NOT_DRAWN;
    init_params.accept_events = fullscreen;

    widget_.Init(init_params);

    owned_window_ = new aura::Window(&child_window_delegate_);
    owned_window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    owned_window_->SetName("TestTopLevelWindow");
    if (fullscreen) {
      owned_window_->SetProperty(aura::client::kShowStateKey,
                                 ui::SHOW_STATE_FULLSCREEN);
    } else {
      owned_window_->SetType(ui::wm::WINDOW_TYPE_MENU);
    }
    owned_window_->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(
        owned_window_,
        widget_.GetNativeView()->GetRootWindow(),
        gfx::Rect(0, 0, 1900, 1600));
    owned_window_->Show();
    owned_window_->AddObserver(this);

    ASSERT_TRUE(owned_window_->parent() != NULL);
    owned_window_->parent()->AddObserver(this);

    top_level_widget_ =
        views::Widget::GetWidgetForNativeView(owned_window_->parent());
    ASSERT_TRUE(top_level_widget_ != NULL);
  }

  void DestroyOwnedWindow() {
    ASSERT_TRUE(owned_window_ != NULL);
    // If async mode is off then clean up state here.
    if (!use_async_mode_) {
      owned_window_->RemoveObserver(this);
      owned_window_->parent()->RemoveObserver(this);
      owner_destroyed_ = true;
      owned_window_destroyed_ = true;
    }
    delete owned_window_;
  }

  void DestroyOwnerWindow() {
    ASSERT_TRUE(top_level_widget_ != NULL);
    top_level_widget_->CloseNow();
  }

  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    if (window == owned_window_) {
      owned_window_destroyed_ = true;
    } else if (window == top_level_widget_->GetNativeView()) {
      owner_destroyed_ = true;
    } else {
      ADD_FAILURE() << "Unexpected window destroyed callback: " << window;
    }
  }

  aura::Window* owned_window() {
    return owned_window_;
  }

  views::Widget* top_level_widget() {
    return top_level_widget_;
  }

  void set_use_async_mode(bool async_mode) {
    use_async_mode_ = async_mode;
  }

 private:
  views::Widget widget_;
  views::Widget* top_level_widget_;
  aura::Window* owned_window_;
  bool owner_destroyed_;
  bool owned_window_destroyed_;
  aura::test::TestWindowDelegate child_window_delegate_;
  // This flag controls whether we need to wait for the destruction to complete
  // before finishing the test. Defaults to true.
  bool use_async_mode_;

  DISALLOW_COPY_AND_ASSIGN(DesktopAuraTopLevelWindowTest);
};

class DesktopAuraWidgetTest : public WidgetTest {
 public:
  DesktopAuraWidgetTest() {}

  void SetUp() override {
    ViewsTestBase::SetUp();
    views_delegate()->set_use_desktop_native_widgets(true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopAuraWidgetTest);
};

TEST_F(DesktopAuraWidgetTest, FullscreenWindowDestroyedBeforeOwnerTest) {
  DesktopAuraTopLevelWindowTest fullscreen_window;
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), true));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.DestroyOwnedWindow());
  RunPendingMessages();
}

TEST_F(DesktopAuraWidgetTest, FullscreenWindowOwnerDestroyed) {
  DesktopAuraTopLevelWindowTest fullscreen_window;
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), true));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.DestroyOwnerWindow());
  RunPendingMessages();
}

TEST_F(DesktopAuraWidgetTest, TopLevelOwnedPopupTest) {
  DesktopAuraTopLevelWindowTest popup_window;
  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
  RunPendingMessages();
}

// This test validates that when a top level owned popup Aura window is
// resized, the widget is resized as well.
TEST_F(DesktopAuraWidgetTest, TopLevelOwnedPopupResizeTest) {
  DesktopAuraTopLevelWindowTest popup_window;

  popup_window.set_use_async_mode(false);

  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  gfx::Rect new_size(0, 0, 400, 400);
  popup_window.owned_window()->SetBounds(new_size);

  EXPECT_EQ(popup_window.top_level_widget()->GetNativeView()->bounds().size(),
            new_size.size());

  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
}

// This test validates that when a top level owned popup Aura window is
// repositioned, the widget is repositioned as well.
TEST_F(DesktopAuraWidgetTest, TopLevelOwnedPopupRepositionTest) {
  DesktopAuraTopLevelWindowTest popup_window;

  popup_window.set_use_async_mode(false);

  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  gfx::Rect new_pos(10, 10, 400, 400);
  popup_window.owned_window()->SetBoundsInScreen(
      new_pos,
      gfx::Screen::GetScreenFor(
          popup_window.owned_window())->GetDisplayNearestPoint(gfx::Point()));

  EXPECT_EQ(new_pos,
            popup_window.top_level_widget()->GetWindowBoundsInScreen());

  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
}

// The following code verifies we can correctly destroy a Widget from a mouse
// enter/exit. We could test move/drag/enter/exit but in general we don't run
// nested message loops from such events, nor has the code ever really dealt
// with this situation.

// Generates two moves (first generates enter, second real move), a press, drag
// and release stopping at |last_event_type|.
void GenerateMouseEvents(Widget* widget, ui::EventType last_event_type) {
  const gfx::Rect screen_bounds(widget->GetWindowBoundsInScreen());
  ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, screen_bounds.CenterPoint(),
                            screen_bounds.CenterPoint(), ui::EventTimeForNow(),
                            0, 0);
  ui::EventProcessor* dispatcher = WidgetTest::GetEventProcessor(widget);
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&move_event);
  if (last_event_type == ui::ET_MOUSE_ENTERED || details.dispatcher_destroyed)
    return;
  details = dispatcher->OnEventFromSource(&move_event);
  if (last_event_type == ui::ET_MOUSE_MOVED || details.dispatcher_destroyed)
    return;

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, screen_bounds.CenterPoint(),
                             screen_bounds.CenterPoint(), ui::EventTimeForNow(),
                             0, 0);
  details = dispatcher->OnEventFromSource(&press_event);
  if (last_event_type == ui::ET_MOUSE_PRESSED || details.dispatcher_destroyed)
    return;

  gfx::Point end_point(screen_bounds.CenterPoint());
  end_point.Offset(1, 1);
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, end_point, end_point,
                            ui::EventTimeForNow(), 0, 0);
  details = dispatcher->OnEventFromSource(&drag_event);
  if (last_event_type == ui::ET_MOUSE_DRAGGED || details.dispatcher_destroyed)
    return;

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, end_point, end_point,
                               ui::EventTimeForNow(), 0, 0);
  details = dispatcher->OnEventFromSource(&release_event);
  if (details.dispatcher_destroyed)
    return;
}

// Creates a widget and invokes GenerateMouseEvents() with |last_event_type|.
void RunCloseWidgetDuringDispatchTest(WidgetTest* test,
                                      ui::EventType last_event_type) {
  // |widget| is deleted by CloseWidgetView.
  Widget* widget = new Widget;
  Widget::InitParams params =
      test->CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget = new PlatformDesktopNativeWidget(widget);
  params.bounds = gfx::Rect(0, 0, 50, 100);
  widget->Init(params);
  widget->SetContentsView(new CloseWidgetView(last_event_type));
  widget->Show();
  GenerateMouseEvents(widget, last_event_type);
}

// Verifies deleting the widget from a mouse pressed event doesn't crash.
TEST_F(DesktopAuraWidgetTest, CloseWidgetDuringMousePress) {
  RunCloseWidgetDuringDispatchTest(this, ui::ET_MOUSE_PRESSED);
}

// Verifies deleting the widget from a mouse released event doesn't crash.
TEST_F(DesktopAuraWidgetTest, CloseWidgetDuringMouseReleased) {
  RunCloseWidgetDuringDispatchTest(this, ui::ET_MOUSE_RELEASED);
}

}  // namespace test
}  // namespace views
