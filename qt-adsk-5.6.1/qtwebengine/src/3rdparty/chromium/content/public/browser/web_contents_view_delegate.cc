// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_view_delegate.h"

#include <stddef.h>

namespace content {

WebContentsViewDelegate::~WebContentsViewDelegate() {
}

gfx::NativeWindow WebContentsViewDelegate::GetNativeWindow() {
  return nullptr;
}

WebDragDestDelegate* WebContentsViewDelegate::GetDragDestDelegate() {
  return nullptr;
}

void WebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params) {
}

void WebContentsViewDelegate::StoreFocus() {
}

void WebContentsViewDelegate::RestoreFocus() {
}

bool WebContentsViewDelegate::Focus() {
  return false;
}

void WebContentsViewDelegate::TakeFocus(bool reverse) {
}

void WebContentsViewDelegate::ShowDisambiguationPopup(
    const gfx::Rect& target_rect,
    const SkBitmap& zoomed_bitmap,
    const gfx::NativeView content,
    const base::Callback<void(ui::GestureEvent*)>& gesture_cb,
    const base::Callback<void(ui::MouseEvent*)>& mouse_cb) {
}

void WebContentsViewDelegate::HideDisambiguationPopup() {
}

void WebContentsViewDelegate::SizeChanged(const gfx::Size& size) {
}

void* WebContentsViewDelegate::CreateRenderWidgetHostViewDelegate(
    RenderWidgetHost* render_widget_host) {
  return nullptr;
}

}  // namespace content
