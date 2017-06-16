// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/cross_process_frame_connector.h"

#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

CrossProcessFrameConnector::CrossProcessFrameConnector(
    RenderFrameProxyHost* frame_proxy_in_parent_renderer)
    : frame_proxy_in_parent_renderer_(frame_proxy_in_parent_renderer),
      view_(NULL),
      device_scale_factor_(1) {
}

CrossProcessFrameConnector::~CrossProcessFrameConnector() {
  if (view_)
    view_->set_cross_process_frame_connector(NULL);
}

bool CrossProcessFrameConnector::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(CrossProcessFrameConnector, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CompositorFrameSwappedACK,
                        OnCompositorFrameSwappedACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ReclaimCompositorResources,
                        OnReclaimCompositorResources)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ForwardInputEvent, OnForwardInputEvent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_InitializeChildFrame,
                        OnInitializeChildFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SatisfySequence, OnSatisfySequence)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RequireSequence, OnRequireSequence)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void CrossProcessFrameConnector::set_view(
    RenderWidgetHostViewChildFrame* view) {
  // Detach ourselves from the previous |view_|.
  if (view_)
    view_->set_cross_process_frame_connector(NULL);

  view_ = view;

  // Attach ourselves to the new view and size it appropriately.
  if (view_) {
    view_->set_cross_process_frame_connector(this);
    SetDeviceScaleFactor(device_scale_factor_);
    SetSize(child_frame_rect_);
  }
}

void CrossProcessFrameConnector::RenderProcessGone() {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_ChildFrameProcessGone(
      frame_proxy_in_parent_renderer_->GetRoutingID()));
}

void CrossProcessFrameConnector::ChildFrameCompositorFrameSwapped(
    uint32 output_surface_id,
    int host_id,
    int route_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  FrameMsg_CompositorFrameSwapped_Params params;
  frame->AssignTo(&params.frame);
  params.output_surface_id = output_surface_id;
  params.producing_route_id = route_id;
  params.producing_host_id = host_id;
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_CompositorFrameSwapped(
      frame_proxy_in_parent_renderer_->GetRoutingID(), params));
}

void CrossProcessFrameConnector::SetChildFrameSurface(
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float scale_factor,
    const cc::SurfaceSequence& sequence) {
  frame_proxy_in_parent_renderer_->Send(new FrameMsg_SetChildFrameSurface(
      frame_proxy_in_parent_renderer_->GetRoutingID(), surface_id, frame_size,
      scale_factor, sequence));
}

void CrossProcessFrameConnector::OnSatisfySequence(
    const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  cc::SurfaceManager* manager = GetSurfaceManager();
  manager->DidSatisfySequences(sequence.id_namespace, &sequences);
}

void CrossProcessFrameConnector::OnRequireSequence(
    const cc::SurfaceId& id,
    const cc::SurfaceSequence& sequence) {
  cc::SurfaceManager* manager = GetSurfaceManager();
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

void CrossProcessFrameConnector::OnCompositorFrameSwappedACK(
    const FrameHostMsg_CompositorFrameSwappedACK_Params& params) {
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(params.producing_route_id,
                                                   params.output_surface_id,
                                                   params.producing_host_id,
                                                   params.ack);
}

void CrossProcessFrameConnector::OnReclaimCompositorResources(
    const FrameHostMsg_ReclaimCompositorResources_Params& params) {
  RenderWidgetHostImpl::SendReclaimCompositorResources(params.route_id,
                                                       params.output_surface_id,
                                                       params.renderer_host_id,
                                                       params.ack);
}

void CrossProcessFrameConnector::OnInitializeChildFrame(gfx::Rect frame_rect,
                                                        float scale_factor) {
  if (scale_factor != device_scale_factor_)
    SetDeviceScaleFactor(scale_factor);

  if (!frame_rect.size().IsEmpty())
    SetSize(frame_rect);
}

gfx::Rect CrossProcessFrameConnector::ChildFrameRect() {
  return child_frame_rect_;
}

void CrossProcessFrameConnector::GetScreenInfo(blink::WebScreenInfo* results) {
  // Inner WebContents's root FrameTreeNode does not have a parent(), so
  // GetRenderWidgetHostView() call below will fail.
  // TODO(lazyboy): Fix this.
  if (frame_proxy_in_parent_renderer_->frame_tree_node()
          ->render_manager()
          ->ForInnerDelegate()) {
    return;
  }

  RenderWidgetHostView* rwhv =
      frame_proxy_in_parent_renderer_->GetRenderWidgetHostView();
  if (rwhv)
    static_cast<RenderWidgetHostViewBase*>(rwhv)->GetScreenInfo(results);
}

void CrossProcessFrameConnector::OnForwardInputEvent(
    const blink::WebInputEvent* event) {
  if (!view_)
    return;

  RenderWidgetHostImpl* child_widget =
      RenderWidgetHostImpl::From(view_->GetRenderWidgetHost());
  RenderFrameHostManager* manager =
      frame_proxy_in_parent_renderer_->frame_tree_node()->render_manager();
  RenderWidgetHostImpl* parent_widget =
      manager->ForInnerDelegate()
          ? manager->GetOuterRenderWidgetHostForKeyboardInput()
          : frame_proxy_in_parent_renderer_->GetRenderViewHost();

  if (blink::WebInputEvent::isKeyboardEventType(event->type)) {
    if (!parent_widget->GetLastKeyboardEvent())
      return;
    NativeWebKeyboardEvent keyboard_event(
        *parent_widget->GetLastKeyboardEvent());
    child_widget->ForwardKeyboardEvent(keyboard_event);
    return;
  }

  if (blink::WebInputEvent::isMouseEventType(event->type)) {
    child_widget->ForwardMouseEvent(
        *static_cast<const blink::WebMouseEvent*>(event));
    return;
  }

  if (event->type == blink::WebInputEvent::MouseWheel) {
    child_widget->ForwardWheelEvent(
        *static_cast<const blink::WebMouseWheelEvent*>(event));
    return;
  }
}

void CrossProcessFrameConnector::SetDeviceScaleFactor(float scale_factor) {
  device_scale_factor_ = scale_factor;
  // The RenderWidgetHost is null in unit tests.
  if (view_ && view_->GetRenderWidgetHost()) {
    RenderWidgetHostImpl* child_widget =
        RenderWidgetHostImpl::From(view_->GetRenderWidgetHost());
    child_widget->NotifyScreenInfoChanged();
  }
}

void CrossProcessFrameConnector::SetSize(gfx::Rect frame_rect) {
  child_frame_rect_ = frame_rect;
  if (view_)
    view_->SetSize(frame_rect.size());
}

}  // namespace content
