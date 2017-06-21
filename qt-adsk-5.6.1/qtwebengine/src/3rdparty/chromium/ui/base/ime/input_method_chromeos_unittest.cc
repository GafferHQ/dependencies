// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_chromeos.h"

#include <X11/Xlib.h>
#undef Bool
#undef FocusIn
#undef FocusOut
#undef None

#include <cstring>

#include "base/i18n/char_iterator.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/composition_text_chromeos.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_ime_candidate_window_handler.h"
#include "ui/base/ime/chromeos/mock_ime_engine_handler.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/geometry/rect.h"

using base::UTF8ToUTF16;
using base::UTF16ToUTF8;

namespace ui {
namespace {

const base::string16 kSampleText = base::UTF8ToUTF16(
    "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A");

typedef chromeos::IMEEngineHandlerInterface::KeyEventDoneCallback
    KeyEventCallback;

uint32 GetOffsetInUTF16(
    const base::string16& utf16_string, uint32 utf8_offset) {
  DCHECK_LT(utf8_offset, utf16_string.size());
  base::i18n::UTF16CharIterator char_iterator(&utf16_string);
  for (size_t i = 0; i < utf8_offset; ++i)
    char_iterator.Advance();
  return char_iterator.array_pos();
}

enum KeyEventHandlerBehavior {
  KEYEVENT_CONSUME,
  KEYEVENT_NOT_CONSUME,
};

}  // namespace


class TestableInputMethodChromeOS : public InputMethodChromeOS {
 public:
  explicit TestableInputMethodChromeOS(internal::InputMethodDelegate* delegate)
      : InputMethodChromeOS(delegate),
        process_key_event_post_ime_call_count_(0) {
  }

  struct ProcessKeyEventPostIMEArgs {
    ProcessKeyEventPostIMEArgs() : event(NULL), handled(false) {}
    const ui::KeyEvent* event;
    bool handled;
  };

  // Overridden from InputMethodChromeOS:
  void ProcessKeyEventPostIME(const ui::KeyEvent& key_event,
                              bool handled) override {
    InputMethodChromeOS::ProcessKeyEventPostIME(key_event, handled);
    process_key_event_post_ime_args_.event = &key_event;
    process_key_event_post_ime_args_.handled = handled;
    ++process_key_event_post_ime_call_count_;
  }

  void ResetCallCount() {
    process_key_event_post_ime_call_count_ = 0;
  }

  const ProcessKeyEventPostIMEArgs& process_key_event_post_ime_args() const {
    return process_key_event_post_ime_args_;
  }

  int process_key_event_post_ime_call_count() const {
    return process_key_event_post_ime_call_count_;
  }

  // Change access rights for testing.
  using InputMethodChromeOS::ExtractCompositionText;
  using InputMethodChromeOS::ResetContext;

 private:
  ProcessKeyEventPostIMEArgs process_key_event_post_ime_args_;
  int process_key_event_post_ime_call_count_;
};

class SynchronousKeyEventHandler {
 public:
  SynchronousKeyEventHandler(uint32 expected_keyval,
                             uint32 expected_keycode,
                             uint32 expected_state,
                             KeyEventHandlerBehavior behavior)
      : expected_keyval_(expected_keyval),
        expected_keycode_(expected_keycode),
        expected_state_(expected_state),
        behavior_(behavior) {}

  virtual ~SynchronousKeyEventHandler() {}

  void Run(uint32 keyval,
           uint32 keycode,
           uint32 state,
           const KeyEventCallback& callback) {
    EXPECT_EQ(expected_keyval_, keyval);
    EXPECT_EQ(expected_keycode_, keycode);
    EXPECT_EQ(expected_state_, state);
    callback.Run(behavior_ == KEYEVENT_CONSUME);
  }

 private:
  const uint32 expected_keyval_;
  const uint32 expected_keycode_;
  const uint32 expected_state_;
  const KeyEventHandlerBehavior behavior_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousKeyEventHandler);
};

class AsynchronousKeyEventHandler {
 public:
  AsynchronousKeyEventHandler(uint32 expected_keyval,
                              uint32 expected_keycode,
                              uint32 expected_state)
      : expected_keyval_(expected_keyval),
        expected_keycode_(expected_keycode),
        expected_state_(expected_state) {}

  virtual ~AsynchronousKeyEventHandler() {}

  void Run(uint32 keyval,
           uint32 keycode,
           uint32 state,
           const KeyEventCallback& callback) {
    EXPECT_EQ(expected_keyval_, keyval);
    EXPECT_EQ(expected_keycode_, keycode);
    EXPECT_EQ(expected_state_, state);
    callback_ = callback;
  }

  void RunCallback(KeyEventHandlerBehavior behavior) {
    callback_.Run(behavior == KEYEVENT_CONSUME);
  }

 private:
  const uint32 expected_keyval_;
  const uint32 expected_keycode_;
  const uint32 expected_state_;
  KeyEventCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousKeyEventHandler);
};

class SetSurroundingTextVerifier {
 public:
  SetSurroundingTextVerifier(const std::string& expected_surrounding_text,
                             uint32 expected_cursor_position,
                             uint32 expected_anchor_position)
      : expected_surrounding_text_(expected_surrounding_text),
        expected_cursor_position_(expected_cursor_position),
        expected_anchor_position_(expected_anchor_position) {}

  void Verify(const std::string& text,
              uint32 cursor_pos,
              uint32 anchor_pos) {
    EXPECT_EQ(expected_surrounding_text_, text);
    EXPECT_EQ(expected_cursor_position_, cursor_pos);
    EXPECT_EQ(expected_anchor_position_, anchor_pos);
  }

 private:
  const std::string expected_surrounding_text_;
  const uint32 expected_cursor_position_;
  const uint32 expected_anchor_position_;

  DISALLOW_COPY_AND_ASSIGN(SetSurroundingTextVerifier);
};

class InputMethodChromeOSTest : public internal::InputMethodDelegate,
                                public testing::Test,
                                public DummyTextInputClient {
 public:
  InputMethodChromeOSTest()
      : dispatched_key_event_(ui::ET_UNKNOWN, ui::VKEY_UNKNOWN, ui::EF_NONE) {
    ResetFlags();
  }

  ~InputMethodChromeOSTest() override {}

  void SetUp() override {
    chromeos::IMEBridge::Initialize();

    mock_ime_engine_handler_.reset(
        new chromeos::MockIMEEngineHandler());
    chromeos::IMEBridge::Get()->SetCurrentEngineHandler(
        mock_ime_engine_handler_.get());

    mock_ime_candidate_window_handler_.reset(
        new chromeos::MockIMECandidateWindowHandler());
    chromeos::IMEBridge::Get()->SetCandidateWindowHandler(
        mock_ime_candidate_window_handler_.get());

    ime_.reset(new TestableInputMethodChromeOS(this));
    ime_->SetFocusedTextInputClient(this);
  }

  void TearDown() override {
    if (ime_.get())
      ime_->SetFocusedTextInputClient(NULL);
    ime_.reset();
    chromeos::IMEBridge::Get()->SetCurrentEngineHandler(NULL);
    chromeos::IMEBridge::Get()->SetCandidateWindowHandler(NULL);
    mock_ime_engine_handler_.reset();
    mock_ime_candidate_window_handler_.reset();
    chromeos::IMEBridge::Shutdown();

    ResetFlags();
  }

  // Overridden from ui::internal::InputMethodDelegate:
  bool DispatchKeyEventPostIME(const ui::KeyEvent& event) override {
    dispatched_key_event_ = event;
    return false;
  }

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const CompositionText& composition) override {
    composition_text_ = composition;
  }
  void ConfirmCompositionText() override {
    confirmed_text_ = composition_text_;
    composition_text_.Clear();
  }
  void ClearCompositionText() override { composition_text_.Clear(); }
  void InsertText(const base::string16& text) override {
    inserted_text_ = text;
  }
  void InsertChar(base::char16 ch, int flags) override {
    inserted_char_ = ch;
    inserted_char_flags_ = flags;
  }
  TextInputType GetTextInputType() const override { return input_type_; }
  TextInputMode GetTextInputMode() const override { return input_mode_; }
  bool CanComposeInline() const override { return can_compose_inline_; }
  gfx::Rect GetCaretBounds() const override { return caret_bounds_; }
  bool HasCompositionText() const override {
    CompositionText empty;
    return composition_text_ != empty;
  }
  bool GetTextRange(gfx::Range* range) const override {
    *range = text_range_;
    return true;
  }
  bool GetSelectionRange(gfx::Range* range) const override {
    *range = selection_range_;
    return true;
  }
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override {
    *text = surrounding_text_.substr(range.GetMin(), range.length());
    return true;
  }
  void OnInputMethodChanged() override {
    ++on_input_method_changed_call_count_;
  }

  bool HasNativeEvent() const {
    return dispatched_key_event_.HasNativeEvent();
  }

  void ResetFlags() {
    dispatched_key_event_ = ui::KeyEvent(ui::ET_UNKNOWN, ui::VKEY_UNKNOWN,
                                         ui::EF_NONE);

    composition_text_.Clear();
    confirmed_text_.Clear();
    inserted_text_.clear();
    inserted_char_ = 0;
    inserted_char_flags_ = 0;
    on_input_method_changed_call_count_ = 0;

    input_type_ = TEXT_INPUT_TYPE_NONE;
    input_mode_ = TEXT_INPUT_MODE_DEFAULT;
    can_compose_inline_ = true;
    caret_bounds_ = gfx::Rect();
  }

  scoped_ptr<TestableInputMethodChromeOS> ime_;

  // Copy of the dispatched key event.
  ui::KeyEvent dispatched_key_event_;

  // Variables for remembering the parameters that are passed to
  // ui::TextInputClient functions.
  CompositionText composition_text_;
  CompositionText confirmed_text_;
  base::string16 inserted_text_;
  base::char16 inserted_char_;
  unsigned int on_input_method_changed_call_count_;
  int inserted_char_flags_;

  // Variables that will be returned from the ui::TextInputClient functions.
  TextInputType input_type_;
  TextInputMode input_mode_;
  bool can_compose_inline_;
  gfx::Rect caret_bounds_;
  gfx::Range text_range_;
  gfx::Range selection_range_;
  base::string16 surrounding_text_;

  scoped_ptr<chromeos::MockIMEEngineHandler> mock_ime_engine_handler_;
  scoped_ptr<chromeos::MockIMECandidateWindowHandler>
      mock_ime_candidate_window_handler_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodChromeOSTest);
};

// Tests public APIs in ui::InputMethod first.

TEST_F(InputMethodChromeOSTest, GetInputLocale) {
  // ui::InputMethodChromeOS does not support the API.
  ime_->OnFocus();
  EXPECT_EQ("", ime_->GetInputLocale());
}

TEST_F(InputMethodChromeOSTest, IsActive) {
  ime_->OnFocus();
  // ui::InputMethodChromeOS always returns true.
  EXPECT_TRUE(ime_->IsActive());
}

TEST_F(InputMethodChromeOSTest, GetInputTextType) {
  ime_->OnFocus();
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_TEXT, ime_->GetTextInputType());
}

TEST_F(InputMethodChromeOSTest, CanComposeInline) {
  ime_->OnFocus();
  EXPECT_TRUE(ime_->CanComposeInline());
  can_compose_inline_ = false;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_FALSE(ime_->CanComposeInline());
}

TEST_F(InputMethodChromeOSTest, GetTextInputClient) {
  ime_->OnFocus();
  EXPECT_EQ(this, ime_->GetTextInputClient());
  ime_->SetFocusedTextInputClient(NULL);
  EXPECT_EQ(NULL, ime_->GetTextInputClient());
}

TEST_F(InputMethodChromeOSTest, GetInputTextType_WithoutFocusedClient) {
  ime_->OnFocus();
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  ime_->SetFocusedTextInputClient(NULL);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  // The OnTextInputTypeChanged() call above should be ignored since |this| is
  // not the current focused client.
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());

  ime_->SetFocusedTextInputClient(this);
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
}

TEST_F(InputMethodChromeOSTest, GetInputTextType_WithoutFocusedWindow) {
  ime_->OnFocus();
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  ime_->OnBlur();
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  // The OnTextInputTypeChanged() call above should be ignored since the top-
  // level window which the ime_ is attached to is not currently focused.
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());

  ime_->OnFocus();
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
}

TEST_F(InputMethodChromeOSTest, GetInputTextType_WithoutFocusedWindow2) {
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE, ime_->GetTextInputType());

  ime_->OnFocus();
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(TEXT_INPUT_TYPE_PASSWORD, ime_->GetTextInputType());
}

// Confirm that IBusClient::FocusIn is called on "connected" if input_type_ is
// TEXT.
TEST_F(InputMethodChromeOSTest, FocusIn_Text) {
  ime_->OnFocus();
  // A context shouldn't be created since the daemon is not running.
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  // Since a form has focus, IBusClient::FocusIn() should be called.
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(
      1,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  // ui::TextInputClient::OnInputMethodChanged() should be called when
  // ui::InputMethodChromeOS connects/disconnects to/from ibus-daemon and the
  // current text input type is not NONE.
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that InputMethodEngine::FocusIn is called on "connected" even if
// input_type_ is PASSWORD.
TEST_F(InputMethodChromeOSTest, FocusIn_Password) {
  ime_->OnFocus();
  EXPECT_EQ(0U, on_input_method_changed_call_count_);
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  // InputMethodEngine::FocusIn() should be called even for password field.
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1U, on_input_method_changed_call_count_);
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodChromeOSTest, FocusOut_None) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnFocus();
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  input_type_ = TEXT_INPUT_TYPE_NONE;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
}

// Confirm that IBusClient::FocusOut is called as expected.
TEST_F(InputMethodChromeOSTest, FocusOut_Password) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnFocus();
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  input_type_ = TEXT_INPUT_TYPE_PASSWORD;
  ime_->OnTextInputTypeChanged(this);
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
}

// FocusIn/FocusOut scenario test
TEST_F(InputMethodChromeOSTest, Focus_Scenario) {
  ime_->OnFocus();
  // Confirm that both FocusIn and FocusOut are NOT called.
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_NONE,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_DEFAULT,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_type_ = TEXT_INPUT_TYPE_TEXT;
  input_mode_ = TEXT_INPUT_MODE_LATIN;
  ime_->OnTextInputTypeChanged(this);
  // Confirm that only FocusIn is called, the TextInputType is TEXT and the
  // TextInputMode is LATIN..
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(0, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_LATIN,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_mode_ = TEXT_INPUT_MODE_KANA;
  ime_->OnTextInputTypeChanged(this);
  // Confirm that both FocusIn and FocusOut are called for mode change.
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(1, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_TEXT,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_KANA,
            mock_ime_engine_handler_->last_text_input_context().mode);

  input_type_ = TEXT_INPUT_TYPE_URL;
  ime_->OnTextInputTypeChanged(this);
  // Confirm that both FocusIn and FocusOut are called and the TextInputType is
  // changed to URL.
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(2, mock_ime_engine_handler_->focus_out_call_count());
  EXPECT_EQ(TEXT_INPUT_TYPE_URL,
            mock_ime_engine_handler_->last_text_input_context().type);
  EXPECT_EQ(TEXT_INPUT_MODE_KANA,
            mock_ime_engine_handler_->last_text_input_context().mode);

  // Confirm that FocusOut is called when set focus to NULL client.
  ime_->SetFocusedTextInputClient(NULL);
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_out_call_count());
  // Confirm that FocusIn is called when set focus to this client.
  ime_->SetFocusedTextInputClient(this);
  EXPECT_EQ(4, mock_ime_engine_handler_->focus_in_call_count());
  EXPECT_EQ(3, mock_ime_engine_handler_->focus_out_call_count());
}

// Test if the new |caret_bounds_| is correctly sent to ibus-daemon.
TEST_F(InputMethodChromeOSTest, OnCaretBoundsChanged) {
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnFocus();
  EXPECT_EQ(
      1,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  caret_bounds_ = gfx::Rect(1, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(
      2,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(
      3,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
  caret_bounds_ = gfx::Rect(0, 2, 3, 4);  // unchanged
  ime_->OnCaretBoundsChanged(this);
  // Current InputMethodChromeOS implementation performs the IPC
  // regardless of the bounds are changed or not.
  EXPECT_EQ(
      4,
      mock_ime_candidate_window_handler_->set_cursor_bounds_call_count());
}

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_NoAttribute) {
  const base::string16 kSampleAsciiText = UTF8ToUTF16("Sample Text");
  const uint32 kCursorPos = 2UL;

  chromeos::CompositionText chromeos_composition_text;
  chromeos_composition_text.set_text(kSampleAsciiText);

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
  EXPECT_EQ(kSampleAsciiText, composition_text.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  // If there is no underline, |underlines| contains one underline and it is
  // whole text underline.
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(0UL, composition_text.underlines[0].start_offset);
  EXPECT_EQ(kSampleAsciiText.size(), composition_text.underlines[0].end_offset);
  EXPECT_FALSE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_SingleUnderline) {
  const uint32 kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  chromeos::CompositionText chromeos_composition_text;
  chromeos_composition_text.set_text(kSampleText);
  chromeos::CompositionText::UnderlineAttribute underline;
  underline.type = chromeos::CompositionText::COMPOSITION_TEXT_UNDERLINE_SINGLE;
  underline.start_index = 1UL;
  underline.end_index = 4UL;
  chromeos_composition_text.mutable_underline_attributes()->push_back(
      underline);

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
  EXPECT_EQ(kSampleText, composition_text.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_index),
            composition_text.underlines[0].end_offset);
  // Single underline represents as black thin line.
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_FALSE(composition_text.underlines[0].thick);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text.underlines[0].background_color);
}

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_DoubleUnderline) {
  const uint32 kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  chromeos::CompositionText chromeos_composition_text;
  chromeos_composition_text.set_text(kSampleText);
  chromeos::CompositionText::UnderlineAttribute underline;
  underline.type = chromeos::CompositionText::COMPOSITION_TEXT_UNDERLINE_DOUBLE;
  underline.start_index = 1UL;
  underline.end_index = 4UL;
  chromeos_composition_text.mutable_underline_attributes()->push_back(
      underline);

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
  EXPECT_EQ(kSampleText, composition_text.text);
  // If there is no selection, |selection| represents cursor position.
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_index),
            composition_text.underlines[0].end_offset);
  // Double underline represents as black thick line.
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text.underlines[0].background_color);
}

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_ErrorUnderline) {
  const uint32 kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  chromeos::CompositionText chromeos_composition_text;
  chromeos_composition_text.set_text(kSampleText);
  chromeos::CompositionText::UnderlineAttribute underline;
  underline.type = chromeos::CompositionText::COMPOSITION_TEXT_UNDERLINE_ERROR;
  underline.start_index = 1UL;
  underline.end_index = 4UL;
  chromeos_composition_text.mutable_underline_attributes()->push_back(
      underline);

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
  EXPECT_EQ(kSampleText, composition_text.text);
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.start_index),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, underline.end_index),
            composition_text.underlines[0].end_offset);
  // Error underline represents as red thin line.
  EXPECT_EQ(SK_ColorRED, composition_text.underlines[0].color);
  EXPECT_FALSE(composition_text.underlines[0].thick);
}

TEST_F(InputMethodChromeOSTest, ExtractCompositionTextTest_Selection) {
  const uint32 kCursorPos = 2UL;

  // Set up chromeos composition text with one underline attribute.
  chromeos::CompositionText chromeos_composition_text;
  chromeos_composition_text.set_text(kSampleText);
  chromeos_composition_text.set_selection_start(1UL);
  chromeos_composition_text.set_selection_end(4UL);

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
  EXPECT_EQ(kSampleText, composition_text.text);
  EXPECT_EQ(kCursorPos, composition_text.selection.start());
  EXPECT_EQ(kCursorPos, composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_start()),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_end()),
            composition_text.underlines[0].end_offset);
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text.underlines[0].background_color);
}

TEST_F(InputMethodChromeOSTest,
       ExtractCompositionTextTest_SelectionStartWithCursor) {
  const uint32 kCursorPos = 1UL;

  // Set up chromeos composition text with one underline attribute.
  chromeos::CompositionText chromeos_composition_text;
  chromeos_composition_text.set_text(kSampleText);
  chromeos_composition_text.set_selection_start(kCursorPos);
  chromeos_composition_text.set_selection_end(4UL);

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
  EXPECT_EQ(kSampleText, composition_text.text);
  // If the cursor position is same as selection bounds, selection start
  // position become opposit side of selection from cursor.
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_end()),
            composition_text.selection.start());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, kCursorPos),
            composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_start()),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_end()),
            composition_text.underlines[0].end_offset);
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text.underlines[0].background_color);
}

TEST_F(InputMethodChromeOSTest,
       ExtractCompositionTextTest_SelectionEndWithCursor) {
  const uint32 kCursorPos = 4UL;

  // Set up chromeos composition text with one underline attribute.
  chromeos::CompositionText chromeos_composition_text;
  chromeos_composition_text.set_text(kSampleText);
  chromeos_composition_text.set_selection_start(1UL);
  chromeos_composition_text.set_selection_end(kCursorPos);

  CompositionText composition_text;
  ime_->ExtractCompositionText(
      chromeos_composition_text, kCursorPos, &composition_text);
  EXPECT_EQ(kSampleText, composition_text.text);
  // If the cursor position is same as selection bounds, selection start
  // position become opposit side of selection from cursor.
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_start()),
            composition_text.selection.start());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText, kCursorPos),
            composition_text.selection.end());
  ASSERT_EQ(1UL, composition_text.underlines.size());
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_start()),
            composition_text.underlines[0].start_offset);
  EXPECT_EQ(GetOffsetInUTF16(kSampleText,
                             chromeos_composition_text.selection_end()),
            composition_text.underlines[0].end_offset);
  EXPECT_EQ(SK_ColorBLACK, composition_text.underlines[0].color);
  EXPECT_TRUE(composition_text.underlines[0].thick);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            composition_text.underlines[0].background_color);
}

TEST_F(InputMethodChromeOSTest, SurroundingText_NoSelectionTest) {
  ime_->OnFocus();
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = UTF8ToUTF16("abcdef");
  text_range_ = gfx::Range(0, 6);
  selection_range_ = gfx::Range(3, 3);

  // Set the verifier for SetSurroundingText mock call.
  SetSurroundingTextVerifier verifier(UTF16ToUTF8(surrounding_text_), 3, 3);


  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
  EXPECT_EQ(UTF16ToUTF8(surrounding_text_),
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(3U,
            mock_ime_engine_handler_->last_set_surrounding_cursor_pos());
  EXPECT_EQ(3U,
            mock_ime_engine_handler_->last_set_surrounding_anchor_pos());
}

TEST_F(InputMethodChromeOSTest, SurroundingText_SelectionTest) {
  ime_->OnFocus();
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = UTF8ToUTF16("abcdef");
  text_range_ = gfx::Range(0, 6);
  selection_range_ = gfx::Range(2, 5);

  // Set the verifier for SetSurroundingText mock call.
  SetSurroundingTextVerifier verifier(UTF16ToUTF8(surrounding_text_), 2, 5);

  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
  EXPECT_EQ(UTF16ToUTF8(surrounding_text_),
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(2U,
            mock_ime_engine_handler_->last_set_surrounding_cursor_pos());
  EXPECT_EQ(5U,
            mock_ime_engine_handler_->last_set_surrounding_anchor_pos());
}

TEST_F(InputMethodChromeOSTest, SurroundingText_PartialText) {
  ime_->OnFocus();
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  surrounding_text_ = UTF8ToUTF16("abcdefghij");
  text_range_ = gfx::Range(5, 10);
  selection_range_ = gfx::Range(7, 9);

  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(1,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
  // Set the verifier for SetSurroundingText mock call.
  // Here (2, 4) is selection range in expected surrounding text coordinates.
  EXPECT_EQ("fghij",
            mock_ime_engine_handler_->last_set_surrounding_text());
  EXPECT_EQ(2U,
            mock_ime_engine_handler_->last_set_surrounding_cursor_pos());
  EXPECT_EQ(4U,
            mock_ime_engine_handler_->last_set_surrounding_anchor_pos());
}

TEST_F(InputMethodChromeOSTest, SurroundingText_BecomeEmptyText) {
  ime_->OnFocus();
  // Click a text input form.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  // Set the TextInputClient behaviors.
  // If the surrounding text becomes empty, text_range become (0, 0) and
  // selection range become invalid.
  surrounding_text_ = UTF8ToUTF16("");
  text_range_ = gfx::Range(0, 0);
  selection_range_ = gfx::Range::InvalidRange();

  ime_->OnCaretBoundsChanged(this);

  // Check the call count.
  EXPECT_EQ(0,
            mock_ime_engine_handler_->set_surrounding_text_call_count());

  // Should not be called twice with same condition.
  ime_->OnCaretBoundsChanged(this);
  EXPECT_EQ(0,
            mock_ime_engine_handler_->set_surrounding_text_call_count());
}

class InputMethodChromeOSKeyEventTest : public InputMethodChromeOSTest {
 public:
  InputMethodChromeOSKeyEventTest() {}
  ~InputMethodChromeOSKeyEventTest() override {}

  void SetUp() override {
    InputMethodChromeOSTest::SetUp();
    ime_->OnFocus();
  }

  DISALLOW_COPY_AND_ASSIGN(InputMethodChromeOSKeyEventTest);
};

TEST_F(InputMethodChromeOSKeyEventTest, KeyEventDelayResponseTest) {
  const int kFlags = ui::EF_SHIFT_DOWN;
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_A, kFlags);

  // Do key event.
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);
  ime_->DispatchKeyEvent(event);

  // Check before state.
  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(1, mock_ime_engine_handler_->process_key_event_call_count());
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(kFlags, key_event->flags());
  EXPECT_EQ(0, ime_->process_key_event_post_ime_call_count());

  (static_cast<chromeos::IMEInputContextHandlerInterface*>(ime_.get()))
      ->CommitText("A");

  EXPECT_EQ(0, inserted_char_);

  // Do callback.
  mock_ime_engine_handler_->last_passed_callback().Run(true);

  // Check the results
  EXPECT_EQ(1, ime_->process_key_event_post_ime_call_count());
  const ui::KeyEvent* stored_event =
      ime_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_A, stored_event->key_code());
  EXPECT_EQ(kFlags, stored_event->flags());
  EXPECT_TRUE(ime_->process_key_event_post_ime_args().handled);

  EXPECT_EQ(L'A', inserted_char_);
}

TEST_F(InputMethodChromeOSKeyEventTest, MultiKeyEventDelayResponseTest) {
  // Preparation
  input_type_ = TEXT_INPUT_TYPE_TEXT;
  ime_->OnTextInputTypeChanged(this);

  const int kFlags = ui::EF_SHIFT_DOWN;
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_B, kFlags);

  // Do key event.
  ime_->DispatchKeyEvent(event);
  const ui::KeyEvent* key_event =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_B, key_event->key_code());
  EXPECT_EQ(kFlags, key_event->flags());

  KeyEventCallback first_callback =
      mock_ime_engine_handler_->last_passed_callback();

  // Do key event again.
  ui::KeyEvent event2(ui::ET_KEY_PRESSED, ui::VKEY_C, kFlags);

  ime_->DispatchKeyEvent(event2);
  const ui::KeyEvent* key_event2 =
      mock_ime_engine_handler_->last_processed_key_event();
  EXPECT_EQ(ui::VKEY_C, key_event2->key_code());
  EXPECT_EQ(kFlags, key_event2->flags());

  // Check before state.
  EXPECT_EQ(2,
            mock_ime_engine_handler_->process_key_event_call_count());
  EXPECT_EQ(0, ime_->process_key_event_post_ime_call_count());

  chromeos::CompositionText comp;
  comp.set_text(base::ASCIIToUTF16("B"));
  (static_cast<chromeos::IMEInputContextHandlerInterface*>(ime_.get()))
      ->UpdateCompositionText(comp, comp.text().length(), true);

  EXPECT_EQ(0, composition_text_.text[0]);

  // Do callback for first key event.
  first_callback.Run(true);

  EXPECT_EQ(comp.text(), composition_text_.text);

  // Check the results for first key event.
  EXPECT_EQ(1, ime_->process_key_event_post_ime_call_count());
  const ui::KeyEvent* stored_event =
      ime_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_B, stored_event->key_code());
  EXPECT_EQ(kFlags, stored_event->flags());
  EXPECT_TRUE(ime_->process_key_event_post_ime_args().handled);
  EXPECT_EQ(0, inserted_char_);

  // Do callback for second key event.
  mock_ime_engine_handler_->last_passed_callback().Run(false);

  // Check the results for second key event.
  EXPECT_EQ(2, ime_->process_key_event_post_ime_call_count());
  stored_event = ime_->process_key_event_post_ime_args().event;
  EXPECT_EQ(ui::VKEY_C, stored_event->key_code());
  EXPECT_EQ(kFlags, stored_event->flags());
  EXPECT_FALSE(ime_->process_key_event_post_ime_args().handled);

  EXPECT_EQ(L'C', inserted_char_);
}

// TODO(nona): Introduce ProcessKeyEventPostIME tests(crbug.com/156593).

}  // namespace ui
