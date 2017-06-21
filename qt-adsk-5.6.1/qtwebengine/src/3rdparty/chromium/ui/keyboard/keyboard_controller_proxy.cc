// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller_proxy.h"

#include "base/command_line.h"
#include "base/values.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/wm/core/shadow.h"

namespace {

// The WebContentsDelegate for the keyboard.
// The delegate deletes itself when the keyboard is destroyed.
class KeyboardContentsDelegate : public content::WebContentsDelegate,
                                 public content::WebContentsObserver {
 public:
  KeyboardContentsDelegate(keyboard::KeyboardControllerProxy* proxy)
      : proxy_(proxy) {}
  ~KeyboardContentsDelegate() override {}

 private:
  // Overridden from content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    source->GetController().LoadURL(
        params.url, params.referrer, params.transition, params.extra_headers);
    Observe(source);
    return source;
  }

  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override {
    return false;
  }

  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int route_id,
      int main_frame_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override {
    return false;
  }

  bool IsPopupOrPanel(const content::WebContents* source) const override {
    return true;
  }

  void MoveContents(content::WebContents* source,
                    const gfx::Rect& pos) override {
    aura::Window* keyboard = proxy_->GetKeyboardWindow();
    // keyboard window must have been added to keyboard container window at this
    // point. Otherwise, wrong keyboard bounds is used and may cause problem as
    // described in crbug.com/367788.
    DCHECK(keyboard->parent());
    // keyboard window bounds may not set to |pos| after this call. If keyboard
    // is in FULL_WIDTH mode, only the height of keyboard window will be
    // changed.
    keyboard->SetBounds(pos);
  }

  // Overridden from content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override {
    proxy_->RequestAudioInput(web_contents, request, callback);
  }

  // Overridden from content::WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

  keyboard::KeyboardControllerProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContentsDelegate);
};

}  // namespace

namespace keyboard {

KeyboardControllerProxy::KeyboardControllerProxy(
    content::BrowserContext* context)
    : browser_context_(context),
      default_url_(kKeyboardURL),
      keyboard_controller_(nullptr) {
}

KeyboardControllerProxy::~KeyboardControllerProxy() {
}

const GURL& KeyboardControllerProxy::GetVirtualKeyboardUrl() {
  if (keyboard::IsInputViewEnabled()) {
    const GURL& override_url = GetOverrideContentUrl();
    return override_url.is_valid() ? override_url : default_url_;
  } else {
    return default_url_;
  }
}

void KeyboardControllerProxy::LoadContents(const GURL& url) {
  if (keyboard_contents_) {
    content::OpenURLParams params(
        url,
        content::Referrer(),
        SINGLETON_TAB,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
        false);
    keyboard_contents_->OpenURL(params);
  }
}

aura::Window* KeyboardControllerProxy::GetKeyboardWindow() {
  if (!keyboard_contents_) {
    content::BrowserContext* context = browser_context();
    keyboard_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(context,
            content::SiteInstance::CreateForURL(context,
                                                GetVirtualKeyboardUrl()))));
    keyboard_contents_->SetDelegate(new KeyboardContentsDelegate(this));
    SetupWebContents(keyboard_contents_.get());
    LoadContents(GetVirtualKeyboardUrl());
    keyboard_contents_->GetNativeView()->AddObserver(this);
  }

  return keyboard_contents_->GetNativeView();
}

bool KeyboardControllerProxy::HasKeyboardWindow() const {
  return keyboard_contents_;
}

void KeyboardControllerProxy::ShowKeyboardContainer(aura::Window* container) {
  GetKeyboardWindow()->Show();
  container->Show();
}

void KeyboardControllerProxy::HideKeyboardContainer(aura::Window* container) {
  container->Hide();
  GetKeyboardWindow()->Hide();
}

void KeyboardControllerProxy::SetUpdateInputType(ui::TextInputType type) {
}

void KeyboardControllerProxy::EnsureCaretInWorkArea() {
  if (GetInputMethod()->GetTextInputClient()) {
    aura::Window* keyboard_window = GetKeyboardWindow();
    aura::Window* root_window = keyboard_window->GetRootWindow();
    gfx::Rect available_bounds = root_window->bounds();
    gfx::Rect keyboard_bounds = keyboard_window->bounds();
    available_bounds.set_height(available_bounds.height() -
        keyboard_bounds.height());
    GetInputMethod()->GetTextInputClient()->EnsureCaretInRect(available_bounds);
  }
}

void KeyboardControllerProxy::LoadSystemKeyboard() {
  DCHECK(keyboard_contents_);
  if (keyboard_contents_->GetURL() != default_url_) {
    // TODO(bshe): The height of system virtual keyboard and IME virtual
    // keyboard may different. The height needs to be restored too.
    LoadContents(default_url_);
  }
}

void KeyboardControllerProxy::ReloadKeyboardIfNeeded() {
  DCHECK(keyboard_contents_);
  if (keyboard_contents_->GetURL() != GetVirtualKeyboardUrl()) {
    if (keyboard_contents_->GetURL().GetOrigin() !=
        GetVirtualKeyboardUrl().GetOrigin()) {
      // Sets keyboard window rectangle to 0 and close current page before
      // navigate to a keyboard in a different extension. This keeps the UX the
      // same as Android. Note we need to explicitly close current page as it
      // might try to resize keyboard window in javascript on a resize event.
      GetKeyboardWindow()->SetBounds(gfx::Rect());
      keyboard_contents_->ClosePage();
      keyboard_controller()->SetKeyboardMode(FULL_WIDTH);
    }
    LoadContents(GetVirtualKeyboardUrl());
  }
}

void KeyboardControllerProxy::SetController(KeyboardController* controller) {
  keyboard_controller_ = controller;
}

void KeyboardControllerProxy::SetupWebContents(content::WebContents* contents) {
}

void KeyboardControllerProxy::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  if (!shadow_) {
    shadow_.reset(new wm::Shadow());
    shadow_->Init(wm::Shadow::STYLE_ACTIVE);
    shadow_->layer()->SetVisible(true);
    DCHECK(keyboard_contents_->GetNativeView()->parent());
    keyboard_contents_->GetNativeView()->parent()->layer()->Add(
        shadow_->layer());
  }

  shadow_->SetContentBounds(new_bounds);
}

void KeyboardControllerProxy::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);
}

}  // namespace keyboard
