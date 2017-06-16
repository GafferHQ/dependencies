// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/no/no_keyboard_layout_engine.h"

namespace ui {

bool NoKeyboardLayoutEngine::CanSetCurrentLayout() const {
  return false;
}

bool NoKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name) {
  return false;
}

bool NoKeyboardLayoutEngine::UsesISOLevel5Shift() const {
  return false;
}

bool NoKeyboardLayoutEngine::UsesAltGr() const {
  return false;
}

bool NoKeyboardLayoutEngine::Lookup(DomCode dom_code,
                                    int flags,
                                    DomKey* dom_key,
                                    base::char16* character,
                                    KeyboardCode* key_code,
                                    uint32* platform_keycode) const {
  return false;
}

}  // namespace ui
