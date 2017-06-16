// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/test_timeouts.h"
#import "testing/gtest_mac.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/events/test/event_generator.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/native_cursor.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/window/dialog_delegate.h"

// Donates an implementation of -[NSAnimation stopAnimation] which calls the
// original implementation, then quits a nested run loop.
@interface TestStopAnimationWaiter : NSObject
@end

@interface ConstrainedWindowAnimationBase (TestingAPI)
- (void)setWindowStateForEnd;
@end

@interface NSWindow (PrivateAPI)
- (BOOL)_isTitleHidden;
@end

namespace views {
namespace test {

// Tests for parts of NativeWidgetMac not covered by BridgedNativeWidget, which
// need access to Cocoa APIs.
class NativeWidgetMacTest : public WidgetTest {
 public:
  NativeWidgetMacTest() {}

  // The content size of NSWindows made by MakeNativeParent().
  NSRect ParentRect() const { return NSMakeRect(100, 100, 300, 200); }

  // Make a native NSWindow to use as a parent.
  NSWindow* MakeNativeParent() {
    native_parent_.reset(
        [[NSWindow alloc] initWithContentRect:ParentRect()
                                    styleMask:NSBorderlessWindowMask
                                      backing:NSBackingStoreBuffered
                                        defer:NO]);
    [native_parent_ setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.
    [native_parent_ makeKeyAndOrderFront:nil];
    return native_parent_;
  }

 private:
  base::scoped_nsobject<NSWindow> native_parent_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacTest);
};

class WidgetChangeObserver : public TestWidgetObserver {
 public:
  WidgetChangeObserver(Widget* widget)
      : TestWidgetObserver(widget),
        gained_visible_count_(0),
        lost_visible_count_(0) {}

  int gained_visible_count() const { return gained_visible_count_; }
  int lost_visible_count() const { return lost_visible_count_; }

 private:
  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget,
                                 bool visible) override {
    ++(visible ? gained_visible_count_ : lost_visible_count_);
  }

  int gained_visible_count_;
  int lost_visible_count_;

  DISALLOW_COPY_AND_ASSIGN(WidgetChangeObserver);
};

// Test visibility states triggered externally.
TEST_F(NativeWidgetMacTest, HideAndShowExternally) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow();
  WidgetChangeObserver observer(widget);

  // Should initially be hidden.
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(0, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Hide();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  // Test when hiding individual windows.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  [ns_window orderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  // Test when hiding the entire application. This doesn't send an orderOut:
  // to the NSWindow.
  [NSApp hide:nil];
  // When the activation policy is NSApplicationActivationPolicyRegular, the
  // calls via NSApp are asynchronous, and the run loop needs to be flushed.
  // With NSApplicationActivationPolicyProhibited, the following RunUntilIdle
  // calls are superfluous, but don't hurt.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  [NSApp unhideWithoutActivation];
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(4, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  // Hide again to test unhiding with an activation.
  [NSApp hide:nil];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, observer.lost_visible_count());
  [NSApp unhide:nil];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, observer.gained_visible_count());

  // Hide again to test makeKeyAndOrderFront:.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(5, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  [ns_window makeKeyAndOrderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(6, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  // No change when closing.
  widget->CloseNow();
  EXPECT_EQ(5, observer.lost_visible_count());
  EXPECT_EQ(6, observer.gained_visible_count());
}

// A view that counts calls to OnPaint().
class PaintCountView : public View {
 public:
  PaintCountView() : paint_count_(0) {
    SetBounds(0, 0, 100, 100);
  }

  // View:
  void OnPaint(gfx::Canvas* canvas) override {
    EXPECT_TRUE(GetWidget()->IsVisible());
    ++paint_count_;
  }

  int paint_count() { return paint_count_; }

 private:
  int paint_count_;

  DISALLOW_COPY_AND_ASSIGN(PaintCountView);
};

// Test minimized states triggered externally, implied visibility and restored
// bounds whilst minimized.
TEST_F(NativeWidgetMacTest, MiniaturizeExternally) {
  Widget* widget = new Widget;
  Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
  // Make the layer not drawn, so that calls to paint can be observed
  // synchronously.
  init_params.layer_type = ui::LAYER_NOT_DRAWN;
  widget->Init(init_params);

  PaintCountView* view = new PaintCountView();
  widget->GetContentsView()->AddChildView(view);
  NSWindow* ns_window = widget->GetNativeWindow();
  WidgetChangeObserver observer(widget);

  widget->SetBounds(gfx::Rect(100, 100, 300, 300));

  EXPECT_TRUE(view->IsDrawn());
  EXPECT_EQ(0, view->paint_count());
  widget->Show();

  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());
  const gfx::Rect restored_bounds = widget->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());

  // Showing should paint.
  EXPECT_EQ(1, view->paint_count());

  // First try performMiniaturize:, which requires a minimize button. Note that
  // Cocoa just blocks the UI thread during the animation, so no need to do
  // anything fancy to wait for it finish.
  [ns_window performMiniaturize:nil];

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsVisible());  // Minimizing also makes things invisible.
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  // No repaint when minimizing. But note that this is partly due to not calling
  // [NSView setNeedsDisplay:YES] on the content view. The superview, which is
  // an NSThemeFrame, would repaint |view| if we had, because the miniaturize
  // button is highlighted for performMiniaturize.
  EXPECT_EQ(1, view->paint_count());

  [ns_window deminiaturize:nil];

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  EXPECT_EQ(2, view->paint_count());  // A single paint when deminiaturizing.
  EXPECT_FALSE([ns_window isMiniaturized]);

  widget->Minimize();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(2, view->paint_count());  // No paint when miniaturizing.

  widget->Restore();  // If miniaturized, should deminiaturize.

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(3, view->paint_count());

  widget->Restore();  // If not miniaturized, does nothing.

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(3, view->paint_count());

  widget->CloseNow();

  // Create a widget without a minimize button.
  widget = CreateTopLevelFramelessPlatformWidget();
  ns_window = widget->GetNativeWindow();
  widget->SetBounds(gfx::Rect(100, 100, 300, 300));
  widget->Show();
  EXPECT_FALSE(widget->IsMinimized());

  // This should fail, since performMiniaturize: requires a minimize button.
  [ns_window performMiniaturize:nil];
  EXPECT_FALSE(widget->IsMinimized());

  // But this should work.
  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());

  // Test closing while minimized.
  widget->CloseNow();
}

// Simple view for the SetCursor test that overrides View::GetCursor().
class CursorView : public View {
 public:
  CursorView(int x, NSCursor* cursor) : cursor_(cursor) {
    SetBounds(x, 0, 100, 300);
  }

  // View:
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override {
    return cursor_;
  }

 private:
  NSCursor* cursor_;

  DISALLOW_COPY_AND_ASSIGN(CursorView);
};

// Test for Widget::SetCursor(). There is no Widget::GetCursor(), so this uses
// -[NSCursor currentCursor] to validate expectations. Note that currentCursor
// is just "the top cursor on the application's cursor stack.", which is why it
// is safe to use this in a non-interactive UI test with the EventGenerator.
TEST_F(NativeWidgetMacTest, SetCursor) {
  NSCursor* arrow = [NSCursor arrowCursor];
  NSCursor* hand = GetNativeHandCursor();
  NSCursor* ibeam = GetNativeIBeamCursor();

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  widget->GetContentsView()->AddChildView(new CursorView(0, hand));
  widget->GetContentsView()->AddChildView(new CursorView(100, ibeam));
  widget->Show();

  // Events used to simulate tracking rectangle updates. These are not passed to
  // toolkit-views, so it only matters whether they are inside or outside the
  // content area.
  NSEvent* event_in_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(100, 100), NSMouseMoved, 0);
  NSEvent* event_out_of_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(-50, -50), NSMouseMoved, 0);

  EXPECT_NE(arrow, hand);
  EXPECT_NE(arrow, ibeam);

  // At the start of the test, the cursor stack should be empty.
  EXPECT_FALSE([NSCursor currentCursor]);

  // Use an event generator to ask views code to set the cursor. However, note
  // that this does not cause Cocoa to generate tracking rectangle updates.
  ui::test::EventGenerator event_generator(GetContext(),
                                           widget->GetNativeWindow());

  // Move the mouse over the first view, then simulate a tracking rectangle
  // update.
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(hand, [NSCursor currentCursor]);

  // A tracking rectangle update not in the content area should forward to
  // the native NSWindow implementation, which sets the arrow cursor.
  [widget->GetNativeWindow() cursorUpdate:event_out_of_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  // Now move to the second view.
  event_generator.MoveMouseTo(gfx::Point(150, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(ibeam, [NSCursor currentCursor]);

  // Moving to the third view (but remaining in the content area) should also
  // forward to the native NSWindow implementation.
  event_generator.MoveMouseTo(gfx::Point(250, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  widget->CloseNow();
}

// Tests that an accessibility request from the system makes its way through to
// a views::Label filling the window.
TEST_F(NativeWidgetMacTest, AccessibilityIntegration) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const base::string16 test_string = base::ASCIIToUTF16("Green");
  views::Label* label = new views::Label(test_string);
  label->SetBounds(0, 0, 100, 100);
  widget->GetContentsView()->AddChildView(label);
  widget->Show();

  // Accessibility hit tests come in Cocoa screen coordinates.
  NSRect nsrect = gfx::ScreenRectToNSRect(screen_rect);
  NSPoint midpoint = NSMakePoint(NSMidX(nsrect), NSMidY(nsrect));

  id hit = [widget->GetNativeWindow() accessibilityHitTest:midpoint];
  id title = [hit accessibilityAttributeValue:NSAccessibilityTitleAttribute];
  EXPECT_NSEQ(title, @"Green");

  widget->CloseNow();
}

// Tests creating a views::Widget parented off a native NSWindow.
TEST_F(NativeWidgetMacTest, NonWidgetParent) {
  NSWindow* native_parent = MakeNativeParent();

  base::scoped_nsobject<NSView> anchor_view(
      [[NSView alloc] initWithFrame:[[native_parent contentView] bounds]]);
  [[native_parent contentView] addSubview:anchor_view];

  // Note: Don't use WidgetTest::CreateChildPlatformWidget because that makes
  // windows of TYPE_CONTROL which are automatically made visible. But still
  // mark it as a child to test window positioning.
  Widget* child = new Widget;
  Widget::InitParams init_params;
  init_params.parent = anchor_view;
  init_params.child = true;
  child->Init(init_params);

  TestWidgetObserver child_observer(child);

  // GetTopLevelNativeWidget() only goes as far as there exists a Widget (i.e.
  // must stop at |child|.
  internal::NativeWidgetPrivate* top_level_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(
          child->GetNativeView());
  EXPECT_EQ(child, top_level_widget->GetWidget());

  // To verify the parent, we need to use NativeWidgetMac APIs.
  BridgedNativeWidget* bridged_native_widget =
      NativeWidgetMac::GetBridgeForNativeWindow(child->GetNativeWindow());
  EXPECT_EQ(native_parent, bridged_native_widget->parent()->GetNSWindow());

  child->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_FALSE(child->IsVisible());
  EXPECT_EQ(0u, [[native_parent childWindows] count]);

  child->Show();
  EXPECT_TRUE(child->IsVisible());
  EXPECT_EQ(1u, [[native_parent childWindows] count]);
  EXPECT_EQ(child->GetNativeWindow(),
            [[native_parent childWindows] objectAtIndex:0]);
  EXPECT_EQ(native_parent, [child->GetNativeWindow() parentWindow]);

  // Child should be positioned on screen relative to the parent, but note we
  // positioned the parent in Cocoa coordinates, so we need to convert.
  gfx::Point parent_origin = gfx::ScreenRectFromNSRect(ParentRect()).origin();
  EXPECT_EQ(gfx::Rect(150, parent_origin.y() + 50, 200, 100),
            child->GetWindowBoundsInScreen());

  // Removing the anchor_view from its view hierarchy is permitted. This should
  // not break the relationship between the two windows.
  [anchor_view removeFromSuperview];
  anchor_view.reset();
  EXPECT_EQ(native_parent, bridged_native_widget->parent()->GetNSWindow());

  // Closing the parent should close and destroy the child.
  EXPECT_FALSE(child_observer.widget_closed());
  [native_parent close];
  EXPECT_TRUE(child_observer.widget_closed());

  EXPECT_EQ(0u, [[native_parent childWindows] count]);
}

// Use Native APIs to query the tooltip text that would be shown once the
// tooltip delay had elapsed.
base::string16 TooltipTextForWidget(Widget* widget) {
  // For Mac, the actual location doesn't matter, since there is only one native
  // view and it fills the window. This just assumes the window is at least big
  // big enough for a constant coordinate to be within it.
  NSPoint point = NSMakePoint(30, 30);
  NSView* view = [widget->GetNativeView() hitTest:point];
  NSString* text =
      [view view:view stringForToolTip:0 point:point userData:nullptr];
  return base::SysNSStringToUTF16(text);
}

// Tests tooltips. The test doesn't wait for tooltips to appear. That is, the
// test assumes Cocoa calls stringForToolTip: at appropriate times and that,
// when a tooltip is already visible, changing it causes an update. These were
// tested manually by inserting a base::RunLoop.Run().
TEST_F(NativeWidgetMacTest, Tooltips) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const base::string16 tooltip_back = base::ASCIIToUTF16("Back");
  const base::string16 tooltip_front = base::ASCIIToUTF16("Front");
  const base::string16 long_tooltip(2000, 'W');

  // Create a nested layout to test corner cases.
  LabelButton* back = new LabelButton(nullptr, base::string16());
  back->SetBounds(10, 10, 80, 80);
  widget->GetContentsView()->AddChildView(back);
  widget->Show();

  ui::test::EventGenerator event_generator(GetContext(),
                                           widget->GetNativeWindow());

  // Initially, there should be no tooltip.
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());

  // Create a new button for the "front", and set the tooltip, but don't add it
  // to the view hierarchy yet.
  LabelButton* front = new LabelButton(nullptr, base::string16());
  front->SetBounds(20, 20, 40, 40);
  front->SetTooltipText(tooltip_front);

  // Changing the tooltip text shouldn't require an additional mousemove to take
  // effect.
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());
  back->SetTooltipText(tooltip_back);
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Adding a new view under the mouse should also take immediate effect.
  back->AddChildView(front);
  EXPECT_EQ(tooltip_front, TooltipTextForWidget(widget));

  // A long tooltip will be wrapped by Cocoa, but the full string should appear.
  // Note that render widget hosts clip at 1024 to prevent DOS, but in toolkit-
  // views the UI is more trusted.
  front->SetTooltipText(long_tooltip);
  EXPECT_EQ(long_tooltip, TooltipTextForWidget(widget));

  // Move the mouse to a different view - tooltip should change.
  event_generator.MoveMouseTo(gfx::Point(15, 15));
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Move the mouse off of any view, tooltip should clear.
  event_generator.MoveMouseTo(gfx::Point(5, 5));
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());

  widget->CloseNow();
}

namespace {

// Delegate to make Widgets of MODAL_TYPE_CHILD.
class ChildModalDialogDelegate : public DialogDelegateView {
 public:
  ChildModalDialogDelegate() {}

  // WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildModalDialogDelegate);
};

// While in scope, waits for a call to a swizzled objective C method, then quits
// a nested run loop.
class ScopedSwizzleWaiter {
 public:
  explicit ScopedSwizzleWaiter(Class target)
      : swizzler_(target,
                  [TestStopAnimationWaiter class],
                  @selector(setWindowStateForEnd)) {
    DCHECK(!instance_);
    instance_ = this;
  }

  ~ScopedSwizzleWaiter() { instance_ = nullptr; }

  static IMP GetMethodAndMarkCalled() {
    return instance_->GetMethodInternal();
  }

  void WaitForMethod() {
    if (method_called_)
      return;

    base::RunLoop run_loop;
    base::MessageLoop::current()->task_runner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  bool method_called() const { return method_called_; }

 private:
  IMP GetMethodInternal() {
    DCHECK(!method_called_);
    method_called_ = true;
    if (run_loop_)
      run_loop_->Quit();
    return swizzler_.GetOriginalImplementation();
  }

  static ScopedSwizzleWaiter* instance_;

  base::mac::ScopedObjCClassSwizzler swizzler_;
  base::RunLoop* run_loop_ = nullptr;
  bool method_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedSwizzleWaiter);
};

ScopedSwizzleWaiter* ScopedSwizzleWaiter::instance_ = nullptr;

// Shows a modal widget and waits for the show animation to complete. Waiting is
// not compulsory (calling Close() while animating the show will cancel the show
// animation). However, testing with overlapping swizzlers is tricky.
Widget* ShowChildModalWidgetAndWait(NSWindow* native_parent) {
  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new ChildModalDialogDelegate, nullptr, [native_parent contentView]);

  modal_dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(modal_dialog_widget->IsVisible());
  ScopedSwizzleWaiter show_waiter([ConstrainedWindowAnimationShow class]);

  modal_dialog_widget->Show();
  // Visible immediately (although it animates from transparent).
  EXPECT_TRUE(modal_dialog_widget->IsVisible());

  // Run the animation.
  show_waiter.WaitForMethod();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  EXPECT_TRUE(show_waiter.method_called());
  return modal_dialog_widget;
}

}  // namespace

// Tests object lifetime for the show/hide animations used for child-modal
// windows. Parents the dialog off a native parent window (not a views::Widget).
TEST_F(NativeWidgetMacTest, NativeWindowChildModalShowHide) {
  NSWindow* native_parent = MakeNativeParent();
  {
    Widget* modal_dialog_widget = ShowChildModalWidgetAndWait(native_parent);
    TestWidgetObserver widget_observer(modal_dialog_widget);

    ScopedSwizzleWaiter hide_waiter([ConstrainedWindowAnimationHide class]);
    EXPECT_TRUE(modal_dialog_widget->IsVisible());
    EXPECT_FALSE(widget_observer.widget_closed());

    // Widget::Close() is always asynchronous, so we can check that the widget
    // is initially visible, but then it's destroyed.
    modal_dialog_widget->Close();
    EXPECT_TRUE(modal_dialog_widget->IsVisible());
    EXPECT_FALSE(hide_waiter.method_called());
    EXPECT_FALSE(widget_observer.widget_closed());

    // Wait for a hide to finish.
    hide_waiter.WaitForMethod();
    EXPECT_TRUE(hide_waiter.method_called());

    // The animation finishing should also mean it has closed the window.
    EXPECT_TRUE(widget_observer.widget_closed());
  }

  {
    // Make a new dialog to test another lifetime flow.
    Widget* modal_dialog_widget = ShowChildModalWidgetAndWait(native_parent);
    TestWidgetObserver widget_observer(modal_dialog_widget);

    // Start an asynchronous close as above.
    ScopedSwizzleWaiter hide_waiter([ConstrainedWindowAnimationHide class]);
    modal_dialog_widget->Close();
    EXPECT_FALSE(widget_observer.widget_closed());
    EXPECT_FALSE(hide_waiter.method_called());

    // Now close the _parent_ window to force a synchronous close of the child.
    [native_parent close];

    // Widget is destroyed immediately. No longer paints, but the animation is
    // still running.
    EXPECT_TRUE(widget_observer.widget_closed());
    EXPECT_FALSE(hide_waiter.method_called());

    // Wait for the hide again. It will call close on its retained copy of the
    // child NSWindow, but that's fine since all the C++ objects are detached.
    hide_waiter.WaitForMethod();
    EXPECT_TRUE(hide_waiter.method_called());
  }
}

// Tests Cocoa properties that should be given to particular widget types.
TEST_F(NativeWidgetMacTest, NativeProperties) {
  // Create a regular widget (TYPE_WINDOW).
  Widget* regular_widget = CreateNativeDesktopWidget();
  EXPECT_TRUE([regular_widget->GetNativeWindow() canBecomeKeyWindow]);
  EXPECT_TRUE([regular_widget->GetNativeWindow() canBecomeMainWindow]);

  // Disabling activation should prevent key and main status.
  regular_widget->widget_delegate()->set_can_activate(false);
  EXPECT_FALSE([regular_widget->GetNativeWindow() canBecomeKeyWindow]);
  EXPECT_FALSE([regular_widget->GetNativeWindow() canBecomeMainWindow]);

  // Create a dialog widget (also TYPE_WINDOW), but with a DialogDelegate.
  Widget* dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new ChildModalDialogDelegate, nullptr, regular_widget->GetNativeView());
  EXPECT_TRUE([dialog_widget->GetNativeWindow() canBecomeKeyWindow]);
  // Dialogs shouldn't take main status away from their parent.
  EXPECT_FALSE([dialog_widget->GetNativeWindow() canBecomeMainWindow]);

  regular_widget->CloseNow();
}

NSData* WindowContentsAsTIFF(NSWindow* window) {
  NSView* frame_view = [[window contentView] superview];
  EXPECT_TRUE(frame_view);

  // Inset to mask off left and right edges which vary in HighDPI.
  NSRect bounds = NSInsetRect([frame_view bounds], 4, 0);

  // On 10.6, the grippy changes appearance slightly when painted the second
  // time in a textured window. Since this test cares about the window title,
  // cut off the bottom of the window.
  bounds.size.height -= 40;
  bounds.origin.y += 40;

  NSBitmapImageRep* bitmap =
      [frame_view bitmapImageRepForCachingDisplayInRect:bounds];
  EXPECT_TRUE(bitmap);

  [frame_view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
  NSData* tiff = [bitmap TIFFRepresentation];
  EXPECT_TRUE(tiff);
  return tiff;
}

class CustomTitleWidgetDelegate : public WidgetDelegate {
 public:
  CustomTitleWidgetDelegate(Widget* widget)
      : widget_(widget), should_show_title_(true) {}

  void set_title(const base::string16& title) { title_ = title; }
  void set_should_show_title(bool show) { should_show_title_ = show; }

  // WidgetDelegate:
  base::string16 GetWindowTitle() const override { return title_; }
  bool ShouldShowWindowTitle() const override { return should_show_title_; }
  Widget* GetWidget() override { return widget_; };
  const Widget* GetWidget() const override { return widget_; };

 private:
  Widget* widget_;
  base::string16 title_;
  bool should_show_title_;

  DISALLOW_COPY_AND_ASSIGN(CustomTitleWidgetDelegate);
};

// Test that undocumented title-hiding API we're using does the job.
TEST_F(NativeWidgetMacTest, DoesHideTitle) {
  // Same as CreateTopLevelPlatformWidget but with a custom delegate.
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  Widget* widget = new Widget;
  params.native_widget = new NativeWidgetCapture(widget);
  CustomTitleWidgetDelegate delegate(widget);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 800, 600);
  widget->Init(params);
  widget->Show();

  NSWindow* ns_window = widget->GetNativeWindow();
  // Disable color correction so we can read unmodified values from the bitmap.
  [ns_window setColorSpace:[NSColorSpace sRGBColorSpace]];

  EXPECT_EQ(base::string16(), delegate.GetWindowTitle());
  EXPECT_NSEQ(@"", [ns_window title]);
  NSData* empty_title_data = WindowContentsAsTIFF(ns_window);

  delegate.set_title(base::ASCIIToUTF16("This is a title"));
  widget->UpdateWindowTitle();
  NSData* this_title_data = WindowContentsAsTIFF(ns_window);

  // The default window with a title should look different from the
  // window with an empty title.
  EXPECT_FALSE([empty_title_data isEqualToData:this_title_data]);

  delegate.set_should_show_title(false);
  delegate.set_title(base::ASCIIToUTF16("This is another title"));
  widget->UpdateWindowTitle();
  NSData* hidden_title_data = WindowContentsAsTIFF(ns_window);

  // With our magic setting, the window with a title should look the
  // same as the window with an empty title.
  EXPECT_TRUE([ns_window _isTitleHidden]);
  EXPECT_TRUE([empty_title_data isEqualToData:hidden_title_data]);

  widget->CloseNow();
}

}  // namespace test
}  // namespace views

@implementation TestStopAnimationWaiter
- (void)setWindowStateForEnd {
  views::test::ScopedSwizzleWaiter::GetMethodAndMarkCalled()(self, _cmd);
}
@end
