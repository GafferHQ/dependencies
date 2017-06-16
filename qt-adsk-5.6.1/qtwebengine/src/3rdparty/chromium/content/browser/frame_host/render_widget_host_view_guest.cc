// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/common/view_messages.h"
#include "content/common/webplugin_geometry.h"
#include "content/public/common/content_switches.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"

#if defined(OS_MACOSX)
#import "content/browser/renderer_host/render_widget_host_view_mac_dictionary_helper.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#endif

namespace content {

namespace {

#if defined(USE_AURA)
blink::WebGestureEvent CreateFlingCancelEvent(double time_stamp) {
  blink::WebGestureEvent gesture_event;
  gesture_event.timeStampSeconds = time_stamp;
  gesture_event.type = blink::WebGestureEvent::GestureFlingCancel;
  gesture_event.sourceDevice = blink::WebGestureDeviceTouchscreen;
  return gesture_event;
}
#endif  // defined(USE_AURA)

}  // namespace

RenderWidgetHostViewGuest::RenderWidgetHostViewGuest(
    RenderWidgetHost* widget_host,
    BrowserPluginGuest* guest,
    base::WeakPtr<RenderWidgetHostViewBase> platform_view)
    : RenderWidgetHostViewChildFrame(widget_host),
      // |guest| is NULL during test.
      guest_(guest ? guest->AsWeakPtr() : base::WeakPtr<BrowserPluginGuest>()),
      platform_view_(platform_view) {
#if defined(USE_AURA)
  gesture_recognizer_.reset(ui::GestureRecognizer::Create());
  gesture_recognizer_->AddGestureEventHelper(this);
#endif  // defined(USE_AURA)
}

RenderWidgetHostViewGuest::~RenderWidgetHostViewGuest() {
#if defined(USE_AURA)
  gesture_recognizer_->RemoveGestureEventHelper(this);
#endif  // defined(USE_AURA)
}

bool RenderWidgetHostViewGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message,
    RenderWidgetHostImpl* embedder) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(RenderWidgetHostViewGuest, message,
                                   embedder)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_HandleInputEvent,
                        OnHandleInputEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewGuest::Show() {
  // If the WebContents associated with us showed an interstitial page in the
  // beginning, the teardown path might call WasShown() while |host_| is in
  // the process of destruction. Avoid calling WasShown below in this case.
  // TODO(lazyboy): We shouldn't be showing interstitial pages in guests in the
  // first place: http://crbug.com/273089.
  //
  // |guest_| is NULL during test.
  if ((guest_ && guest_->is_in_destruction()) || !host_->is_hidden())
    return;
  // Make sure the size of this view matches the size of the WebContentsView.
  // The two sizes may fall out of sync if we switch RenderWidgetHostViews,
  // resize, and then switch page, as is the case with interstitial pages.
  // NOTE: |guest_| is NULL in unit tests.
  if (guest_)
    SetSize(guest_->web_contents()->GetViewBounds().size());
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewGuest::Hide() {
  // |guest_| is NULL during test.
  if ((guest_ && guest_->is_in_destruction()) || host_->is_hidden())
    return;
  host_->WasHidden();
}

void RenderWidgetHostViewGuest::SetSize(const gfx::Size& size) {
  size_ = size;
  host_->WasResized();
}

void RenderWidgetHostViewGuest::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewGuest::Focus() {
  // InterstitialPageImpl focuses views directly, so we place focus logic here.
  // InterstitialPages are not WebContents, and so BrowserPluginGuest does not
  // have direct access to the interstitial page's RenderWidgetHost.
  if (guest_)
    guest_->SetFocus(host_, true, blink::WebFocusTypeNone);
}

bool RenderWidgetHostViewGuest::HasFocus() const {
  if (!guest_)
    return false;
  return guest_->focused();
}

#if defined(USE_AURA)
void RenderWidgetHostViewGuest::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch, InputEventAckState ack_result) {
  // TODO(fsamuel): Currently we will only take this codepath if the guest has
  // requested touch events. A better solution is to always forward touchpresses
  // to the embedder process to target a BrowserPlugin, and then route all
  // subsequent touch points of that touchdown to the appropriate guest until
  // that touch point is released.
  ScopedVector<ui::TouchEvent> events;
  if (!MakeUITouchEventsFromWebTouchEvents(touch, &events, LOCAL_COORDINATES))
    return;

  ui::EventResult result = (ack_result ==
      INPUT_EVENT_ACK_STATE_CONSUMED) ? ui::ER_HANDLED : ui::ER_UNHANDLED;
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter)  {
    if (!gesture_recognizer_->ProcessTouchEventPreDispatch(*iter, this))
      continue;

    scoped_ptr<ui::GestureRecognizer::Gestures> gestures;
    gestures.reset(gesture_recognizer_->AckTouchEvent(
        (*iter)->unique_event_id(), result, this));
    ProcessGestures(gestures.get());
  }
}
#endif

gfx::Rect RenderWidgetHostViewGuest::GetViewBounds() const {
  if (!guest_)
    return gfx::Rect();

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  gfx::Rect embedder_bounds;
  if (rwhv)
    embedder_bounds = rwhv->GetViewBounds();
  return gfx::Rect(
      guest_->GetScreenCoordinates(embedder_bounds.origin()), size_);
}

void RenderWidgetHostViewGuest::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  // The |platform_view_| gets destroyed before we get here if this view
  // is for an InterstitialPage.
  if (platform_view_)
    platform_view_->RenderProcessGone(status, error_code);

  // Destroy the guest view instance only, so we don't end up calling
  // platform_view_->Destroy().
  DestroyGuestView();
}

void RenderWidgetHostViewGuest::Destroy() {
  // The RenderWidgetHost's destruction led here, so don't call it.
  DestroyGuestView();

  if (platform_view_)  // The platform view might have been destroyed already.
    platform_view_->Destroy();
}

gfx::Size RenderWidgetHostViewGuest::GetPhysicalBackingSize() const {
  return RenderWidgetHostViewBase::GetPhysicalBackingSize();
}

base::string16 RenderWidgetHostViewGuest::GetSelectedText() const {
  return platform_view_->GetSelectedText();
}

void RenderWidgetHostViewGuest::SetTooltipText(
    const base::string16& tooltip_text) {
  if (guest_)
    guest_->SetTooltipText(tooltip_text);
}

void RenderWidgetHostViewGuest::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  if (!guest_ || !guest_->attached()) {
    // We shouldn't hang on to a surface while we are detached.
    ClearCompositorSurfaceIfNecessary();
    return;
  }

  last_scroll_offset_ = frame->metadata.root_scroll_offset;
  // When not using surfaces, the frame just gets proxied to
  // the embedder's renderer to be composited.
  if (!frame->delegated_frame_data || !use_surfaces_) {
    guest_->SwapCompositorFrame(output_surface_id,
                                host_->GetProcess()->GetID(),
                                host_->GetRoutingID(),
                                frame.Pass());
    return;
  }

  cc::RenderPass* root_pass =
      frame->delegated_frame_data->render_pass_list.back();

  gfx::Size frame_size = root_pass->output_rect.size();
  float scale_factor = frame->metadata.device_scale_factor;

  guest_->UpdateGuestSizeIfNecessary(frame_size, scale_factor);

  // Check whether we need to recreate the cc::Surface, which means the child
  // frame renderer has changed its output surface, or size, or scale factor.
  if (output_surface_id != last_output_surface_id_ && surface_factory_) {
    surface_factory_->Destroy(surface_id_);
    surface_factory_.reset();
  }
  if (output_surface_id != last_output_surface_id_ ||
      frame_size != current_surface_size_ ||
      scale_factor != current_surface_scale_factor_ ||
      guest_->has_attached_since_surface_set()) {
    ClearCompositorSurfaceIfNecessary();
    last_output_surface_id_ = output_surface_id;
    current_surface_size_ = frame_size;
    current_surface_scale_factor_ = scale_factor;
  }

  if (!surface_factory_) {
    cc::SurfaceManager* manager = GetSurfaceManager();
    surface_factory_ = make_scoped_ptr(new cc::SurfaceFactory(manager, this));
  }

  if (surface_id_.is_null()) {
    surface_id_ = id_allocator_->GenerateId();
    surface_factory_->Create(surface_id_);

    cc::SurfaceSequence sequence = cc::SurfaceSequence(
        id_allocator_->id_namespace(), next_surface_sequence_++);
    // The renderer process will satisfy this dependency when it creates a
    // SurfaceLayer.
    cc::SurfaceManager* manager = GetSurfaceManager();
    manager->GetSurfaceForId(surface_id_)->AddDestructionDependency(sequence);
    guest_->SetChildFrameSurface(surface_id_, frame_size, scale_factor,
                                 sequence);
  }

  cc::SurfaceFactory::DrawCallback ack_callback = base::Bind(
      &RenderWidgetHostViewChildFrame::SurfaceDrawn,
      RenderWidgetHostViewChildFrame::AsWeakPtr(), output_surface_id);
  ack_pending_count_++;
  // If this value grows very large, something is going wrong.
  DCHECK(ack_pending_count_ < 1000);
  surface_factory_->SubmitFrame(surface_id_, frame.Pass(), ack_callback);
}

bool RenderWidgetHostViewGuest::OnMessageReceived(const IPC::Message& msg) {
  if (!platform_view_) {
    // In theory, we can get here if there's a delay between DestroyGuestView()
    // being called and when our destructor is invoked.
    return false;
  }

  return platform_view_->OnMessageReceived(msg);
}

void RenderWidgetHostViewGuest::InitAsChild(
    gfx::NativeView parent_view) {
  platform_view_->InitAsChild(parent_view);
}

void RenderWidgetHostViewGuest::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& bounds) {
  // This should never get called.
  NOTREACHED();
}

void RenderWidgetHostViewGuest::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  // This should never get called.
  NOTREACHED();
}

gfx::NativeView RenderWidgetHostViewGuest::GetNativeView() const {
  if (!guest_)
    return gfx::NativeView();

  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeView();
  return rwhv->GetNativeView();
}

gfx::NativeViewId RenderWidgetHostViewGuest::GetNativeViewId() const {
  if (!guest_)
    return static_cast<gfx::NativeViewId>(NULL);

  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return static_cast<gfx::NativeViewId>(NULL);
  return rwhv->GetNativeViewId();
}

gfx::NativeViewAccessible RenderWidgetHostViewGuest::GetNativeViewAccessible() {
  if (!guest_)
    return gfx::NativeViewAccessible();

  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return gfx::NativeViewAccessible();
  return rwhv->GetNativeViewAccessible();
}

void RenderWidgetHostViewGuest::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
  platform_view_->MovePluginWindows(moves);
}

void RenderWidgetHostViewGuest::UpdateCursor(const WebCursor& cursor) {
  // InterstitialPages are not WebContents so we cannot intercept
  // ViewHostMsg_SetCursor for interstitial pages in BrowserPluginGuest.
  // All guest RenderViewHosts have RenderWidgetHostViewGuests however,
  // and so we will always hit this code path.
  if (!guest_)
    return;
  guest_->SendMessageToEmbedder(
      new BrowserPluginMsg_SetCursor(guest_->browser_plugin_instance_id(),
                                     cursor));

}

void RenderWidgetHostViewGuest::SetIsLoading(bool is_loading) {
  platform_view_->SetIsLoading(is_loading);
}

void RenderWidgetHostViewGuest::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline,
    int flags) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  // Forward the information to embedding RWHV.
  rwhv->TextInputTypeChanged(type, input_mode, can_compose_inline, flags);
}

void RenderWidgetHostViewGuest::ImeCancelComposition() {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  // Forward the information to embedding RWHV.
  rwhv->ImeCancelComposition();
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void RenderWidgetHostViewGuest::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  std::vector<gfx::Rect> guest_character_bounds;
  for (size_t i = 0; i < character_bounds.size(); ++i) {
    guest_character_bounds.push_back(gfx::Rect(
        guest_->GetScreenCoordinates(character_bounds[i].origin()),
        character_bounds[i].size()));
  }
  // Forward the information to embedding RWHV.
  rwhv->ImeCompositionRangeChanged(range, guest_character_bounds);
}
#endif

void RenderWidgetHostViewGuest::SelectionChanged(const base::string16& text,
                                                 size_t offset,
                                                 const gfx::Range& range) {
  platform_view_->SelectionChanged(text, offset, range);
}

void RenderWidgetHostViewGuest::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (!guest_)
    return;

  RenderWidgetHostViewBase* rwhv = GetOwnerRenderWidgetHostView();
  if (!rwhv)
    return;
  ViewHostMsg_SelectionBounds_Params guest_params(params);
  guest_params.anchor_rect.set_origin(
      guest_->GetScreenCoordinates(params.anchor_rect.origin()));
  guest_params.focus_rect.set_origin(
      guest_->GetScreenCoordinates(params.focus_rect.origin()));
  rwhv->SelectionBoundsChanged(guest_params);
}

void RenderWidgetHostViewGuest::SetBackgroundColor(SkColor color) {
  // Content embedders can toggle opaque backgrounds through this API.
  // We plumb the value here so that BrowserPlugin updates its compositing
  // state in response to this change. We also want to preserve this flag
  // after recovering from a crash so we let BrowserPluginGuest store it.
  if (!guest_)
    return;
  RenderWidgetHostViewBase::SetBackgroundColor(color);
  bool opaque = GetBackgroundOpaque();
  host_->SetBackgroundOpaque(opaque);
  guest_->SetContentsOpaque(opaque);
}

bool RenderWidgetHostViewGuest::LockMouse() {
  return platform_view_->LockMouse();
}

void RenderWidgetHostViewGuest::UnlockMouse() {
  return platform_view_->UnlockMouse();
}

void RenderWidgetHostViewGuest::GetScreenInfo(blink::WebScreenInfo* results) {
  if (!guest_)
    return;
  RenderWidgetHostViewBase* embedder_view = GetOwnerRenderWidgetHostView();
  if (embedder_view)
    embedder_view->GetScreenInfo(results);
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewGuest::SetActive(bool active) {
  platform_view_->SetActive(active);
}

void RenderWidgetHostViewGuest::SetWindowVisibility(bool visible) {
  platform_view_->SetWindowVisibility(visible);
}

void RenderWidgetHostViewGuest::WindowFrameChanged() {
  platform_view_->WindowFrameChanged();
}

void RenderWidgetHostViewGuest::ShowDefinitionForSelection() {
  if (!guest_)
    return;

  gfx::Point origin;
  gfx::Rect guest_bounds = GetViewBounds();
  RenderWidgetHostView* rwhv = guest_->GetOwnerRenderWidgetHostView();
  gfx::Rect embedder_bounds;
  if (rwhv)
    embedder_bounds = rwhv->GetViewBounds();

  gfx::Vector2d guest_offset = gfx::Vector2d(
      // Horizontal offset of guest from embedder.
      guest_bounds.x() - embedder_bounds.x(),
      // Vertical offset from guest's top to embedder's bottom edge.
      embedder_bounds.bottom() - guest_bounds.y());

  RenderWidgetHostViewMacDictionaryHelper helper(platform_view_.get());
  helper.SetTargetView(rwhv);
  helper.set_offset(guest_offset);
  helper.ShowDefinitionForSelection();
}

bool RenderWidgetHostViewGuest::SupportsSpeech() const {
  return platform_view_->SupportsSpeech();
}

void RenderWidgetHostViewGuest::SpeakSelection() {
  platform_view_->SpeakSelection();
}

bool RenderWidgetHostViewGuest::IsSpeaking() const {
  return platform_view_->IsSpeaking();
}

void RenderWidgetHostViewGuest::StopSpeaking() {
  platform_view_->StopSpeaking();
}

bool RenderWidgetHostViewGuest::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  return false;
}

#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(USE_AURA)
void RenderWidgetHostViewGuest::ShowDisambiguationPopup(
    const gfx::Rect& rect_pixels,
    const SkBitmap& zoomed_bitmap) {
}
#endif  // defined(OS_ANDROID) || defined(USE_AURA)

#if defined(OS_ANDROID)
void RenderWidgetHostViewGuest::LockCompositingSurface() {
}

void RenderWidgetHostViewGuest::UnlockCompositingSurface() {
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
void RenderWidgetHostViewGuest::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId RenderWidgetHostViewGuest::GetParentForWindowlessPlugin()
    const {
  return NULL;
}
#endif

void RenderWidgetHostViewGuest::DestroyGuestView() {
  host_->SetView(NULL);
  host_ = NULL;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

bool RenderWidgetHostViewGuest::CanDispatchToConsumer(
    ui::GestureConsumer* consumer) {
  CHECK_EQ(static_cast<RenderWidgetHostViewGuest*>(consumer), this);
  return true;
}

void RenderWidgetHostViewGuest::DispatchGestureEvent(
    ui::GestureEvent* event) {
  ForwardGestureEventToRenderer(event);
}

void RenderWidgetHostViewGuest::DispatchCancelTouchEvent(
    ui::TouchEvent* event) {
  if (!host_)
    return;

  blink::WebTouchEvent cancel_event;
  // TODO(rbyers): This event has no touches in it.  Don't we need to know what
  // touches are currently active in order to cancel them all properly?
  WebTouchEventTraits::ResetType(blink::WebInputEvent::TouchCancel,
                                 event->time_stamp().InSecondsF(),
                                 &cancel_event);

  host_->ForwardTouchEventWithLatencyInfo(cancel_event, *event->latency());
}

bool RenderWidgetHostViewGuest::ForwardGestureEventToRenderer(
    ui::GestureEvent* gesture) {
#if defined(USE_AURA)
  if (!host_)
    return false;

  if ((gesture->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      gesture->type() == ui::ET_GESTURE_PINCH_UPDATE ||
      gesture->type() == ui::ET_GESTURE_PINCH_END) && !pinch_zoom_enabled_) {
    return true;
  }

  blink::WebGestureEvent web_gesture =
      MakeWebGestureEventFromUIEvent(*gesture);
  const gfx::Point& client_point = gesture->location();
  const gfx::Point& screen_point = gesture->location();

  web_gesture.x = client_point.x();
  web_gesture.y = client_point.y();
  web_gesture.globalX = screen_point.x();
  web_gesture.globalY = screen_point.y();

  if (web_gesture.type == blink::WebGestureEvent::Undefined)
    return false;
  if (web_gesture.type == blink::WebGestureEvent::GestureTapDown) {
    host_->ForwardGestureEvent(
        CreateFlingCancelEvent(gesture->time_stamp().InSecondsF()));
  }
  host_->ForwardGestureEvent(web_gesture);
  return true;
#else
  return false;
#endif
}

void RenderWidgetHostViewGuest::ProcessGestures(
    ui::GestureRecognizer::Gestures* gestures) {
  if ((gestures == NULL) || gestures->empty())
    return;
  for (ui::GestureRecognizer::Gestures::iterator g_it = gestures->begin();
      g_it != gestures->end();
      ++g_it) {
    ForwardGestureEventToRenderer(*g_it);
  }
}

RenderWidgetHostViewBase*
RenderWidgetHostViewGuest::GetOwnerRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewBase*>(
      guest_->GetOwnerRenderWidgetHostView());
}

void RenderWidgetHostViewGuest::OnHandleInputEvent(
    RenderWidgetHostImpl* embedder,
    int browser_plugin_instance_id,
    const gfx::Rect& guest_window_rect,
    const blink::WebInputEvent* event) {
  if (blink::WebInputEvent::isMouseEventType(event->type)) {
    host_->ForwardMouseEvent(
        *static_cast<const blink::WebMouseEvent*>(event));
    return;
  }

  if (event->type == blink::WebInputEvent::MouseWheel) {
    host_->ForwardWheelEvent(
        *static_cast<const blink::WebMouseWheelEvent*>(event));
    return;
  }

  if (blink::WebInputEvent::isKeyboardEventType(event->type)) {
    if (!embedder->GetLastKeyboardEvent())
      return;
    NativeWebKeyboardEvent keyboard_event(*embedder->GetLastKeyboardEvent());
    host_->ForwardKeyboardEvent(keyboard_event);
    return;
  }

  if (blink::WebInputEvent::isTouchEventType(event->type)) {
    if (event->type == blink::WebInputEvent::TouchStart &&
        !embedder->GetView()->HasFocus()) {
      embedder->GetView()->Focus();
    }

    host_->ForwardTouchEventWithLatencyInfo(
        *static_cast<const blink::WebTouchEvent*>(event),
        ui::LatencyInfo());
    return;
  }

  if (blink::WebInputEvent::isGestureEventType(event->type)) {
    host_->ForwardGestureEvent(
        *static_cast<const blink::WebGestureEvent*>(event));
    return;
  }
}

}  // namespace content
