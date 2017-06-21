// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_auralinux.h"

#include "base/auto_reset.h"
#include "base/environment.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ui {

InputMethodAuraLinux::InputMethodAuraLinux(
    internal::InputMethodDelegate* delegate)
    : text_input_type_(TEXT_INPUT_TYPE_NONE),
      is_sync_mode_(false),
      composition_changed_(false),
      suppress_next_result_(false) {
  SetDelegate(delegate);
  context_ =
      LinuxInputMethodContextFactory::instance()->CreateInputMethodContext(
          this, false);
  context_simple_ =
      LinuxInputMethodContextFactory::instance()->CreateInputMethodContext(
          this, true);
}

InputMethodAuraLinux::~InputMethodAuraLinux() {}

LinuxInputMethodContext* InputMethodAuraLinux::GetContextForTesting(
    bool is_simple) {
  return is_simple ? context_simple_.get() : context_.get();
}

// Overriden from InputMethod.

bool InputMethodAuraLinux::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

bool InputMethodAuraLinux::DispatchKeyEvent(const ui::KeyEvent& event) {
  DCHECK(event.type() == ET_KEY_PRESSED || event.type() == ET_KEY_RELEASED);
  DCHECK(system_toplevel_window_focused());

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event);

  suppress_next_result_ = false;
  composition_changed_ = false;
  result_text_.clear();

  bool filtered = false;
  {
    base::AutoReset<bool> flipper(&is_sync_mode_, true);
    if (text_input_type_ != TEXT_INPUT_TYPE_NONE &&
        text_input_type_ != TEXT_INPUT_TYPE_PASSWORD) {
      filtered = context_->DispatchKeyEvent(event);
    } else {
      filtered = context_simple_->DispatchKeyEvent(event);
    }
  }

  if (event.type() == ui::ET_KEY_PRESSED && filtered) {
    if (NeedInsertChar())
      DispatchKeyEventPostIME(event);
    else if (HasInputMethodResult())
      SendFakeProcessKeyEvent(event.flags());

    // Don't send VKEY_PROCESSKEY event if there is no result text or
    // composition. This is to workaround the weird behavior of IBus with US
    // keyboard, which mutes the keydown and later fake a new keydown with IME
    // result in sync mode. In that case, user would expect only
    // keydown/keypress/keyup event without an initial 229 keydown event.
  }

  TextInputClient* client = GetTextInputClient();
  // Processes the result text before composition for sync mode.
  if (!result_text_.empty()) {
    if (filtered && NeedInsertChar()) {
      for (const auto ch : result_text_)
        client->InsertChar(ch, event.flags());
    } else {
      // If |filtered| is false, that means the IME wants to commit some text
      // but still release the key to the application. For example, Korean IME
      // handles ENTER key to confirm its composition but still release it for
      // the default behavior (e.g. trigger search, etc.)
      // In such case, don't do InsertChar because a key should only trigger the
      // keydown event once.
      client->InsertText(result_text_);
    }
  }

  if (composition_changed_ && !IsTextInputTypeNone()) {
    // If composition changed, does SetComposition if composition is not empty.
    // And ClearComposition if composition is empty.
    if (!composition_.text.empty())
      client->SetCompositionText(composition_);
    else if (result_text_.empty())
      client->ClearCompositionText();
  }

  // Makes sure the cached composition is cleared after committing any text or
  // cleared composition.
  if (!client->HasCompositionText())
    composition_.Clear();

  if (!filtered) {
    DispatchKeyEventPostIME(event);
    if (event.type() == ui::ET_KEY_PRESSED) {
      // If a key event was not filtered by |context_| or |context_simple_|,
      // then it means the key event didn't generate any result text. For some
      // cases, the key event may still generate a valid character, eg. a
      // control-key event (ctrl-a, return, tab, etc.). We need to send the
      // character to the focused text input client by calling
      // TextInputClient::InsertChar().
      // Note: don't use |client| and use GetTextInputClient() here because
      // DispatchKeyEventPostIME may cause the current text input client change.
      base::char16 ch = event.GetCharacter();
      if (ch && GetTextInputClient())
        GetTextInputClient()->InsertChar(ch, event.flags());
    }
  }

  return true;
}

void InputMethodAuraLinux::UpdateContextFocusState() {
  bool old_text_input_type = text_input_type_;
  text_input_type_ = GetTextInputType();

  // We only focus in |context_| when the focus is in a textfield.
  if (old_text_input_type != TEXT_INPUT_TYPE_NONE &&
      text_input_type_ == TEXT_INPUT_TYPE_NONE) {
    context_->Blur();
  } else if (old_text_input_type == TEXT_INPUT_TYPE_NONE &&
             text_input_type_ != TEXT_INPUT_TYPE_NONE) {
    context_->Focus();
  }

  // |context_simple_| can be used in any textfield, including password box, and
  // even if the focused text input client's text input type is
  // ui::TEXT_INPUT_TYPE_NONE.
  if (GetTextInputClient())
    context_simple_->Focus();
  else
    context_simple_->Blur();
}

void InputMethodAuraLinux::OnTextInputTypeChanged(
    const TextInputClient* client) {
  UpdateContextFocusState();
  InputMethodBase::OnTextInputTypeChanged(client);
  // TODO(yoichio): Support inputmode HTML attribute.
}

void InputMethodAuraLinux::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputCaretBoundsChanged(client);
  context_->SetCursorLocation(GetTextInputClient()->GetCaretBounds());
}

void InputMethodAuraLinux::CancelComposition(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  ResetContext();
}

void InputMethodAuraLinux::ResetContext() {
  if (!GetTextInputClient())
    return;

  // To prevent any text from being committed when resetting the |context_|;
  is_sync_mode_ = true;
  suppress_next_result_ = true;

  context_->Reset();
  context_simple_->Reset();

  // Some input methods may not honour the reset call. Focusing out/in the
  // |context_| to make sure it gets reset correctly.
  if (text_input_type_ != TEXT_INPUT_TYPE_NONE) {
    context_->Blur();
    context_->Focus();
  }

  composition_.Clear();
  result_text_.clear();
  is_sync_mode_ = false;
  composition_changed_ = false;
}

void InputMethodAuraLinux::OnInputLocaleChanged() {
}

std::string InputMethodAuraLinux::GetInputLocale() {
  return "";
}

bool InputMethodAuraLinux::IsActive() {
  // InputMethodAuraLinux is always ready and up.
  return true;
}

bool InputMethodAuraLinux::IsCandidatePopupOpen() const {
  // There seems no way to detect candidate windows or any popups.
  return false;
}

// Overriden from ui::LinuxInputMethodContextDelegate

void InputMethodAuraLinux::OnCommit(const base::string16& text) {
  if (suppress_next_result_ || !GetTextInputClient()) {
    suppress_next_result_ = false;
    return;
  }

  if (is_sync_mode_) {
    // Append the text to the buffer, because commit signal might be fired
    // multiple times when processing a key event.
    result_text_.append(text);
  } else if (!IsTextInputTypeNone()) {
    // If we are not handling key event, do not bother sending text result if
    // the focused text input client does not support text input.
    SendFakeProcessKeyEvent(0);
    GetTextInputClient()->InsertText(text);
    composition_.Clear();
  }
}

void InputMethodAuraLinux::OnPreeditChanged(
    const CompositionText& composition_text) {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  if (is_sync_mode_) {
    if (!composition_.text.empty() || !composition_text.text.empty())
      composition_changed_ = true;
  } else {
    SendFakeProcessKeyEvent(0);
    GetTextInputClient()->SetCompositionText(composition_text);
  }

  composition_ = composition_text;
}

void InputMethodAuraLinux::OnPreeditEnd() {
  if (suppress_next_result_ || IsTextInputTypeNone())
    return;

  if (is_sync_mode_) {
    if (!composition_.text.empty()) {
      composition_.Clear();
      composition_changed_ = true;
    }
  } else {
    TextInputClient* client = GetTextInputClient();
    if (client && client->HasCompositionText()) {
      SendFakeProcessKeyEvent(0);
      client->ClearCompositionText();
    }
    composition_.Clear();
  }
}

// Overridden from InputMethodBase.

void InputMethodAuraLinux::OnFocus() {
  InputMethodBase::OnFocus();
  UpdateContextFocusState();
}

void InputMethodAuraLinux::OnBlur() {
  ConfirmCompositionText();
  InputMethodBase::OnBlur();
  UpdateContextFocusState();
}

void InputMethodAuraLinux::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  ConfirmCompositionText();
}

void InputMethodAuraLinux::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  UpdateContextFocusState();

  // Force to update caret bounds, in case the View thinks that the caret
  // bounds has not changed.
  if (text_input_type_ != TEXT_INPUT_TYPE_NONE)
    OnCaretBoundsChanged(GetTextInputClient());

  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
}

// private

bool InputMethodAuraLinux::HasInputMethodResult() {
  return !result_text_.empty() || composition_changed_;
}

bool InputMethodAuraLinux::NeedInsertChar() const {
  return IsTextInputTypeNone() ||
         (!composition_changed_ && composition_.text.empty() &&
          result_text_.length() == 1);
}

void InputMethodAuraLinux::SendFakeProcessKeyEvent(int flags) const {
  DispatchKeyEventPostIME(
      KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_PROCESSKEY, flags));
}

void InputMethodAuraLinux::ConfirmCompositionText() {
  TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText();

  ResetContext();
}

}  // namespace ui
