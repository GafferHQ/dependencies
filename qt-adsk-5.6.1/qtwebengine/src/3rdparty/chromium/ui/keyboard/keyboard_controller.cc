// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_layout_manager.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/masked_window_targeter.h"

#if defined(OS_CHROMEOS)
#include "base/process/launch.h"
#include "base/sys_info.h"
#if defined(USE_OZONE)
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#endif
#endif  // if defined(OS_CHROMEOS)

namespace {

const int kHideKeyboardDelayMs = 100;

// The virtual keyboard show/hide animation duration.
const int kShowAnimationDurationMs = 350;
const int kHideAnimationDurationMs = 100;

// The opacity of virtual keyboard container when show animation starts or
// hide animation finishes. This cannot be zero because we call Show() on the
// keyboard window before setting the opacity back to 1.0. Since windows are not
// allowed to be shown with zero opacity, we always animate to 0.01 instead.
const float kAnimationStartOrAfterHideOpacity = 0.01f;

// The KeyboardWindowDelegate makes sure the keyboard-window does not get focus.
// This is necessary to make sure that the synthetic key-events reach the target
// window.
// The delegate deletes itself when the window is destroyed.
class KeyboardWindowDelegate : public aura::WindowDelegate {
 public:
  KeyboardWindowDelegate() {}
  ~KeyboardWindowDelegate() override {}

 private:
  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return false; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(gfx::Path* mask) const override {}

  DISALLOW_COPY_AND_ASSIGN(KeyboardWindowDelegate);
};

void ToggleTouchEventLogging(bool enable) {
#if defined(OS_CHROMEOS)
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetInputController()
      ->SetTouchEventLoggingEnabled(enable);
#elif defined(USE_X11)
  if (!base::SysInfo::IsRunningOnChromeOS())
    return;
  base::CommandLine command(
      base::FilePath("/opt/google/touchscreen/toggle_touch_event_logging"));
  if (enable)
    command.AppendArg("1");
  else
    command.AppendArg("0");
  VLOG(1) << "Running " << command.GetCommandLineString();
  base::LaunchOptions options;
  options.wait = true;
  base::LaunchProcess(command, options);
#endif
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

namespace keyboard {

// Observer for both keyboard show and hide animations. It should be owned by
// KeyboardController.
class CallbackAnimationObserver : public ui::LayerAnimationObserver {
 public:
  CallbackAnimationObserver(const scoped_refptr<ui::LayerAnimator>& animator,
                            base::Callback<void(void)> callback);
  ~CallbackAnimationObserver() override;

 private:
  // Overridden from ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* seq) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* seq) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* seq) override {}

  scoped_refptr<ui::LayerAnimator> animator_;
  base::Callback<void(void)> callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackAnimationObserver);
};

CallbackAnimationObserver::CallbackAnimationObserver(
    const scoped_refptr<ui::LayerAnimator>& animator,
    base::Callback<void(void)> callback)
    : animator_(animator), callback_(callback) {
}

CallbackAnimationObserver::~CallbackAnimationObserver() {
  animator_->RemoveObserver(this);
}

void CallbackAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* seq) {
  if (animator_->is_animating())
    return;
  animator_->RemoveObserver(this);
  callback_.Run();
}

void CallbackAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* seq) {
  animator_->RemoveObserver(this);
}

class WindowBoundsChangeObserver : public aura::WindowObserver {
 public:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroyed(aura::Window* window) override;

  void AddObservedWindow(aura::Window* window);
  void RemoveAllObservedWindows();

 private:
  std::set<aura::Window*> observed_windows_;
};

void WindowBoundsChangeObserver::OnWindowBoundsChanged(aura::Window* window,
    const gfx::Rect& old_bounds, const gfx::Rect& new_bounds) {
  KeyboardController* controller =  KeyboardController::GetInstance();
  if (controller)
    controller->UpdateWindowInsets(window);
}

void WindowBoundsChangeObserver::OnWindowDestroyed(aura::Window* window) {
  if (window->HasObserver(this))
    window->RemoveObserver(this);
  observed_windows_.erase(window);
}

void WindowBoundsChangeObserver::AddObservedWindow(aura::Window* window) {
  if (!window->HasObserver(this)) {
    window->AddObserver(this);
    observed_windows_.insert(window);
  }
}

void WindowBoundsChangeObserver::RemoveAllObservedWindows() {
  for (std::set<aura::Window*>::iterator it = observed_windows_.begin();
       it != observed_windows_.end(); ++it)
    (*it)->RemoveObserver(this);
  observed_windows_.clear();
}

// static
KeyboardController* KeyboardController::instance_ = NULL;

KeyboardController::KeyboardController(KeyboardControllerProxy* proxy)
    : proxy_(proxy),
      input_method_(NULL),
      keyboard_visible_(false),
      show_on_resize_(false),
      lock_keyboard_(false),
      keyboard_mode_(FULL_WIDTH),
      type_(ui::TEXT_INPUT_TYPE_NONE),
      weak_factory_(this) {
  CHECK(proxy);
  input_method_ = proxy_->GetInputMethod();
  input_method_->AddObserver(this);
  window_bounds_observer_.reset(new WindowBoundsChangeObserver());
  proxy_->SetController(this);
}

KeyboardController::~KeyboardController() {
  if (container_) {
    if (container_->GetRootWindow())
      container_->GetRootWindow()->RemoveObserver(this);
    container_->RemoveObserver(this);
  }
  if (input_method_)
    input_method_->RemoveObserver(this);
  ResetWindowInsets();
  proxy_->SetController(nullptr);
}

// static
void KeyboardController::ResetInstance(KeyboardController* controller) {
  if (instance_ && instance_ != controller)
    delete instance_;
  instance_ = controller;
}

// static
KeyboardController* KeyboardController::GetInstance() {
  return instance_;
}

aura::Window* KeyboardController::GetContainerWindow() {
  if (!container_.get()) {
    container_.reset(new aura::Window(new KeyboardWindowDelegate()));
    container_->SetName("KeyboardContainer");
    container_->set_owned_by_parent(false);
    container_->Init(ui::LAYER_NOT_DRAWN);
    container_->AddObserver(this);
    container_->SetLayoutManager(new KeyboardLayoutManager(this));
  }
  return container_.get();
}

void KeyboardController::NotifyKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  current_keyboard_bounds_ = new_bounds;
  if (proxy_->HasKeyboardWindow() && proxy_->GetKeyboardWindow()->IsVisible()) {
    FOR_EACH_OBSERVER(KeyboardControllerObserver,
                      observer_list_,
                      OnKeyboardBoundsChanging(new_bounds));
    if (keyboard::IsKeyboardOverscrollEnabled()) {
      // Adjust the height of the viewport for visible windows on the primary
      // display.
      // TODO(kevers): Add EnvObserver to properly initialize insets if a
      // window is created while the keyboard is visible.
      scoped_ptr<content::RenderWidgetHostIterator> widgets(
          content::RenderWidgetHost::GetRenderWidgetHosts());
      aura::Window* keyboard_window = proxy_->GetKeyboardWindow();
      aura::Window* root_window = keyboard_window->GetRootWindow();
      while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
        content::RenderWidgetHostView* view = widget->GetView();
        // Can be NULL, e.g. if the RenderWidget is being destroyed or
        // the render process crashed.
        if (view) {
          aura::Window* window = view->GetNativeView();
          // If virtual keyboard failed to load, a widget that displays error
          // message will be created and adds as a child of the virtual keyboard
          // window. We want to avoid add BoundsChangedObserver to that window.
          if (!keyboard_window->Contains(window) &&
              window->GetRootWindow() == root_window) {
            gfx::Rect window_bounds = window->GetBoundsInScreen();
            gfx::Rect intersect = gfx::IntersectRects(window_bounds,
                                                      new_bounds);
            int overlap = intersect.height();
            if (overlap > 0 && overlap < window_bounds.height())
              view->SetInsets(gfx::Insets(0, 0, overlap, 0));
            else
              view->SetInsets(gfx::Insets());
            AddBoundsChangedObserver(window);
          }
        }
      }
    } else {
      ResetWindowInsets();
    }
  } else {
    current_keyboard_bounds_ = gfx::Rect();
  }
}

void KeyboardController::HideKeyboard(HideReason reason) {
  keyboard_visible_ = false;
  ToggleTouchEventLogging(true);

  keyboard::LogKeyboardControlEvent(
      reason == HIDE_REASON_AUTOMATIC ?
          keyboard::KEYBOARD_CONTROL_HIDE_AUTO :
          keyboard::KEYBOARD_CONTROL_HIDE_USER);

  NotifyKeyboardBoundsChanging(gfx::Rect());

  set_lock_keyboard(false);

  ui::LayerAnimator* container_animator = container_->layer()->GetAnimator();
  animation_observer_.reset(new CallbackAnimationObserver(
      container_animator,
      base::Bind(&KeyboardController::HideAnimationFinished,
                 base::Unretained(this))));
  container_animator->AddObserver(animation_observer_.get());

  ui::ScopedLayerAnimationSettings settings(container_animator);
  settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kHideAnimationDurationMs));
  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container_->SetTransform(transform);
  container_->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

void KeyboardController::AddObserver(KeyboardControllerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void KeyboardController::RemoveObserver(KeyboardControllerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void KeyboardController::SetKeyboardMode(KeyboardMode mode) {
  if (keyboard_mode_ == mode)
    return;

  keyboard_mode_ = mode;
  // When keyboard is floating, no overscroll or resize is necessary. Sets
  // keyboard bounds to zero so overscroll or resize is disabled.
  if (keyboard_mode_ == FLOATING) {
    NotifyKeyboardBoundsChanging(gfx::Rect());
  } else if (keyboard_mode_ == FULL_WIDTH) {
    // TODO(bshe): revisit this logic after we decide to support resize virtual
    // keyboard.
    int keyboard_height = GetContainerWindow()->bounds().height();
    const gfx::Rect& root_bounds = container_->GetRootWindow()->bounds();
    gfx::Rect new_bounds = root_bounds;
    new_bounds.set_y(root_bounds.height() - keyboard_height);
    new_bounds.set_height(keyboard_height);
    GetContainerWindow()->SetBounds(new_bounds);
    // No animation added, so call ShowAnimationFinished immediately.
    ShowAnimationFinished();
  }
}

void KeyboardController::ShowKeyboard(bool lock) {
  set_lock_keyboard(lock);
  ShowKeyboardInternal();
}

void KeyboardController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.new_parent && params.target == container_.get())
    OnTextInputStateChanged(proxy_->GetInputMethod()->GetTextInputClient());
}

void KeyboardController::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!window->GetRootWindow()->HasObserver(this))
    window->GetRootWindow()->AddObserver(this);
}

void KeyboardController::OnWindowRemovingFromRootWindow(aura::Window* window,
    aura::Window* new_root) {
  if (window->GetRootWindow()->HasObserver(this))
    window->GetRootWindow()->RemoveObserver(this);
}

void KeyboardController::OnWindowBoundsChanged(aura::Window* window,
                                               const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  if (!window->IsRootWindow())
    return;
  // Keep the same height when window resize. It gets called when screen
  // rotate.
  if (!keyboard_container_initialized() || !proxy_->HasKeyboardWindow())
    return;

  int container_height = container_->bounds().height();
  if (keyboard_mode_ == FULL_WIDTH) {
    container_->SetBounds(gfx::Rect(new_bounds.x(),
                                    new_bounds.bottom() - container_height,
                                    new_bounds.width(),
                                    container_height));
  } else if (keyboard_mode_ == FLOATING) {
    // When screen rotate, horizontally center floating virtual keyboard
    // window and vertically align it to the bottom.
    int container_width = container_->bounds().width();
    container_->SetBounds(gfx::Rect(
        new_bounds.x() + (new_bounds.width() - container_width) / 2,
        new_bounds.bottom() - container_height,
        container_width,
        container_height));
  }
}

void KeyboardController::Reload() {
  if (proxy_->HasKeyboardWindow()) {
    // A reload should never try to show virtual keyboard. If keyboard is not
    // visible before reload, it should keep invisible after reload.
    show_on_resize_ = false;
    proxy_->ReloadKeyboardIfNeeded();
  }
}

void KeyboardController::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  if (!container_.get())
    return;

  type_ = client ? client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;

  if (type_ == ui::TEXT_INPUT_TYPE_NONE && !lock_keyboard_) {
    if (keyboard_visible_) {
      // Set the visibility state here so that any queries for visibility
      // before the timer fires returns the correct future value.
      keyboard_visible_ = false;
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&KeyboardController::HideKeyboard,
                     weak_factory_.GetWeakPtr(), HIDE_REASON_AUTOMATIC),
          base::TimeDelta::FromMilliseconds(kHideKeyboardDelayMs));
    }
  } else {
    // Abort a pending keyboard hide.
    if (WillHideKeyboard()) {
      weak_factory_.InvalidateWeakPtrs();
      keyboard_visible_ = true;
    }
    proxy_->SetUpdateInputType(type_);
    // Do not explicitly show the Virtual keyboard unless it is in the process
    // of hiding. Instead, the virtual keyboard is shown in response to a user
    // gesture (mouse or touch) that is received while an element has input
    // focus. Showing the keyboard requires an explicit call to
    // OnShowImeIfNeeded.
  }
}

void KeyboardController::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  DCHECK_EQ(input_method_, input_method);
  input_method_ = NULL;
}

void KeyboardController::OnShowImeIfNeeded() {
  ShowKeyboardInternal();
}

bool KeyboardController::ShouldEnableInsets(aura::Window* window) {
  aura::Window* keyboard_window = proxy_->GetKeyboardWindow();
  return (keyboard_window->GetRootWindow() == window->GetRootWindow() &&
          keyboard::IsKeyboardOverscrollEnabled() &&
          keyboard_window->IsVisible() && keyboard_visible_);
}

void KeyboardController::UpdateWindowInsets(aura::Window* window) {
  aura::Window* keyboard_window = proxy_->GetKeyboardWindow();
  if (window == keyboard_window)
    return;

  scoped_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    if (view && window->Contains(view->GetNativeView())) {
      gfx::Rect window_bounds = view->GetNativeView()->GetBoundsInScreen();
      gfx::Rect intersect =
          gfx::IntersectRects(window_bounds, keyboard_window->bounds());
      int overlap = ShouldEnableInsets(window) ? intersect.height() : 0;
      if (overlap > 0 && overlap < window_bounds.height())
        view->SetInsets(gfx::Insets(0, 0, overlap, 0));
      else
        view->SetInsets(gfx::Insets());
      return;
    }
  }
}

void KeyboardController::ShowKeyboardInternal() {
  if (!container_.get())
    return;

  if (container_->children().empty()) {
    keyboard::MarkKeyboardLoadStarted();
    aura::Window* keyboard = proxy_->GetKeyboardWindow();
    keyboard->Show();
    container_->AddChild(keyboard);
    keyboard->set_owned_by_parent(false);
  }

  proxy_->ReloadKeyboardIfNeeded();

  if (keyboard_visible_) {
    return;
  } else if (proxy_->GetKeyboardWindow()->bounds().height() == 0) {
    show_on_resize_ = true;
    return;
  }

  keyboard_visible_ = true;

  // If the controller is in the process of hiding the keyboard, do not log
  // the stat here since the keyboard will not actually be shown.
  if (!WillHideKeyboard())
    keyboard::LogKeyboardControlEvent(keyboard::KEYBOARD_CONTROL_SHOW);

  weak_factory_.InvalidateWeakPtrs();

  // If |container_| has hide animation, its visibility is set to false when
  // hide animation finished. So even if the container is visible at this
  // point, it may in the process of hiding. We still need to show keyboard
  // container in this case.
  if (container_->IsVisible() &&
      !container_->layer()->GetAnimator()->is_animating())
    return;

  ToggleTouchEventLogging(false);
  ui::LayerAnimator* container_animator = container_->layer()->GetAnimator();

  // If the container is not animating, makes sure the position and opacity
  // are at begin states for animation.
  if (!container_animator->is_animating()) {
    gfx::Transform transform;
    transform.Translate(0, kAnimationDistance);
    container_->SetTransform(transform);
    container_->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
  }

  container_animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  if (keyboard_mode_ == FLOATING) {
    animation_observer_.reset();
  } else {
    animation_observer_.reset(new CallbackAnimationObserver(
        container_animator,
        base::Bind(&KeyboardController::ShowAnimationFinished,
                   base::Unretained(this))));
    container_animator->AddObserver(animation_observer_.get());
  }

  proxy_->ShowKeyboardContainer(container_.get());

  {
    // Scope the following animation settings as we don't want to animate
    // visibility change that triggered by a call to the base class function
    // ShowKeyboardContainer with these settings. The container should become
    // visible immediately.
    ui::ScopedLayerAnimationSettings settings(container_animator);
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kShowAnimationDurationMs));
    container_->SetTransform(gfx::Transform());
    container_->layer()->SetOpacity(1.0);
  }
}

void KeyboardController::ResetWindowInsets() {
  const gfx::Insets insets;
  scoped_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    if (view)
      view->SetInsets(insets);
  }
  window_bounds_observer_->RemoveAllObservedWindows();
}

bool KeyboardController::WillHideKeyboard() const {
  return weak_factory_.HasWeakPtrs();
}

void KeyboardController::ShowAnimationFinished() {
  // Notify observers after animation finished to prevent reveal desktop
  // background during animation.
  NotifyKeyboardBoundsChanging(container_->bounds());
  proxy_->EnsureCaretInWorkArea();
}

void KeyboardController::HideAnimationFinished() {
  proxy_->HideKeyboardContainer(container_.get());
}

void KeyboardController::AddBoundsChangedObserver(aura::Window* window) {
  aura::Window* target_window = window ? window->GetToplevelWindow() : nullptr;
  if (target_window)
    window_bounds_observer_->AddObservedWindow(target_window);
}

}  // namespace keyboard
