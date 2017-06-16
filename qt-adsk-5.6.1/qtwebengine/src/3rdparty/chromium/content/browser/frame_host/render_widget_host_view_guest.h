// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserPluginGuest;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct NativeWebKeyboardEvent;

// See comments in render_widget_host_view.h about this class and its members.
// This version is for the BrowserPlugin which handles a lot of the
// functionality in a diffent place and isn't platform specific.
// The BrowserPlugin is currently a special case for out-of-process rendered
// content and therefore inherits from RenderWidgetHostViewChildFrame.
// Eventually all RenderWidgetHostViewGuest code will be subsumed by
// RenderWidgetHostViewChildFrame and this class will be removed.
//
// Some elements that are platform specific will be deal with by delegating
// the relevant calls to the platform view.
class CONTENT_EXPORT RenderWidgetHostViewGuest
    : public RenderWidgetHostViewChildFrame,
      public ui::GestureConsumer,
      public ui::GestureEventHelper {
 public:
  RenderWidgetHostViewGuest(
      RenderWidgetHost* widget,
      BrowserPluginGuest* guest,
      base::WeakPtr<RenderWidgetHostViewBase> platform_view);
  ~RenderWidgetHostViewGuest() override;

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message,
                                     RenderWidgetHostImpl* embedder);

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  void Focus() override;
  bool HasFocus() const override;
  void Show() override;
  void Hide() override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewId GetNativeViewId() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  gfx::Rect GetViewBounds() const override;
  void SetBackgroundColor(SkColor color) override;
  gfx::Size GetPhysicalBackingSize() const override;
  base::string16 GetSelectedText() const override;

  // RenderWidgetHostViewBase implementation.
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void MovePluginWindows(const std::vector<WebPluginGeometry>& moves) override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void TextInputTypeChanged(ui::TextInputType type,
                            ui::TextInputMode input_mode,
                            bool can_compose_inline,
                            int flags) override;
  void ImeCancelComposition() override;
#if defined(OS_MACOSX) || defined(USE_AURA)
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
#endif
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  void OnSwapCompositorFrame(uint32 output_surface_id,
                             scoped_ptr<cc::CompositorFrame> frame) override;
#if defined(USE_AURA)
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
#endif
  bool LockMouse() override;
  void UnlockMouse() override;
  void GetScreenInfo(blink::WebScreenInfo* results) override;

#if defined(OS_MACOSX)
  // RenderWidgetHostView implementation.
  void SetActive(bool active) override;
  void SetWindowVisibility(bool visible) override;
  void WindowFrameChanged() override;
  void ShowDefinitionForSelection() override;
  bool SupportsSpeech() const override;
  void SpeakSelection() override;
  bool IsSpeaking() const override;
  void StopSpeaking() override;

  // RenderWidgetHostViewBase implementation.
  bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) override;
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(USE_AURA)
  // RenderWidgetHostViewBase implementation.
  void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                               const SkBitmap& zoomed_bitmap) override;
#endif  // defined(OS_ANDROID) || defined(USE_AURA)

#if defined(OS_ANDROID)
  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) override;
  gfx::NativeViewId GetParentForWindowlessPlugin() const override;
#endif

  // Overridden from ui::GestureEventHelper.
  bool CanDispatchToConsumer(ui::GestureConsumer* consumer) override;
  void DispatchGestureEvent(ui::GestureEvent* event) override;
  void DispatchCancelTouchEvent(ui::TouchEvent* event) override;

 protected:
  friend class RenderWidgetHostView;

 private:
  // Destroys this view without calling |Destroy| on |platform_view_|.
  void DestroyGuestView();

  // Builds and forwards a WebKitGestureEvent to the renderer.
  bool ForwardGestureEventToRenderer(ui::GestureEvent* gesture);

  // Process all of the given gestures (passes them on to renderer)
  void ProcessGestures(ui::GestureRecognizer::Gestures* gestures);

  RenderWidgetHostViewBase* GetOwnerRenderWidgetHostView() const;

  void OnHandleInputEvent(RenderWidgetHostImpl* embedder,
                          int browser_plugin_instance_id,
                          const gfx::Rect& guest_window_rect,
                          const blink::WebInputEvent* event);

  // BrowserPluginGuest and RenderWidgetHostViewGuest's lifetimes are not tied
  // to one another, therefore we access |guest_| through WeakPtr.
  base::WeakPtr<BrowserPluginGuest> guest_;
  gfx::Size size_;
  // The platform view for this RenderWidgetHostView.
  // RenderWidgetHostViewGuest mostly only cares about stuff related to
  // compositing, the rest are directly forwared to this |platform_view_|.
  base::WeakPtr<RenderWidgetHostViewBase> platform_view_;
#if defined(USE_AURA)
  scoped_ptr<ui::GestureRecognizer> gesture_recognizer_;
#endif
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_WIDGET_HOST_VIEW_GUEST_H_
