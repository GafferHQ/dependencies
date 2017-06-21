// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_WIN_H_
#define UI_GFX_SCREEN_WIN_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/display_change_notifier.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {

class GFX_EXPORT ScreenWin : public Screen {
 public:
  ScreenWin();
  ~ScreenWin() override;

 protected:
  // Overridden from gfx::Screen:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView window) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  gfx::Display GetPrimaryDisplay() const override;
  void AddObserver(DisplayObserver* observer) override;
  void RemoveObserver(DisplayObserver* observer) override;

  // Returns the HWND associated with the NativeView.
  virtual HWND GetHWNDFromNativeView(NativeView window) const;

  // Returns the NativeView associated with the HWND.
  virtual NativeWindow GetNativeWindowFromHWND(HWND hwnd) const;

 private:
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Helper implementing the DisplayObserver handling.
  gfx::DisplayChangeNotifier change_notifier_;

  scoped_ptr<SingletonHwndObserver> singleton_hwnd_observer_;

  // Current list of displays.
  std::vector<gfx::Display> displays_;

  DISALLOW_COPY_AND_ASSIGN(ScreenWin);
};

}  // namespace gfx

#endif  // UI_GFX_SCREEN_WIN_H_
