// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host_ui_shim.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_MACOSX)
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

namespace {

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

#if defined(OS_MACOSX)
void OnSurfaceDisplayedCallback(int output_surface_id) {
  content::ImageTransportFactory::GetInstance()->OnSurfaceDisplayed(
      output_surface_id);
}
#endif

base::LazyInstance<IDMap<GpuProcessHostUIShim> > g_hosts_by_id =
    LAZY_INSTANCE_INITIALIZER;

void SendOnIOThreadTask(int host_id, IPC::Message* msg) {
  GpuProcessHost* host = GpuProcessHost::FromID(host_id);
  if (host)
    host->Send(msg);
  else
    delete msg;
}

class ScopedSendOnIOThread {
 public:
  ScopedSendOnIOThread(int host_id, IPC::Message* msg)
      : host_id_(host_id),
        msg_(msg),
        cancelled_(false) {
  }

  ~ScopedSendOnIOThread() {
    if (!cancelled_) {
      BrowserThread::PostTask(BrowserThread::IO,
                              FROM_HERE,
                              base::Bind(&SendOnIOThreadTask,
                                         host_id_,
                                         msg_.release()));
    }
  }

  void Cancel() { cancelled_ = true; }

 private:
  int host_id_;
  scoped_ptr<IPC::Message> msg_;
  bool cancelled_;
};

RenderWidgetHostViewBase* GetRenderWidgetHostViewFromSurfaceID(
    int surface_id) {
  int render_process_id = 0;
  int render_widget_id = 0;
  if (!GpuSurfaceTracker::Get()->GetRenderWidgetIDForSurface(
        surface_id, &render_process_id, &render_widget_id))
    return NULL;

  RenderWidgetHost* host =
      RenderWidgetHost::FromID(render_process_id, render_widget_id);
  return host ? static_cast<RenderWidgetHostViewBase*>(host->GetView()) : NULL;
}

}  // namespace

void RouteToGpuProcessHostUIShimTask(int host_id, const IPC::Message& msg) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(host_id);
  if (ui_shim)
    ui_shim->OnMessageReceived(msg);
}

GpuProcessHostUIShim::GpuProcessHostUIShim(int host_id)
    : host_id_(host_id) {
  g_hosts_by_id.Pointer()->AddWithID(this, host_id_);
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelEstablished(
          host_id,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          base::Bind(&SendOnIOThreadTask, host_id_));
#endif
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::Create(int host_id) {
  DCHECK(!FromID(host_id));
  return new GpuProcessHostUIShim(host_id);
}

// static
void GpuProcessHostUIShim::Destroy(int host_id, const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      logging::LOG_ERROR, "GpuProcessHostUIShim",
      message);

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelDestroyed(host_id);
#endif

  delete FromID(host_id);
}

// static
void GpuProcessHostUIShim::DestroyAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!g_hosts_by_id.Pointer()->IsEmpty()) {
    IDMap<GpuProcessHostUIShim>::iterator it(g_hosts_by_id.Pointer());
    delete it.GetCurrentValue();
  }
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::FromID(int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_hosts_by_id.Pointer()->Lookup(host_id);
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::GetOneInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_hosts_by_id.Pointer()->IsEmpty())
    return NULL;
  IDMap<GpuProcessHostUIShim>::iterator it(g_hosts_by_id.Pointer());
  return it.GetCurrentValue();
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  return BrowserThread::PostTask(BrowserThread::IO,
                                 FROM_HERE,
                                 base::Bind(&SendOnIOThreadTask,
                                            host_id_,
                                            msg));
}

bool GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

#if defined(USE_OZONE)
  if (ui::OzonePlatform::GetInstance()
          ->GetGpuPlatformSupportHost()
          ->OnMessageReceived(message))
    return true;
#endif

  if (message.routing_id() != MSG_ROUTING_CONTROL)
    return false;

  return OnControlMessageReceived(message);
}

void GpuProcessHostUIShim::RelinquishGpuResources(
    const base::Closure& callback) {
  DCHECK(relinquish_callback_.is_null());
  relinquish_callback_ = callback;
  Send(new GpuMsg_RelinquishResources());
}

void GpuProcessHostUIShim::SimulateRemoveAllContext() {
  Send(new GpuMsg_Clean());
}

void GpuProcessHostUIShim::SimulateCrash() {
  Send(new GpuMsg_Crash());
}

void GpuProcessHostUIShim::SimulateHang() {
  Send(new GpuMsg_Hang());
}

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
  DCHECK(CalledOnValidThread());
  g_hosts_by_id.Pointer()->Remove(host_id_);
}

bool GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage,
                        OnLogMessage)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceInitialized,
                        OnAcceleratedSurfaceInitialized)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#endif
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)
    IPC_MESSAGE_HANDLER(GpuHostMsg_VideoMemoryUsageStats,
                        OnVideoMemoryUsageStatsReceived);
    IPC_MESSAGE_HANDLER(GpuHostMsg_ResourcesRelinquished,
                        OnResourcesRelinquished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AddSubscription, OnAddSubscription);
    IPC_MESSAGE_HANDLER(GpuHostMsg_RemoveSubscription, OnRemoveSubscription);

    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHostUIShim::OnLogMessage(
    int level,
    const std::string& header,
    const std::string& message) {
  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      level, header, message);
}

void GpuProcessHostUIShim::OnGraphicsInfoCollected(
    const gpu::GPUInfo& gpu_info) {
  // OnGraphicsInfoCollected is sent back after the GPU process successfully
  // initializes GL.
  TRACE_EVENT0("test_gpu", "OnGraphicsInfoCollected");

  GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info);
}

void GpuProcessHostUIShim::OnAcceleratedSurfaceInitialized(int32 surface_id,
                                                           int32 route_id) {
  RenderWidgetHostViewBase* view =
      GetRenderWidgetHostViewFromSurfaceID(surface_id);
  if (!view)
    return;
  view->AcceleratedSurfaceInitialized(route_id);
}

#if defined(OS_MACOSX)
void GpuProcessHostUIShim::OnAcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  TRACE_EVENT0("renderer",
      "GpuProcessHostUIShim::OnAcceleratedSurfaceBuffersSwapped");
  if (!ui::LatencyInfo::Verify(params.latency_info,
                               "GpuHostMsg_AcceleratedSurfaceBuffersSwapped")) {
    return;
  }

  // On Mac with delegated rendering, accelerated surfaces are not necessarily
  // associated with a RenderWidgetHostViewBase.
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  DCHECK(IsDelegatedRendererEnabled());

  // If the frame was intended for an NSView that the gfx::AcceleratedWidget is
  // no longer attached to, do not pass the frame along to the widget. Just ack
  // it to the GPU process immediately, so we can proceed to the next frame.
  bool should_not_show_frame =
      content::ImageTransportFactory::GetInstance()
          ->SurfaceShouldNotShowFramesAfterSuspendForRecycle(params.surface_id);
  if (should_not_show_frame) {
    OnSurfaceDisplayedCallback(params.surface_id);
  } else {
    gfx::AcceleratedWidget native_widget =
        content::GpuSurfaceTracker::Get()->AcquireNativeWidget(
            params.surface_id);
    ui::AcceleratedWidgetMacGotAcceleratedFrame(
        native_widget, params.surface_handle, params.latency_info, params.size,
        params.scale_factor,
        params.damage_rect,
        base::Bind(&OnSurfaceDisplayedCallback, params.surface_id),
        &ack_params.disable_throttling, &ack_params.renderer_id);
  }
  Send(new AcceleratedSurfaceMsg_BufferPresented(params.route_id, ack_params));
}
#endif

void GpuProcessHostUIShim::OnVideoMemoryUsageStatsReceived(
    const GPUVideoMemoryUsageStats& video_memory_usage_stats) {
  GpuDataManagerImpl::GetInstance()->UpdateVideoMemoryUsageStats(
      video_memory_usage_stats);
}

void GpuProcessHostUIShim::OnResourcesRelinquished() {
  if (!relinquish_callback_.is_null()) {
    base::ResetAndReturn(&relinquish_callback_).Run();
  }
}

void GpuProcessHostUIShim::OnAddSubscription(
    int32 process_id, unsigned int target) {
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph) {
    rph->OnAddSubscription(target);
  }
}

void GpuProcessHostUIShim::OnRemoveSubscription(
    int32 process_id, unsigned int target) {
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph) {
    rph->OnRemoveSubscription(target);
  }
}

}  // namespace content
