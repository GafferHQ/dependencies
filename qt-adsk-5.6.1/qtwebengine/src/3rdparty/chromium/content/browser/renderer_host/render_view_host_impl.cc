// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/speech_recognition_messages.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/filename_util.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/touch/touch_enabled.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/native_theme/native_theme_switches.h"
#include "url/url_constants.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "ui/gfx/platform_font_win.h"
#include "ui/gfx/win/dpi.h"
#endif

using base::TimeDelta;
using blink::WebConsoleMessage;
using blink::WebDragOperation;
using blink::WebDragOperationNone;
using blink::WebDragOperationsMask;
using blink::WebInputEvent;
using blink::WebMediaPlayerAction;
using blink::WebPluginAction;

namespace content {
namespace {

#if defined(OS_WIN)

const int kVirtualKeyboardDisplayWaitTimeoutMs = 100;
const int kMaxVirtualKeyboardDisplayRetries = 5;

void DismissVirtualKeyboardTask() {
  static int virtual_keyboard_display_retries = 0;
  // If the virtual keyboard is not yet visible, then we execute the task again
  // waiting for it to show up.
  if (!base::win::DismissVirtualKeyboard()) {
    if (virtual_keyboard_display_retries < kMaxVirtualKeyboardDisplayRetries) {
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(base::IgnoreResult(&DismissVirtualKeyboardTask)),
          TimeDelta::FromMilliseconds(kVirtualKeyboardDisplayWaitTimeoutMs));
      ++virtual_keyboard_display_retries;
    } else {
      virtual_keyboard_display_retries = 0;
    }
  }
}

void GetWindowsSpecificPrefs(RendererPreferences* prefs) {
  NONCLIENTMETRICS_XP metrics = {0};
  base::win::GetNonClientMetrics(&metrics);

  prefs->caption_font_family_name = metrics.lfCaptionFont.lfFaceName;
  prefs->caption_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfCaptionFont);

  prefs->small_caption_font_family_name = metrics.lfSmCaptionFont.lfFaceName;
  prefs->small_caption_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfSmCaptionFont);

  prefs->menu_font_family_name = metrics.lfMenuFont.lfFaceName;
  prefs->menu_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfMenuFont);

  prefs->status_font_family_name = metrics.lfStatusFont.lfFaceName;
  prefs->status_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfStatusFont);

  prefs->message_font_family_name = metrics.lfMessageFont.lfFaceName;
  prefs->message_font_height = gfx::PlatformFontWin::GetFontSize(
      metrics.lfMessageFont);

  prefs->vertical_scroll_bar_width_in_dips =
      gfx::win::GetSystemMetricsInDIP(SM_CXVSCROLL);
  prefs->horizontal_scroll_bar_height_in_dips =
      gfx::win::GetSystemMetricsInDIP(SM_CYHSCROLL);
  prefs->arrow_bitmap_height_vertical_scroll_bar_in_dips =
      gfx::win::GetSystemMetricsInDIP(SM_CYVSCROLL);
  prefs->arrow_bitmap_width_horizontal_scroll_bar_in_dips =
      gfx::win::GetSystemMetricsInDIP(SM_CXHSCROLL);
}
#endif

}  // namespace

// static
const int64 RenderViewHostImpl::kUnloadTimeoutMS = 1000;

///////////////////////////////////////////////////////////////////////////////
// RenderViewHost, public:

// static
RenderViewHost* RenderViewHost::FromID(int render_process_id,
                                       int render_view_id) {
  return RenderViewHostImpl::FromID(render_process_id, render_view_id);
}

// static
RenderViewHost* RenderViewHost::From(RenderWidgetHost* rwh) {
  DCHECK(rwh->IsRenderView());
  return static_cast<RenderViewHostImpl*>(RenderWidgetHostImpl::From(rwh));
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, public:

// static
RenderViewHostImpl* RenderViewHostImpl::FromID(int render_process_id,
                                               int render_view_id) {
  RenderWidgetHost* widget =
      RenderWidgetHost::FromID(render_process_id, render_view_id);
  if (!widget || !widget->IsRenderView())
    return NULL;
  return static_cast<RenderViewHostImpl*>(RenderWidgetHostImpl::From(widget));
}

RenderViewHostImpl::RenderViewHostImpl(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int routing_id,
    int main_frame_routing_id,
    bool swapped_out,
    bool hidden,
    bool has_initialized_audio_host)
    : RenderWidgetHostImpl(widget_delegate,
                           instance->GetProcess(),
                           routing_id,
                           hidden),
      frames_ref_count_(0),
      delegate_(delegate),
      instance_(static_cast<SiteInstanceImpl*>(instance)),
      waiting_for_drag_context_response_(false),
      enabled_bindings_(0),
      page_id_(-1),
      is_active_(!swapped_out),
      is_swapped_out_(swapped_out),
      main_frame_routing_id_(main_frame_routing_id),
      is_waiting_for_close_ack_(false),
      sudden_termination_allowed_(false),
      render_view_termination_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      virtual_keyboard_requested_(false),
      is_focused_element_editable_(false),
      updating_web_preferences_(false),
      weak_factory_(this) {
  DCHECK(instance_.get());
  CHECK(delegate_);  // http://crbug.com/82827

  GetProcess()->AddObserver(this);
  GetProcess()->EnableSendQueue();

  if (ResourceDispatcherHostImpl::Get()) {
    bool has_active_audio = false;
    if (has_initialized_audio_host) {
      scoped_refptr<AudioRendererHost> arh =
          static_cast<RenderProcessHostImpl*>(GetProcess())
              ->audio_renderer_host();
      if (arh.get())
        has_active_audio =
            arh->RenderFrameHasActiveAudio(main_frame_routing_id_);
    }
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostCreated,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(),
                   GetRoutingID(),
                   !is_hidden(),
                   has_active_audio));
  }
}

RenderViewHostImpl::~RenderViewHostImpl() {
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostDeleted,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(), GetRoutingID()));
  }

  delegate_->RenderViewDeleted(this);
  GetProcess()->RemoveObserver(this);
}

RenderViewHostDelegate* RenderViewHostImpl::GetDelegate() const {
  return delegate_;
}

SiteInstanceImpl* RenderViewHostImpl::GetSiteInstance() const {
  return instance_.get();
}

bool RenderViewHostImpl::CreateRenderView(
    int opener_frame_route_id,
    int proxy_route_id,
    int32 max_page_id,
    const FrameReplicationState& replicated_frame_state,
    bool window_was_created_with_opener) {
  TRACE_EVENT0("renderer_host,navigation",
               "RenderViewHostImpl::CreateRenderView");
  DCHECK(!IsRenderViewLive()) << "Creating view twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;
  DCHECK(GetProcess()->HasConnection());
  DCHECK(GetProcess()->GetBrowserContext());

  set_renderer_initialized(true);

  GpuSurfaceTracker::Get()->SetSurfaceHandle(
      surface_id(), GetCompositingSurface());

  // Ensure the RenderView starts with a next_page_id larger than any existing
  // page ID it might be asked to render.
  int32 next_page_id = 1;
  if (max_page_id > -1)
    next_page_id = max_page_id + 1;

  ViewMsg_New_Params params;
  params.renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext());
#if defined(OS_WIN)
  GetWindowsSpecificPrefs(&params.renderer_preferences);
#endif
  params.web_preferences = GetWebkitPreferences();
  params.view_id = GetRoutingID();
  params.main_frame_routing_id = main_frame_routing_id_;
  params.surface_id = surface_id();
  params.session_storage_namespace_id =
      delegate_->GetSessionStorageNamespace(instance_.get())->id();
  // Ensure the RenderView sets its opener correctly.
  params.opener_frame_route_id = opener_frame_route_id;
  params.swapped_out = !is_active_;
  params.replicated_frame_state = replicated_frame_state;
  params.proxy_routing_id = proxy_route_id;
  params.hidden = is_hidden();
  params.never_visible = delegate_->IsNeverVisible();
  params.window_was_created_with_opener = window_was_created_with_opener;
  params.next_page_id = next_page_id;
  params.enable_auto_resize = auto_resize_enabled();
  params.min_size = min_size_for_auto_resize();
  params.max_size = max_size_for_auto_resize();
  GetResizeParams(&params.initial_size);

  if (!Send(new ViewMsg_New(params)))
    return false;
  SetInitialRenderSizeParams(params.initial_size);

  // If the RWHV has not yet been set, the surface ID namespace will get
  // passed down by the call to SetView().
  if (view_) {
    Send(new ViewMsg_SetSurfaceIdNamespace(GetRoutingID(),
                                           view_->GetSurfaceIdNamespace()));
  }

  // If it's enabled, tell the renderer to set up the Javascript bindings for
  // sending messages back to the browser.
  if (GetProcess()->IsForGuestsOnly())
    DCHECK_EQ(0, enabled_bindings_);
  Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
  // Let our delegate know that we created a RenderView.
  delegate_->RenderViewCreated(this);

  // Since this method can create the main RenderFrame in the renderer process,
  // set the proper state on its corresponding RenderFrameHost.
  if (main_frame_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameHostImpl::FromID(GetProcess()->GetID(), main_frame_routing_id_)
        ->SetRenderFrameCreated(true);
  }

  return true;
}

bool RenderViewHostImpl::IsRenderViewLive() const {
  return GetProcess()->HasConnection() && renderer_initialized();
}

void RenderViewHostImpl::SyncRendererPrefs() {
  RendererPreferences renderer_preferences =
      delegate_->GetRendererPrefs(GetProcess()->GetBrowserContext());
#if defined(OS_WIN)
  GetWindowsSpecificPrefs(&renderer_preferences);
#endif
  Send(new ViewMsg_SetRendererPrefs(GetRoutingID(), renderer_preferences));
}

WebPreferences RenderViewHostImpl::ComputeWebkitPrefs() {
  TRACE_EVENT0("browser", "RenderViewHostImpl::GetWebkitPrefs");
  WebPreferences prefs;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  prefs.web_security_enabled =
      !command_line.HasSwitch(switches::kDisableWebSecurity);
  prefs.java_enabled =
      !command_line.HasSwitch(switches::kDisableJava);

  prefs.remote_fonts_enabled =
      !command_line.HasSwitch(switches::kDisableRemoteFonts);
  prefs.application_cache_enabled = true;
  prefs.xss_auditor_enabled =
      !command_line.HasSwitch(switches::kDisableXSSAuditor);
  prefs.local_storage_enabled =
      !command_line.HasSwitch(switches::kDisableLocalStorage);
  prefs.databases_enabled =
      !command_line.HasSwitch(switches::kDisableDatabases);
#if defined(OS_ANDROID)
  // WebAudio is enabled by default on x86 and ARM.
  prefs.webaudio_enabled =
      !command_line.HasSwitch(switches::kDisableWebAudio);
#endif

  prefs.experimental_webgl_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisable3DAPIs) &&
      !command_line.HasSwitch(switches::kDisableExperimentalWebGL);

  prefs.pepper_3d_enabled =
      !command_line.HasSwitch(switches::kDisablePepper3d);

  prefs.flash_3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlash3d);
  prefs.flash_stage3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);
  prefs.flash_stage3d_baseline_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);

  prefs.allow_file_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowFileAccessFromFiles);

  prefs.accelerated_2d_canvas_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  prefs.antialiased_2d_canvas_disabled =
      command_line.HasSwitch(switches::kDisable2dCanvasAntialiasing);
  prefs.antialiased_clips_2d_canvas_enabled =
      command_line.HasSwitch(switches::kEnable2dCanvasClipAntialiasing);
  prefs.accelerated_2d_canvas_msaa_sample_count =
      atoi(command_line.GetSwitchValueASCII(
      switches::kAcceleratedCanvas2dMSAASampleCount).c_str());
  prefs.text_blobs_enabled =
      !command_line.HasSwitch(switches::kDisableTextBlobs);

  prefs.pinch_overlay_scrollbar_thickness = 10;
  prefs.use_solid_color_scrollbars = ui::IsOverlayScrollbarEnabled();
  prefs.invert_viewport_scroll_order =
      command_line.HasSwitch(switches::kInvertViewportScrollOrder);

#if defined(OS_ANDROID)
  // On Android, user gestures are normally required, unless that requirement
  // is disabled with a command-line switch or the equivalent field trial is
  // is set to "Enabled".
  const std::string autoplay_group_name = base::FieldTrialList::FindFullName(
      "MediaElementAutoplay");
  prefs.user_gesture_required_for_media_playback = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForMediaPlayback) &&
          (autoplay_group_name.empty() || autoplay_group_name != "Enabled");
#endif

  prefs.touch_enabled = ui::AreTouchEventsEnabled();
  prefs.device_supports_touch = prefs.touch_enabled &&
      ui::IsTouchDevicePresent();
  prefs.available_pointer_types = ui::GetAvailablePointerTypes();
  prefs.primary_pointer_type = ui::GetPrimaryPointerType();
  prefs.available_hover_types = ui::GetAvailableHoverTypes();
  prefs.primary_hover_type = ui::GetPrimaryHoverType();

#if defined(OS_ANDROID)
  prefs.device_supports_mouse = false;
#endif

  prefs.pointer_events_max_touch_points = ui::MaxTouchPoints();

  prefs.touch_adjustment_enabled =
      !command_line.HasSwitch(switches::kDisableTouchAdjustment);

  const std::string slimming_group =
      base::FieldTrialList::FindFullName("SlimmingPaint");
  prefs.slimming_paint_enabled =
      (command_line.HasSwitch(switches::kEnableSlimmingPaint) ||
      !command_line.HasSwitch(switches::kDisableSlimmingPaint)) &&
      (slimming_group != "DisableSlimmingPaint");
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  bool default_enable_scroll_animator = true;
#else
  bool default_enable_scroll_animator = false;
#endif
  prefs.enable_scroll_animator = default_enable_scroll_animator;
  if (command_line.HasSwitch(switches::kEnableSmoothScrolling))
    prefs.enable_scroll_animator = true;
  if (command_line.HasSwitch(switches::kDisableSmoothScrolling))
    prefs.enable_scroll_animator = false;

  // Certain GPU features might have been blacklisted.
  GpuDataManagerImpl::GetInstance()->UpdateRendererWebPrefs(&prefs);

  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    prefs.loads_images_automatically = true;
    prefs.javascript_enabled = true;
  }

  prefs.connection_type = net::NetworkChangeNotifier::GetConnectionType();
  prefs.is_online =
      prefs.connection_type != net::NetworkChangeNotifier::CONNECTION_NONE;

  prefs.number_of_cpu_cores = base::SysInfo::NumberOfProcessors();

  prefs.viewport_meta_enabled =
      command_line.HasSwitch(switches::kEnableViewportMeta);

  prefs.viewport_enabled =
      command_line.HasSwitch(switches::kEnableViewport) ||
      prefs.viewport_meta_enabled;

  prefs.main_frame_resizes_are_orientation_changes =
      command_line.HasSwitch(switches::kMainFrameResizesAreOrientationChanges);

  prefs.image_color_profiles_enabled =
      command_line.HasSwitch(switches::kEnableImageColorProfiles);

  prefs.spatial_navigation_enabled = command_line.HasSwitch(
      switches::kEnableSpatialNavigation);

  prefs.disable_reading_from_canvas = command_line.HasSwitch(
      switches::kDisableReadingFromCanvas);

  prefs.strict_mixed_content_checking = command_line.HasSwitch(
      switches::kEnableStrictMixedContentChecking);

  prefs.strict_powerful_feature_restrictions = command_line.HasSwitch(
      switches::kEnableStrictPowerfulFeatureRestrictions);

  prefs.v8_cache_options = GetV8CacheOptions();

  GetContentClient()->browser()->OverrideWebkitPrefs(this, &prefs);
  return prefs;
}

void RenderViewHostImpl::SuppressDialogsUntilSwapOut() {
  Send(new ViewMsg_SuppressDialogsUntilSwapOut(GetRoutingID()));
}

void RenderViewHostImpl::ClosePage() {
  is_waiting_for_close_ack_ = true;
  StartHangMonitorTimeout(TimeDelta::FromMilliseconds(kUnloadTimeoutMS));

  if (IsRenderViewLive()) {
    // Since we are sending an IPC message to the renderer, increase the event
    // count to prevent the hang monitor timeout from being stopped by input
    // event acknowledgements.
    increment_in_flight_event_count();

    // TODO(creis): Should this be moved to Shutdown?  It may not be called for
    // RenderViewHosts that have been swapped out.
    NotificationService::current()->Notify(
        NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
        Source<RenderViewHost>(this),
        NotificationService::NoDetails());

    Send(new ViewMsg_ClosePage(GetRoutingID()));
  } else {
    // This RenderViewHost doesn't have a live renderer, so just skip the unload
    // event and close the page.
    ClosePageIgnoringUnloadEvents();
  }
}

void RenderViewHostImpl::ClosePageIgnoringUnloadEvents() {
  StopHangMonitorTimeout();
  is_waiting_for_close_ack_ = false;

  sudden_termination_allowed_ = true;
  delegate_->Close(this);
}

#if defined(OS_ANDROID)
void RenderViewHostImpl::ActivateNearestFindResult(int request_id,
                                                   float x,
                                                   float y) {
  Send(new InputMsg_ActivateNearestFindResult(GetRoutingID(),
                                              request_id, x, y));
}

void RenderViewHostImpl::RequestFindMatchRects(int current_version) {
  Send(new ViewMsg_FindMatchRects(GetRoutingID(), current_version));
}
#endif

void RenderViewHostImpl::RenderProcessExited(RenderProcessHost* host,
                                             base::TerminationStatus status,
                                             int exit_code) {
  if (!renderer_initialized())
    return;

  RenderWidgetHostImpl::RendererExited(status, exit_code);
  delegate_->RenderViewTerminated(
      this, static_cast<base::TerminationStatus>(status), exit_code);
}

void RenderViewHostImpl::DragTargetDragEnter(
    const DropData& drop_data,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed,
    int key_modifiers) {
  const int renderer_id = GetProcess()->GetID();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

#if defined(OS_CHROMEOS)
  // The externalfile:// scheme is used in Chrome OS to open external files in a
  // browser tab.
  if (drop_data.url.SchemeIs(content::kExternalFileScheme))
    policy->GrantRequestURL(renderer_id, drop_data.url);
#endif

  // The URL could have been cobbled together from any highlighted text string,
  // and can't be interpreted as a capability.
  DropData filtered_data(drop_data);
  GetProcess()->FilterURL(true, &filtered_data.url);
  if (drop_data.did_originate_from_renderer) {
    filtered_data.filenames.clear();
  }

  // The filenames vector, on the other hand, does represent a capability to
  // access the given files.
  storage::IsolatedContext::FileInfoSet files;
  for (std::vector<ui::FileInfo>::iterator iter(
           filtered_data.filenames.begin());
       iter != filtered_data.filenames.end();
       ++iter) {
    // A dragged file may wind up as the value of an input element, or it
    // may be used as the target of a navigation instead.  We don't know
    // which will happen at this point, so generously grant both access
    // and request permissions to the specific file to cover both cases.
    // We do not give it the permission to request all file:// URLs.

    // Make sure we have the same display_name as the one we register.
    if (iter->display_name.empty()) {
      std::string name;
      files.AddPath(iter->path, &name);
      iter->display_name = base::FilePath::FromUTF8Unsafe(name);
    } else {
      files.AddPathWithName(iter->path, iter->display_name.AsUTF8Unsafe());
    }

    policy->GrantRequestSpecificFileURL(renderer_id,
                                        net::FilePathToFileURL(iter->path));

    // If the renderer already has permission to read these paths, we don't need
    // to re-grant them. This prevents problems with DnD for files in the CrOS
    // file manager--the file manager already had read/write access to those
    // directories, but dragging a file would cause the read/write access to be
    // overwritten with read-only access, making them impossible to delete or
    // rename until the renderer was killed.
    if (!policy->CanReadFile(renderer_id, iter->path))
      policy->GrantReadFile(renderer_id, iter->path);
  }

  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  std::string filesystem_id = isolated_context->RegisterDraggedFileSystem(
      files);
  if (!filesystem_id.empty()) {
    // Grant the permission iff the ID is valid.
    policy->GrantReadFileSystem(renderer_id, filesystem_id);
  }
  filtered_data.filesystem_id = base::UTF8ToUTF16(filesystem_id);

  storage::FileSystemContext* file_system_context =
      BrowserContext::GetStoragePartition(GetProcess()->GetBrowserContext(),
                                          GetSiteInstance())
          ->GetFileSystemContext();
  for (size_t i = 0; i < filtered_data.file_system_files.size(); ++i) {
    storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(filtered_data.file_system_files[i].url);

    std::string register_name;
    std::string filesystem_id = isolated_context->RegisterFileSystemForPath(
        file_system_url.type(), file_system_url.filesystem_id(),
        file_system_url.path(), &register_name);
    policy->GrantReadFileSystem(renderer_id, filesystem_id);

    // Note: We are using the origin URL provided by the sender here. It may be
    // different from the receiver's.
    filtered_data.file_system_files[i].url =
        GURL(storage::GetIsolatedFileSystemRootURIString(
                 file_system_url.origin(), filesystem_id, std::string())
                 .append(register_name));
  }

  Send(new DragMsg_TargetDragEnter(GetRoutingID(), filtered_data, client_pt,
                                   screen_pt, operations_allowed,
                                   key_modifiers));
}

void RenderViewHostImpl::DragTargetDragOver(
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    WebDragOperationsMask operations_allowed,
    int key_modifiers) {
  Send(new DragMsg_TargetDragOver(GetRoutingID(), client_pt, screen_pt,
                                  operations_allowed, key_modifiers));
}

void RenderViewHostImpl::DragTargetDragLeave() {
  Send(new DragMsg_TargetDragLeave(GetRoutingID()));
}

void RenderViewHostImpl::DragTargetDrop(
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    int key_modifiers) {
  Send(new DragMsg_TargetDrop(GetRoutingID(), client_pt, screen_pt,
                              key_modifiers));
}

void RenderViewHostImpl::DragSourceEndedAt(
    int client_x, int client_y, int screen_x, int screen_y,
    WebDragOperation operation) {
  Send(new DragMsg_SourceEnded(GetRoutingID(),
                               gfx::Point(client_x, client_y),
                               gfx::Point(screen_x, screen_y),
                               operation));
}

void RenderViewHostImpl::DragSourceSystemDragEnded() {
  Send(new DragMsg_SourceSystemDragEnded(GetRoutingID()));
}

RenderFrameHost* RenderViewHostImpl::GetMainFrame() {
  return RenderFrameHost::FromID(GetProcess()->GetID(), main_frame_routing_id_);
}

void RenderViewHostImpl::AllowBindings(int bindings_flags) {
  // Never grant any bindings to browser plugin guests.
  if (GetProcess()->IsForGuestsOnly()) {
    NOTREACHED() << "Never grant bindings to a guest process.";
    return;
  }

  // Ensure we aren't granting WebUI bindings to a process that has already
  // been used for non-privileged views.
  if (bindings_flags & BINDINGS_POLICY_WEB_UI &&
      GetProcess()->HasConnection() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    // This process has no bindings yet. Make sure it does not have more
    // than this single active view.
    // --single-process only has one renderer.
    if (GetProcess()->GetActiveViewCount() > 1 &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess))
      return;
  }

  if (bindings_flags & BINDINGS_POLICY_WEB_UI) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        GetProcess()->GetID());
  }

  enabled_bindings_ |= bindings_flags;
  if (renderer_initialized())
    Send(new ViewMsg_AllowBindings(GetRoutingID(), enabled_bindings_));
}

int RenderViewHostImpl::GetEnabledBindings() const {
  return enabled_bindings_;
}

void RenderViewHostImpl::SetWebUIProperty(const std::string& name,
                                          const std::string& value) {
  // This is a sanity check before telling the renderer to enable the property.
  // It could lie and send the corresponding IPC messages anyway, but we will
  // not act on them if enabled_bindings_ doesn't agree. If we get here without
  // WebUI bindings, kill the renderer process.
  if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI) {
    Send(new ViewMsg_SetWebUIProperty(GetRoutingID(), name, value));
  } else {
    RecordAction(
        base::UserMetricsAction("BindingsMismatchTerminate_RVH_WebUI"));
    GetProcess()->Shutdown(content::RESULT_CODE_KILLED, false);
  }
}

void RenderViewHostImpl::GotFocus() {
  RenderWidgetHostImpl::GotFocus();  // Notifies the renderer it got focus.

  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->GotFocus();
}

void RenderViewHostImpl::LostCapture() {
  RenderWidgetHostImpl::LostCapture();
  delegate_->LostCapture();
}

void RenderViewHostImpl::LostMouseLock() {
  RenderWidgetHostImpl::LostMouseLock();
  delegate_->LostMouseLock();
}

void RenderViewHostImpl::SetInitialFocus(bool reverse) {
  Send(new ViewMsg_SetInitialFocus(GetRoutingID(), reverse));
}

void RenderViewHostImpl::FilesSelectedInChooser(
    const std::vector<content::FileChooserFileInfo>& files,
    FileChooserParams::Mode permissions) {
  storage::FileSystemContext* const file_system_context =
      BrowserContext::GetStoragePartition(GetProcess()->GetBrowserContext(),
                                          GetSiteInstance())
          ->GetFileSystemContext();
  // Grant the security access requested to the given files.
  for (size_t i = 0; i < files.size(); ++i) {
    const content::FileChooserFileInfo& file = files[i];
    if (permissions == FileChooserParams::Save) {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantCreateReadWriteFile(
          GetProcess()->GetID(), file.file_path);
    } else {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
          GetProcess()->GetID(), file.file_path);
    }
    if (file.file_system_url.is_valid()) {
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFileSystem(
          GetProcess()->GetID(),
          file_system_context->CrackURL(file.file_system_url)
          .mount_filesystem_id());
    }
  }
  Send(new ViewMsg_RunFileChooserResponse(GetRoutingID(), files));
}

void RenderViewHostImpl::DirectoryEnumerationFinished(
    int request_id,
    const std::vector<base::FilePath>& files) {
  // Grant the security access requested to the given files.
  for (std::vector<base::FilePath>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        GetProcess()->GetID(), *file);
  }
  Send(new ViewMsg_EnumerateDirectoryResponse(GetRoutingID(),
                                              request_id,
                                              files));
}

void RenderViewHostImpl::SetIsLoading(bool is_loading) {
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostSetIsLoading,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(),
                   GetRoutingID(),
                   is_loading));
  }
  RenderWidgetHostImpl::SetIsLoading(is_loading);
}

void RenderViewHostImpl::LoadStateChanged(
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64 upload_position,
    uint64 upload_size) {
  delegate_->LoadStateChanged(url, load_state, upload_position, upload_size);
}

bool RenderViewHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_ ||
      GetProcess()->SuddenTerminationAllowed();
}

///////////////////////////////////////////////////////////////////////////////
// RenderViewHostImpl, IPC message handlers:

bool RenderViewHostImpl::OnMessageReceived(const IPC::Message& msg) {
  if (!BrowserMessageFilter::CheckCanDispatchOnUI(msg, this))
    return true;

  // Filter out most IPC messages if this renderer is swapped out.
  // We still want to handle certain ACKs to keep our state consistent.
  if (is_swapped_out_) {
    if (!SwappedOutMessages::CanHandleWhileSwappedOut(msg)) {
      // If this is a synchronous message and we decided not to handle it,
      // we must send an error reply, or else the renderer will be stuck
      // and won't respond to future requests.
      if (msg.is_sync()) {
        IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
        reply->set_reply_error();
        Send(reply);
      }
      // Don't continue looking for someone to handle it.
      return true;
    }
  }

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowView, OnShowView)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowFullscreenWidget,
                        OnShowFullscreenWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RenderViewReady, OnRenderViewReady)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateState, OnUpdateState)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateTargetURL, OnUpdateTargetURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Close, OnClose)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestMove, OnRequestMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentAvailableInMainFrame,
                        OnDocumentAvailableInMainFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidContentsPreferredSizeChange,
                        OnDidContentsPreferredSizeChange)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RouteCloseEvent,
                        OnRouteCloseEvent)
    IPC_MESSAGE_HANDLER(DragHostMsg_StartDragging, OnStartDragging)
    IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor, OnUpdateDragCursor)
    IPC_MESSAGE_HANDLER(DragHostMsg_TargetDrop_ACK, OnTargetDropACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeChanged, OnFocusedNodeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PageScaleFactorIsOneChanged,
                        OnPageScaleFactorIsOneChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RunFileChooser, OnRunFileChooser)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FocusedNodeTouched, OnFocusedNodeTouched)
    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(
        handled = RenderWidgetHostImpl::OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderViewHostImpl::Init() {
  RenderWidgetHostImpl::Init();
}

void RenderViewHostImpl::Shutdown() {
  // We can't release the SessionStorageNamespace until our peer
  // in the renderer has wound down.
  if (GetProcess()->HasConnection()) {
    RenderProcessHostImpl::ReleaseOnCloseACK(
        GetProcess(),
        delegate_->GetSessionStorageNamespaceMap(),
        GetRoutingID());
  }

  RenderWidgetHostImpl::Shutdown();
}

void RenderViewHostImpl::WasHidden() {
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostWasHidden,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(), GetRoutingID()));
  }

  RenderWidgetHostImpl::WasHidden();
}

void RenderViewHostImpl::WasShown(const ui::LatencyInfo& latency_info) {
  if (ResourceDispatcherHostImpl::Get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ResourceDispatcherHostImpl::OnRenderViewHostWasShown,
                   base::Unretained(ResourceDispatcherHostImpl::Get()),
                   GetProcess()->GetID(), GetRoutingID()));
  }

  RenderWidgetHostImpl::WasShown(latency_info);
}

bool RenderViewHostImpl::IsRenderView() const {
  return true;
}

void RenderViewHostImpl::CreateNewWindow(
    int route_id,
    int main_frame_route_id,
    const ViewHostMsg_CreateWindow_Params& params,
    SessionStorageNamespace* session_storage_namespace) {
  ViewHostMsg_CreateWindow_Params validated_params(params);
  GetProcess()->FilterURL(false, &validated_params.target_url);
  GetProcess()->FilterURL(false, &validated_params.opener_url);
  GetProcess()->FilterURL(true, &validated_params.opener_security_origin);

  delegate_->CreateNewWindow(
      GetProcess()->GetID(), route_id, main_frame_route_id, validated_params,
      session_storage_namespace);
}

void RenderViewHostImpl::CreateNewWidget(int route_id,
                                     blink::WebPopupType popup_type) {
  delegate_->CreateNewWidget(GetProcess()->GetID(), route_id, popup_type);
}

void RenderViewHostImpl::CreateNewFullscreenWidget(int route_id) {
  delegate_->CreateNewFullscreenWidget(GetProcess()->GetID(), route_id);
}

void RenderViewHostImpl::OnShowView(int route_id,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_rect,
                                    bool user_gesture) {
  if (is_active_) {
    delegate_->ShowCreatedWindow(
        route_id, disposition, initial_rect, user_gesture);
  }
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnShowWidget(int route_id,
                                      const gfx::Rect& initial_rect) {
  if (is_active_)
    delegate_->ShowCreatedWidget(route_id, initial_rect);
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnShowFullscreenWidget(int route_id) {
  if (is_active_)
    delegate_->ShowCreatedFullscreenWidget(route_id);
  Send(new ViewMsg_Move_ACK(route_id));
}

void RenderViewHostImpl::OnRenderViewReady() {
  render_view_termination_status_ = base::TERMINATION_STATUS_STILL_RUNNING;
  SendScreenRects();
  WasResized();
  delegate_->RenderViewReady(this);
}

void RenderViewHostImpl::OnRenderProcessGone(int status, int exit_code) {
  // Do nothing, otherwise RenderWidgetHostImpl will assume it is not a
  // RenderViewHostImpl and destroy itself.
  // TODO(nasko): Remove this hack once RenderViewHost and RenderWidgetHost are
  // decoupled.
}

void RenderViewHostImpl::OnUpdateState(int32 page_id, const PageState& state) {
  // If the following DCHECK fails, you have encountered a tricky edge-case that
  // has evaded reproduction for a very long time. Please report what you were
  // doing on http://crbug.com/407376, whether or not you can reproduce the
  // failure.
  DCHECK_EQ(page_id, page_id_);

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(state)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RVH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  delegate_->UpdateState(this, page_id, state);
}

void RenderViewHostImpl::OnUpdateTargetURL(const GURL& url) {
  if (is_active_)
    delegate_->UpdateTargetURL(this, url);

  // Send a notification back to the renderer that we are ready to
  // receive more target urls.
  Send(new ViewMsg_UpdateTargetURL_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnClose() {
  // If the renderer is telling us to close, it has already run the unload
  // events, and we can take the fast path.
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::OnRequestMove(const gfx::Rect& pos) {
  if (is_active_)
    delegate_->RequestMove(pos);
  Send(new ViewMsg_Move_ACK(GetRoutingID()));
}

void RenderViewHostImpl::OnDocumentAvailableInMainFrame(
    bool uses_temporary_zoom_level) {
  delegate_->DocumentAvailableInMainFrame(this);

  if (!uses_temporary_zoom_level)
    return;

  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));
  host_zoom_map->SetTemporaryZoomLevel(GetProcess()->GetID(),
                                       GetRoutingID(),
                                       host_zoom_map->GetDefaultZoomLevel());
}

void RenderViewHostImpl::OnDidContentsPreferredSizeChange(
    const gfx::Size& new_size) {
  delegate_->UpdatePreferredSize(new_size);
}

void RenderViewHostImpl::OnRenderAutoResized(const gfx::Size& new_size) {
  delegate_->ResizeDueToAutoResize(new_size);
}

void RenderViewHostImpl::OnRouteCloseEvent() {
  // Have the delegate route this to the active RenderViewHost.
  delegate_->RouteCloseEvent(this);
}

void RenderViewHostImpl::OnStartDragging(
    const DropData& drop_data,
    WebDragOperationsMask drag_operations_mask,
    const SkBitmap& bitmap,
    const gfx::Vector2d& bitmap_offset_in_dip,
    const DragEventSourceInfo& event_info) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (!view)
    return;

  DropData filtered_data(drop_data);
  RenderProcessHost* process = GetProcess();
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Allow drag of Javascript URLs to enable bookmarklet drag to bookmark bar.
  if (!filtered_data.url.SchemeIs(url::kJavaScriptScheme))
    process->FilterURL(true, &filtered_data.url);
  process->FilterURL(false, &filtered_data.html_base_url);
  // Filter out any paths that the renderer didn't have access to. This prevents
  // the following attack on a malicious renderer:
  // 1. StartDragging IPC sent with renderer-specified filesystem paths that it
  //    doesn't have read permissions for.
  // 2. We initiate a native DnD operation.
  // 3. DnD operation immediately ends since mouse is not held down. DnD events
  //    still fire though, which causes read permissions to be granted to the
  //    renderer for any file paths in the drop.
  filtered_data.filenames.clear();
  for (std::vector<ui::FileInfo>::const_iterator it =
           drop_data.filenames.begin();
       it != drop_data.filenames.end();
       ++it) {
    if (policy->CanReadFile(GetProcess()->GetID(), it->path))
      filtered_data.filenames.push_back(*it);
  }

  storage::FileSystemContext* file_system_context =
      BrowserContext::GetStoragePartition(GetProcess()->GetBrowserContext(),
                                          GetSiteInstance())
          ->GetFileSystemContext();
  filtered_data.file_system_files.clear();
  for (size_t i = 0; i < drop_data.file_system_files.size(); ++i) {
    storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(drop_data.file_system_files[i].url);
    if (policy->CanReadFileSystemFile(GetProcess()->GetID(), file_system_url))
      filtered_data.file_system_files.push_back(drop_data.file_system_files[i]);
  }

  float scale = GetScaleFactorForView(GetView());
  gfx::ImageSkia image(gfx::ImageSkiaRep(bitmap, scale));
  view->StartDragging(filtered_data, drag_operations_mask, image,
      bitmap_offset_in_dip, event_info);
}

void RenderViewHostImpl::OnUpdateDragCursor(WebDragOperation current_op) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->UpdateDragCursor(current_op);
}

void RenderViewHostImpl::OnTargetDropACK() {
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_VIEW_HOST_DID_RECEIVE_DRAG_TARGET_DROP_ACK,
      Source<RenderViewHost>(this),
      NotificationService::NoDetails());
}

void RenderViewHostImpl::OnTakeFocus(bool reverse) {
  RenderViewHostDelegateView* view = delegate_->GetDelegateView();
  if (view)
    view->TakeFocus(reverse);
}

void RenderViewHostImpl::OnFocusedNodeChanged(
    bool is_editable_node,
    const gfx::Rect& node_bounds_in_viewport) {
  is_focused_element_editable_ = is_editable_node;
  if (view_)
    view_->FocusedNodeChanged(is_editable_node);
#if defined(OS_WIN)
  if (!is_editable_node && virtual_keyboard_requested_) {
    virtual_keyboard_requested_ = false;
    delegate_->SetIsVirtualKeyboardRequested(false);
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(base::IgnoreResult(&DismissVirtualKeyboardTask)),
        TimeDelta::FromMilliseconds(kVirtualKeyboardDisplayWaitTimeoutMs));
  }
#endif

  // Convert node_bounds to screen coordinates.
  gfx::Rect view_bounds_in_screen = view_->GetViewBounds();
  gfx::Point origin = node_bounds_in_viewport.origin();
  origin.Offset(view_bounds_in_screen.x(), view_bounds_in_screen.y());
  gfx::Rect node_bounds_in_screen(origin.x(), origin.y(),
                                  node_bounds_in_viewport.width(),
                                  node_bounds_in_viewport.height());
  FocusedNodeDetails details = {is_editable_node, node_bounds_in_screen};
  NotificationService::current()->Notify(NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                                         Source<RenderViewHost>(this),
                                         Details<FocusedNodeDetails>(&details));
}

void RenderViewHostImpl::OnUserGesture() {
  delegate_->OnUserGesture();
}

void RenderViewHostImpl::OnClosePageACK() {
  decrement_in_flight_event_count();
  ClosePageIgnoringUnloadEvents();
}

void RenderViewHostImpl::NotifyRendererUnresponsive() {
  delegate_->RendererUnresponsive(this);
}

void RenderViewHostImpl::NotifyRendererResponsive() {
  delegate_->RendererResponsive(this);
}

void RenderViewHostImpl::RequestToLockMouse(bool user_gesture,
                                            bool last_unlocked_by_target) {
  delegate_->RequestToLockMouse(user_gesture, last_unlocked_by_target);
}

bool RenderViewHostImpl::IsFullscreenGranted() const {
  return delegate_->IsFullscreenForCurrentTab();
}

blink::WebDisplayMode RenderViewHostImpl::GetDisplayMode() const {
  return delegate_->GetDisplayMode();
}

void RenderViewHostImpl::OnFocus() {
  // Note: We allow focus and blur from swapped out RenderViewHosts, even when
  // the active RenderViewHost is in a different BrowsingInstance (e.g., WebUI).
  delegate_->Activate();
}

void RenderViewHostImpl::OnBlur() {
  delegate_->Deactivate();
}

gfx::Rect RenderViewHostImpl::GetRootWindowResizerRect() const {
  return delegate_->GetRootWindowResizerRect();
}

void RenderViewHostImpl::ForwardMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  RenderWidgetHostImpl::ForwardMouseEvent(mouse_event);
  if (mouse_event.type == WebInputEvent::MouseWheel && ignore_input_events())
    delegate_->OnIgnoredUIEvent();
}

void RenderViewHostImpl::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& key_event) {
  if (ignore_input_events()) {
    if (key_event.type == WebInputEvent::RawKeyDown)
      delegate_->OnIgnoredUIEvent();
    return;
  }
  RenderWidgetHostImpl::ForwardKeyboardEvent(key_event);
}

void RenderViewHostImpl::OnTextSurroundingSelectionResponse(
    const base::string16& content,
    size_t start_offset,
    size_t end_offset) {
  if (!view_)
    return;
  view_->OnTextSurroundingSelectionResponse(content, start_offset, end_offset);
}

WebPreferences RenderViewHostImpl::GetWebkitPreferences() {
  if (!web_preferences_.get()) {
    OnWebkitPreferencesChanged();
  }
  return *web_preferences_;
}

void RenderViewHostImpl::UpdateWebkitPreferences(const WebPreferences& prefs) {
  web_preferences_.reset(new WebPreferences(prefs));
  Send(new ViewMsg_UpdateWebPreferences(GetRoutingID(), prefs));
}

void RenderViewHostImpl::OnWebkitPreferencesChanged() {
  // This is defensive code to avoid infinite loops due to code run inside
  // UpdateWebkitPreferences() accidentally updating more preferences and thus
  // calling back into this code. See crbug.com/398751 for one past example.
  if (updating_web_preferences_)
    return;
  updating_web_preferences_ = true;
  UpdateWebkitPreferences(ComputeWebkitPrefs());
  updating_web_preferences_ = false;
}

void RenderViewHostImpl::ClearFocusedElement() {
  is_focused_element_editable_ = false;
  Send(new ViewMsg_ClearFocusedElement(GetRoutingID()));
}

bool RenderViewHostImpl::IsFocusedElementEditable() {
  return is_focused_element_editable_;
}

void RenderViewHostImpl::Zoom(PageZoom zoom) {
  Send(new ViewMsg_Zoom(GetRoutingID(), zoom));
}

void RenderViewHostImpl::DisableScrollbarsForThreshold(const gfx::Size& size) {
  Send(new ViewMsg_DisableScrollbarsForSmallWindows(GetRoutingID(), size));
}

void RenderViewHostImpl::EnablePreferredSizeMode() {
  Send(new ViewMsg_EnablePreferredSizeChangedMode(GetRoutingID()));
}

void RenderViewHostImpl::EnableAutoResize(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  SetAutoResize(true, min_size, max_size);
  Send(new ViewMsg_EnableAutoResize(GetRoutingID(), min_size, max_size));
}

void RenderViewHostImpl::DisableAutoResize(const gfx::Size& new_size) {
  SetAutoResize(false, gfx::Size(), gfx::Size());
  Send(new ViewMsg_DisableAutoResize(GetRoutingID(), new_size));
  if (!new_size.IsEmpty())
    GetView()->SetSize(new_size);
}

void RenderViewHostImpl::CopyImageAt(int x, int y) {
  Send(new ViewMsg_CopyImageAt(GetRoutingID(), x, y));
}

void RenderViewHostImpl::SaveImageAt(int x, int y) {
  Send(new ViewMsg_SaveImageAt(GetRoutingID(), x, y));
}

void RenderViewHostImpl::ExecuteMediaPlayerActionAtLocation(
  const gfx::Point& location, const blink::WebMediaPlayerAction& action) {
  Send(new ViewMsg_MediaPlayerActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::ExecutePluginActionAtLocation(
  const gfx::Point& location, const blink::WebPluginAction& action) {
  Send(new ViewMsg_PluginActionAt(GetRoutingID(), location, action));
}

void RenderViewHostImpl::NotifyMoveOrResizeStarted() {
  Send(new ViewMsg_MoveOrResizeStarted(GetRoutingID()));
}

void RenderViewHostImpl::OnDidZoomURL(double zoom_level,
                                      const GURL& url) {
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));

  host_zoom_map->SetZoomLevelForView(GetProcess()->GetID(),
                                     GetRoutingID(),
                                     zoom_level,
                                     net::GetHostOrSpecFromURL(url));
}

void RenderViewHostImpl::OnPageScaleFactorIsOneChanged(bool is_one) {
  if (!GetSiteInstance())
    return;
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));
  if (!host_zoom_map)
    return;
  if (!GetProcess())
    return;
  host_zoom_map->SetPageScaleFactorIsOneForView(GetProcess()->GetID(),
                                                GetRoutingID(), is_one);
}

void RenderViewHostImpl::OnRunFileChooser(const FileChooserParams& params) {
  // Do not allow messages with absolute paths in them as this can permit a
  // renderer to coerce the browser to perform I/O on a renderer controlled
  // path.
  if (params.default_file_name != params.default_file_name.BaseName()) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RVH_FILE_CHOOSER_PATH);
    return;
  }

  delegate_->RunFileChooser(this, params);
}

void RenderViewHostImpl::OnFocusedNodeTouched(bool editable) {
#if defined(OS_WIN)
  if (editable) {
    virtual_keyboard_requested_ = base::win::DisplayVirtualKeyboard();
    delegate_->SetIsVirtualKeyboardRequested(true);
  } else {
    virtual_keyboard_requested_ = false;
    delegate_->SetIsVirtualKeyboardRequested(false);
    base::win::DismissVirtualKeyboard();
  }
#endif
}

bool RenderViewHostImpl::CanAccessFilesOfPageState(
    const PageState& state) const {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const std::vector<base::FilePath>& file_paths = state.GetReferencedFiles();
  for (const auto& file : file_paths) {
    if (!policy->CanReadFile(GetProcess()->GetID(), file))
      return false;
  }
  return true;
}

void RenderViewHostImpl::GrantFileAccessFromPageState(const PageState& state) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const std::vector<base::FilePath>& file_paths = state.GetReferencedFiles();
  for (const auto& file : file_paths) {
    if (!policy->CanReadFile(GetProcess()->GetID(), file))
      policy->GrantReadFile(GetProcess()->GetID(), file);
  }
}

void RenderViewHostImpl::SelectWordAroundCaret() {
  Send(new ViewMsg_SelectWordAroundCaret(GetRoutingID()));
}

}  // namespace content
