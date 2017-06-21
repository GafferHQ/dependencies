// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/dom/keycode_converter.h"

#include <map>

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

using ui::KeycodeConverter;

namespace {

#if defined(OS_WIN)
const size_t kExpectedMappedKeyCount = 138;
#elif defined(OS_LINUX)
const size_t kExpectedMappedKeyCount = 168;
#elif defined(OS_MACOSX)
const size_t kExpectedMappedKeyCount = 118;
#else
const size_t kExpectedMappedKeyCount = 0;
#endif

const uint32_t kUsbNonExistentKeycode = 0xffffff;
const uint32_t kUsbUsBackslash =        0x070031;
const uint32_t kUsbNonUsHash =          0x070032;

TEST(UsbKeycodeMap, Basic) {
  // Verify that the first element in the table is the "invalid" code.
  const ui::KeycodeMapEntry* keycode_map =
      ui::KeycodeConverter::GetKeycodeMapForTest();
  EXPECT_EQ(ui::KeycodeConverter::InvalidUsbKeycode(),
            keycode_map[0].usb_keycode);
  EXPECT_EQ(ui::KeycodeConverter::InvalidNativeKeycode(),
            keycode_map[0].native_keycode);
  EXPECT_EQ(ui::KeycodeConverter::InvalidNativeKeycode(),
            ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::NONE));

  // Verify that there are no duplicate entries in the mapping.
  std::map<uint32_t, uint16_t> usb_to_native;
  std::map<uint16_t, uint32_t> native_to_usb;
  size_t numEntries = ui::KeycodeConverter::NumKeycodeMapEntriesForTest();
  for (size_t i = 0; i < numEntries; ++i) {
    const ui::KeycodeMapEntry* entry = &keycode_map[i];
    // Don't test keys with no native keycode mapping on this platform.
    if (entry->native_keycode == ui::KeycodeConverter::InvalidNativeKeycode())
      continue;

    // Verify UsbKeycodeToNativeKeycode works for this key.
    EXPECT_EQ(
        entry->native_keycode,
        ui::KeycodeConverter::UsbKeycodeToNativeKeycode(entry->usb_keycode));

    // Verify DomCodeToNativeKeycode works correctly.
    ui::DomCode dom_code =
        ui::KeycodeConverter::CodeStringToDomCode(entry->code);
    if (entry->code) {
      EXPECT_EQ(entry->native_keycode,
                ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code));
    } else {
      EXPECT_EQ(ui::DomCode::NONE, dom_code);
    }

    // Verify that the USB or native codes aren't duplicated.
    EXPECT_EQ(0U, usb_to_native.count(entry->usb_keycode))
        << " duplicate of USB code 0x" << std::hex << std::setfill('0')
        << std::setw(6) << entry->usb_keycode
        << " to native 0x"
        << std::setw(4) << entry->native_keycode
        << " (previous was 0x"
        << std::setw(4) << usb_to_native[entry->usb_keycode]
        << ")";
    usb_to_native[entry->usb_keycode] = entry->native_keycode;
    EXPECT_EQ(0U, native_to_usb.count(entry->native_keycode))
        << " duplicate of native code 0x" << std::hex << std::setfill('0')
        << std::setw(4) << entry->native_keycode
        << " to USB 0x"
        << std::setw(6) << entry->usb_keycode
        << " (previous was 0x"
        << std::setw(6) << native_to_usb[entry->native_keycode]
        << ")";
    native_to_usb[entry->native_keycode] = entry->usb_keycode;
  }
  ASSERT_EQ(usb_to_native.size(), native_to_usb.size());

  // Verify that the number of mapped keys is what we expect, i.e. we haven't
  // lost any, and if we've added some then the expectation has been updated.
  EXPECT_EQ(kExpectedMappedKeyCount, usb_to_native.size());
}

TEST(UsbKeycodeMap, NonExistent) {
  // Verify that UsbKeycodeToNativeKeycode works for a non-existent USB keycode.
  EXPECT_EQ(
      ui::KeycodeConverter::InvalidNativeKeycode(),
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbNonExistentKeycode));
}

TEST(UsbKeycodeMap, UsBackslashIsNonUsHash) {
  // Verify that UsbKeycodeToNativeKeycode treats the non-US "hash" key
  // as equivalent to the US "backslash" key.
  EXPECT_EQ(ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbUsBackslash),
            ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbNonUsHash));
}

TEST(KeycodeConverter, DomCode) {
  // Test invalid and unknown arguments to CodeStringToDomCode()
  EXPECT_EQ(ui::DomCode::NONE,
            ui::KeycodeConverter::CodeStringToDomCode(nullptr));
  EXPECT_EQ(ui::DomCode::NONE, ui::KeycodeConverter::CodeStringToDomCode("-"));
  EXPECT_EQ(ui::DomCode::NONE, ui::KeycodeConverter::CodeStringToDomCode(""));
  // Round-trip test DOM Level 3 .code strings.
  const ui::KeycodeMapEntry* keycode_map =
      ui::KeycodeConverter::GetKeycodeMapForTest();
  size_t numEntries = ui::KeycodeConverter::NumKeycodeMapEntriesForTest();
  for (size_t i = 0; i < numEntries; ++i) {
    SCOPED_TRACE(i);
    const ui::KeycodeMapEntry* entry = &keycode_map[i];
    ui::DomCode code = ui::KeycodeConverter::CodeStringToDomCode(entry->code);
    if (entry->code) {
      EXPECT_STREQ(entry->code,
                   ui::KeycodeConverter::DomCodeToCodeString(code));
    } else {
      EXPECT_EQ(static_cast<int>(ui::DomCode::NONE),
                static_cast<int>(code));
    }
  }
}

TEST(KeycodeConverter, DomKey) {
  // Test invalid and unknown arguments to KeyStringToDomKey()
  EXPECT_EQ(ui::DomKey::NONE, ui::KeycodeConverter::KeyStringToDomKey(nullptr));
  EXPECT_EQ(ui::DomKey::NONE, ui::KeycodeConverter::KeyStringToDomKey("-"));
  // Round-trip test DOM Level 3 .key strings.
  const char* s = nullptr;
  for (size_t i = 0;
       (s = ui::KeycodeConverter::DomKeyStringForTest(i)) != nullptr;
       ++i) {
    SCOPED_TRACE(i);
    ui::DomKey key = ui::KeycodeConverter::KeyStringToDomKey(s);
    if (s) {
      EXPECT_STREQ(s, ui::KeycodeConverter::DomKeyToKeyString(key));
    } else {
      EXPECT_EQ(ui::DomKey::NONE, key);
    }
  }
}

}  // namespace
