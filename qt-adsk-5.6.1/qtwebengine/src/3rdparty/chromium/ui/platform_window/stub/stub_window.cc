// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/stub/stub_window.h"

#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

StubWindow::StubWindow(PlatformWindowDelegate* delegate) : delegate_(delegate) {
  delegate_->OnAcceleratedWidgetAvailable(gfx::kNullAcceleratedWidget, 1.f);
}

StubWindow::~StubWindow() {
}

void StubWindow::Show() {
}

void StubWindow::Hide() {
}

void StubWindow::Close() {
  delegate_->OnClosed();
}

void StubWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect StubWindow::GetBounds() {
  return bounds_;
}

void StubWindow::SetCapture() {
}

void StubWindow::ReleaseCapture() {
}

void StubWindow::ToggleFullscreen() {
}

void StubWindow::Maximize() {
}

void StubWindow::Minimize() {
}

void StubWindow::Restore() {
}

void StubWindow::SetCursor(PlatformCursor cursor) {
}

void StubWindow::MoveCursorTo(const gfx::Point& location) {
}

void StubWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
}

}  // namespace ui
