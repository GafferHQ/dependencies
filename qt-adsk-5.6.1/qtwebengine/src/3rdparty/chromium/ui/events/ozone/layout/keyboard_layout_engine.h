// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/layout/events_ozone_layout_export.h"

namespace ui {

enum class DomCode;
enum class DomKey;

// A KeyboardLayoutEngine provides a platform-independent interface to
// key mapping. Key mapping provides a meaning (DomKey and character,
// and optionally Windows key code) for a physical key press (DomCode
// and modifier flags).
//
// This interface does not expose individual layouts because it must support
// platforms that only provide for one active system layout, and/or platforms
// where layouts have no accessible representation.
class EVENTS_OZONE_LAYOUT_EXPORT KeyboardLayoutEngine {
 public:
  KeyboardLayoutEngine() {}
  virtual ~KeyboardLayoutEngine() {}

  // Returns true if it is possible to change the current layout.
  virtual bool CanSetCurrentLayout() const = 0;

  // Sets the current layout; returns true on success.
  // Drop-in replacement for ImeKeyboard::SetCurrentKeyboardLayoutByName();
  // the argument string is defined by that interface (crbug.com/362698).
  virtual bool SetCurrentLayoutByName(const std::string& layout_name) = 0;

  // Returns true if the current layout makes use of the ISO Level 5 Shift key.
  // Drop-in replacement for ImeKeyboard::IsISOLevel5ShiftAvailable().
  virtual bool UsesISOLevel5Shift() const = 0;

  // Returns true if the current layout makes use of the AltGr
  // (ISO Level 3 Shift) key.
  // Drop-in replacement for ImeKeyboard::IsAltGrAvailable().
  virtual bool UsesAltGr() const = 0;

  // Provides the meaning of a physical key.
  //
  // The caller must supply valid addresses for all the output parameters;
  // the function must not use their initial values.
  //
  // Returns true if it can determine the DOM meaning (i.e. ui::DomKey and
  // character) and the corresponding legacy KeyboardCode from the given
  // physical state (ui::DomCode and ui::EventFlags), OR if it can determine
  // that there is no meaning in the current layout (e.g. the key is unbound).
  // In the latter case, the function sets *dom_key to UNIDENTIFIED, *character
  // to 0, and *key_code to VKEY_UNKNOWN.
  //
  // Returns false if it cannot determine the meaning (and cannot determine
  // that there is none); in this case it does not set any of the output
  // parameters.
  //
  // TODO(kpschoedel): remove the platform_keycode output. crbug.com/442757
  virtual bool Lookup(DomCode dom_code,
                      int event_flags,
                      DomKey* dom_key,
                      base::char16* character,
                      KeyboardCode* key_code,
                      uint32* platform_keycode) const = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_KEYBOARD_LAYOUT_ENGINE_H_
