// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_factory.h"

#include "ui/base/ime/mock_input_method.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/input_method_chromeos.h"
#elif defined(OS_WIN)
#include "base/win/metro.h"
#include "ui/base/ime/input_method_win.h"
#include "ui/base/ime/remote_input_method_win.h"
#elif defined(OS_MACOSX)
#include "ui/base/ime/input_method_mac.h"
#elif defined(USE_AURA) && defined(OS_LINUX) && defined(USE_X11) && \
      !defined(OS_CHROMEOS)
#include "ui/base/ime/input_method_auralinux.h"
#else
#include "ui/base/ime/input_method_minimal.h"
#endif

namespace {

ui::InputMethod* g_input_method_for_testing = nullptr;

bool g_input_method_set_for_testing = false;

bool g_create_input_method_called = false;

}  // namespace

namespace ui {

scoped_ptr<InputMethod> CreateInputMethod(
    internal::InputMethodDelegate* delegate,
    gfx::AcceleratedWidget widget) {
  if (!g_create_input_method_called)
    g_create_input_method_called = true;

  if (g_input_method_for_testing) {
    ui::InputMethod* ret = g_input_method_for_testing;
    g_input_method_for_testing = nullptr;
    return make_scoped_ptr(ret);
  }

  if (g_input_method_set_for_testing)
    return make_scoped_ptr(new MockInputMethod(delegate));

#if defined(OS_CHROMEOS)
  return make_scoped_ptr(new InputMethodChromeOS(delegate));
#elif defined(OS_WIN)
  if (IsRemoteInputMethodWinRequired(widget))
    return CreateRemoteInputMethodWin(delegate);
  return make_scoped_ptr(new InputMethodWin(delegate, widget));
#elif defined(OS_MACOSX)
  return make_scoped_ptr(new InputMethodMac(delegate));
#elif defined(USE_AURA) && defined(OS_LINUX) && defined(USE_X11) && \
      !defined(OS_CHROMEOS)
  return make_scoped_ptr(new InputMethodAuraLinux(delegate));
#else
  return make_scoped_ptr(new InputMethodMinimal(delegate));
#endif
}

void SetUpInputMethodFactoryForTesting() {
  if (g_input_method_set_for_testing)
    return;

  CHECK(!g_create_input_method_called)
      << "ui::SetUpInputMethodFactoryForTesting was called after use of "
      << "ui::CreateInputMethod.  You must call "
      << "ui::SetUpInputMethodFactoryForTesting earlier.";

  g_input_method_set_for_testing = true;
}

void SetUpInputMethodForTesting(InputMethod* input_method) {
  g_input_method_for_testing = input_method;
}

}  // namespace ui
