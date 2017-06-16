// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_child_frame.h"

#include "base/command_line.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"

namespace content {

RenderWidgetHostViewChildFrame::RenderWidgetHostViewChildFrame(
    RenderWidgetHost* widget_host)
    : host_(RenderWidgetHostImpl::From(widget_host)),
      use_surfaces_(UseSurfacesEnabled()),
      next_surface_sequence_(1u),
      last_output_surface_id_(0),
      current_surface_scale_factor_(1.f),
      ack_pending_count_(0),
      frame_connector_(nullptr),
      weak_factory_(this) {
  if (use_surfaces_)
    id_allocator_ = CreateSurfaceIdAllocator();

  host_->SetView(this);
}

RenderWidgetHostViewChildFrame::~RenderWidgetHostViewChildFrame() {
  if (!surface_id_.is_null())
    surface_factory_->Destroy(surface_id_);
}

void RenderWidgetHostViewChildFrame::InitAsChild(
    gfx::NativeView parent_view) {
  NOTREACHED();
}

RenderWidgetHost* RenderWidgetHostViewChildFrame::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewChildFrame::SetSize(const gfx::Size& size) {
  host_->WasResized();
}

void RenderWidgetHostViewChildFrame::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewChildFrame::Focus() {
}

bool RenderWidgetHostViewChildFrame::HasFocus() const {
  return false;
}

bool RenderWidgetHostViewChildFrame::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewChildFrame::Show() {
  if (!host_->is_hidden())
    return;
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewChildFrame::Hide() {
  if (host_->is_hidden())
    return;
  host_->WasHidden();
}

bool RenderWidgetHostViewChildFrame::IsShowing() {
  return !host_->is_hidden();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetViewBounds() const {
  gfx::Rect rect;
  if (frame_connector_)
    rect = frame_connector_->ChildFrameRect();
  return rect;
}

gfx::Vector2dF RenderWidgetHostViewChildFrame::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewChildFrame::GetNativeView() const {
  NOTREACHED();
  return NULL;
}

gfx::NativeViewId RenderWidgetHostViewChildFrame::GetNativeViewId() const {
  NOTREACHED();
  return 0;
}

gfx::NativeViewAccessible
RenderWidgetHostViewChildFrame::GetNativeViewAccessible() {
  NOTREACHED();
  return NULL;
}

void RenderWidgetHostViewChildFrame::SetBackgroundColor(SkColor color) {
}

gfx::Size RenderWidgetHostViewChildFrame::GetPhysicalBackingSize() const {
  gfx::Size size;
  if (frame_connector_)
    size = frame_connector_->ChildFrameRect().size();
  return size;
}

void RenderWidgetHostViewChildFrame::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::ImeCancelComposition() {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
}

void RenderWidgetHostViewChildFrame::UpdateCursor(const WebCursor& cursor) {
}

void RenderWidgetHostViewChildFrame::SetIsLoading(bool is_loading) {
  // It is valid for an inner WebContents's SetIsLoading() to end up here.
  // This is because an inner WebContents's main frame's RenderWidgetHostView
  // is a RenderWidgetHostViewChildFrame. In contrast, when there is no
  // inner/outer WebContents, only subframe's RenderWidgetHostView can be a
  // RenderWidgetHostViewChildFrame which do not get a SetIsLoading() call.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess) &&
      BrowserPluginGuest::IsGuest(
          static_cast<RenderViewHostImpl*>(RenderViewHost::From(host_)))) {
    return;
  }

  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline,
    int flags) {
  // TODO(kenrb): Implement.
}

void RenderWidgetHostViewChildFrame::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  if (frame_connector_)
    frame_connector_->RenderProcessGone();
  Destroy();
}

void RenderWidgetHostViewChildFrame::Destroy() {
  if (frame_connector_) {
    frame_connector_->set_view(NULL);
    frame_connector_ = NULL;
  }

  host_->SetView(NULL);
  host_ = NULL;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewChildFrame::SetTooltipText(
    const base::string16& tooltip_text) {
}

void RenderWidgetHostViewChildFrame::SelectionChanged(
    const base::string16& text,
    size_t offset,
    const gfx::Range& range) {
}

void RenderWidgetHostViewChildFrame::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
}

#if defined(OS_ANDROID)
void RenderWidgetHostViewChildFrame::LockCompositingSurface() {
}

void RenderWidgetHostViewChildFrame::UnlockCompositingSurface() {
}
#endif

void RenderWidgetHostViewChildFrame::SurfaceDrawn(uint32 output_surface_id,
                                                  cc::SurfaceDrawStatus drawn) {
  cc::CompositorFrameAck ack;
  DCHECK(ack_pending_count_ > 0);
  if (!surface_returned_resources_.empty())
    ack.resources.swap(surface_returned_resources_);
  if (host_) {
    host_->Send(new ViewMsg_SwapCompositorFrameAck(host_->GetRoutingID(),
                                                   output_surface_id, ack));
  }
  ack_pending_count_--;
}

void RenderWidgetHostViewChildFrame::OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) {
  last_scroll_offset_ = frame->metadata.root_scroll_offset;

  if (!frame_connector_)
    return;

  // When not using surfaces, the frame just gets proxied to
  // the embedder's renderer to be composited.
  if (!frame->delegated_frame_data || !use_surfaces_) {
    frame_connector_->ChildFrameCompositorFrameSwapped(
        output_surface_id,
        host_->GetProcess()->GetID(),
        host_->GetRoutingID(),
        frame.Pass());
    return;
  }

  cc::RenderPass* root_pass =
      frame->delegated_frame_data->render_pass_list.back();

  gfx::Size frame_size = root_pass->output_rect.size();
  float scale_factor = frame->metadata.device_scale_factor;

  // Check whether we need to recreate the cc::Surface, which means the child
  // frame renderer has changed its output surface, or size, or scale factor.
  if (output_surface_id != last_output_surface_id_ && surface_factory_) {
    surface_factory_->Destroy(surface_id_);
    surface_factory_.reset();
  }
  if (output_surface_id != last_output_surface_id_ ||
      frame_size != current_surface_size_ ||
      scale_factor != current_surface_scale_factor_) {
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
    frame_connector_->SetChildFrameSurface(surface_id_, frame_size,
                                           scale_factor, sequence);
  }

  cc::SurfaceFactory::DrawCallback ack_callback =
      base::Bind(&RenderWidgetHostViewChildFrame::SurfaceDrawn, AsWeakPtr(),
                 output_surface_id);
  ack_pending_count_++;
  // If this value grows very large, something is going wrong.
  DCHECK(ack_pending_count_ < 1000);
  surface_factory_->SubmitFrame(surface_id_, frame.Pass(), ack_callback);
}

void RenderWidgetHostViewChildFrame::GetScreenInfo(
    blink::WebScreenInfo* results) {
  if (!frame_connector_)
    return;
  frame_connector_->GetScreenInfo(results);
}

gfx::Rect RenderWidgetHostViewChildFrame::GetBoundsInRootWindow() {
  // We do not have any root window specific parts in this view.
  return GetViewBounds();
}

#if defined(USE_AURA)
void RenderWidgetHostViewChildFrame::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
}
#endif  // defined(USE_AURA)

bool RenderWidgetHostViewChildFrame::LockMouse() {
  return false;
}

void RenderWidgetHostViewChildFrame::UnlockMouse() {
}

uint32_t RenderWidgetHostViewChildFrame::GetSurfaceIdNamespace() {
  if (!use_surfaces_)
    return 0;

  return id_allocator_->id_namespace();
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewChildFrame::SetActive(bool active) {
}

void RenderWidgetHostViewChildFrame::SetWindowVisibility(bool visible) {
}

void RenderWidgetHostViewChildFrame::WindowFrameChanged() {
}

void RenderWidgetHostViewChildFrame::ShowDefinitionForSelection() {
}

bool RenderWidgetHostViewChildFrame::SupportsSpeech() const {
  return false;
}

void RenderWidgetHostViewChildFrame::SpeakSelection() {
}

bool RenderWidgetHostViewChildFrame::IsSpeaking() const {
  return false;
}

void RenderWidgetHostViewChildFrame::StopSpeaking() {
}

bool RenderWidgetHostViewChildFrame::PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) {
  return false;
}
#endif // defined(OS_MACOSX)

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& /* dst_size */,
    ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  callback.Run(SkBitmap(), READBACK_FAILED);
}

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool RenderWidgetHostViewChildFrame::CanCopyToVideoFrame() const {
  return false;
}

bool RenderWidgetHostViewChildFrame::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

gfx::GLSurfaceHandle RenderWidgetHostViewChildFrame::GetCompositingSurface() {
  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NULL_TRANSPORT);
}

#if defined(OS_WIN)
void RenderWidgetHostViewChildFrame::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId RenderWidgetHostViewChildFrame::GetParentForWindowlessPlugin()
    const {
  return NULL;
}
#endif // defined(OS_WIN)

// cc::SurfaceFactoryClient implementation.
void RenderWidgetHostViewChildFrame::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;

  if (!ack_pending_count_ && host_) {
    cc::CompositorFrameAck ack;
    std::copy(resources.begin(), resources.end(),
              std::back_inserter(ack.resources));
    host_->Send(new ViewMsg_ReclaimCompositorResources(
        host_->GetRoutingID(), last_output_surface_id_, ack));
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

BrowserAccessibilityManager*
RenderWidgetHostViewChildFrame::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate) {
  return BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
}

void RenderWidgetHostViewChildFrame::ClearCompositorSurfaceIfNecessary() {
  if (surface_factory_ && !surface_id_.is_null())
    surface_factory_->Destroy(surface_id_);
  surface_id_ = cc::SurfaceId();
}

}  // namespace content
