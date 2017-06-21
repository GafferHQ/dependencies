// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/numerics/safe_math.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_resize_helper.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_log_event.h"
#include "net/base/io_buffer.h"
#include "net/base/keygen_handler.h"
#include "net/base/mime_util.h"
#include "net/base/request_priority.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ui/gfx/color_profile.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#else
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#endif
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif
#if defined(OS_WIN)
#include "content/common/font_cache_dispatcher_win.h"
#include "content/common/sandbox_win.h"
#endif
#if defined(OS_ANDROID)
#include "media/base/android/webaudio_media_codec_bridge.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#endif

using net::CookieStore;

namespace content {
namespace {

#if defined(ENABLE_PLUGINS)
const int kPluginsRefreshThresholdInSeconds = 3;
#endif

const uint32 kFilteredMessageClasses[] = {
  ChildProcessMsgStart,
  FrameMsgStart,
  ViewMsgStart,
};

#if defined(OS_WIN)
// On Windows, |g_color_profile| can run on an arbitrary background thread.
// We avoid races by using LazyInstance's constructor lock to initialize the
// object.
base::LazyInstance<gfx::ColorProfile>::Leaky g_color_profile =
    LAZY_INSTANCE_INITIALIZER;
#endif

// Common functionality for converting a sync renderer message to a callback
// function in the browser. Derive from this, create it on the heap when
// issuing your callback. When done, write your reply parameters into
// reply_msg(), and then call SendReplyAndDeleteThis().
class RenderMessageCompletionCallback {
 public:
  RenderMessageCompletionCallback(RenderMessageFilter* filter,
                                  IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual ~RenderMessageCompletionCallback() {
    if (reply_msg_) {
      // If the owner of this class failed to call SendReplyAndDeleteThis(),
      // send an error reply to prevent the renderer from being hung.
      reply_msg_->set_reply_error();
      filter_->Send(reply_msg_);
    }
  }

  RenderMessageFilter* filter() { return filter_.get(); }
  IPC::Message* reply_msg() { return reply_msg_; }

  void SendReplyAndDeleteThis() {
    filter_->Send(reply_msg_);
    reply_msg_ = NULL;
    delete this;
  }

 private:
  scoped_refptr<RenderMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

#if defined(ENABLE_PLUGINS)
class OpenChannelToPpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PpapiPluginProcessHost::PluginClient {
 public:
  OpenChannelToPpapiPluginCallback(RenderMessageFilter* filter,
                                   ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context) {
  }

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    *renderer_handle = filter()->PeerHandle();
    *renderer_id = filter()->render_process_id();
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int plugin_child_id) override {
    ViewHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(
        reply_msg(), channel_handle, plugin_pid, plugin_child_id);
    SendReplyAndDeleteThis();
  }

  bool OffTheRecord() override { return filter()->OffTheRecord(); }

  ResourceContext* GetResourceContext() override { return context_; }

 private:
  ResourceContext* context_;
};

class OpenChannelToPpapiBrokerCallback
    : public PpapiPluginProcessHost::BrokerClient {
 public:
  OpenChannelToPpapiBrokerCallback(RenderMessageFilter* filter,
                                   int routing_id)
      : filter_(filter),
        routing_id_(routing_id) {
  }

  ~OpenChannelToPpapiBrokerCallback() override {}

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    *renderer_handle = filter_->PeerHandle();
    *renderer_id = filter_->render_process_id();
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int /* plugin_child_id */) override {
    filter_->Send(new ViewMsg_PpapiBrokerChannelCreated(routing_id_,
                                                        plugin_pid,
                                                        channel_handle));
    delete this;
  }

  bool OffTheRecord() override { return filter_->OffTheRecord(); }

 private:
  scoped_refptr<RenderMessageFilter> filter_;
  int routing_id_;
};
#endif  // defined(ENABLE_PLUGINS)

}  // namespace

class RenderMessageFilter::OpenChannelToNpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PluginProcessHost::Client {
 public:
  OpenChannelToNpapiPluginCallback(RenderMessageFilter* filter,
                                   ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context),
        host_(NULL),
        sent_plugin_channel_request_(false) {
  }

  int ID() override { return filter()->render_process_id(); }

  ResourceContext* GetResourceContext() override { return context_; }

  bool OffTheRecord() override {
    if (filter()->OffTheRecord())
      return true;
    if (GetContentClient()->browser()->AllowSaveLocalState(context_))
      return false;

    // For now, only disallow storing data for Flash <http://crbug.com/97319>.
    for (size_t i = 0; i < info_.mime_types.size(); ++i) {
      if (info_.mime_types[i].mime_type == kFlashPluginSwfMimeType)
        return true;
    }
    return false;
  }

  void SetPluginInfo(const WebPluginInfo& info) override { info_ = info; }

  void OnFoundPluginProcessHost(PluginProcessHost* host) override {
    DCHECK(host);
    host_ = host;
  }

  void OnSentPluginChannelRequest() override {
    sent_plugin_channel_request_ = true;
  }

  void OnChannelOpened(const IPC::ChannelHandle& handle) override {
    WriteReplyAndDeleteThis(handle);
  }

  void OnError() override { WriteReplyAndDeleteThis(IPC::ChannelHandle()); }

  PluginProcessHost* host() const {
    return host_;
  }

  bool sent_plugin_channel_request() const {
    return sent_plugin_channel_request_;
  }

  void Cancel() {
    delete this;
  }

 private:
  void WriteReplyAndDeleteThis(const IPC::ChannelHandle& handle) {
    FrameHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg(),
                                                       handle, info_);
    filter()->OnCompletedOpenChannelToNpapiPlugin(this);
    SendReplyAndDeleteThis();
  }

  ResourceContext* context_;
  WebPluginInfo info_;
  PluginProcessHost* host_;
  bool sent_plugin_channel_request_;
};

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    PluginServiceImpl* plugin_service,
    BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper,
    media::AudioManager* audio_manager,
    MediaInternals* media_internals,
    DOMStorageContextWrapper* dom_storage_context)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      resource_dispatcher_host_(ResourceDispatcherHostImpl::Get()),
      plugin_service_(plugin_service),
      profile_data_directory_(browser_context->GetPath()),
      bitmap_manager_client_(HostSharedBitmapManager::current()),
      request_context_(request_context),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      incognito_(browser_context->IsOffTheRecord()),
      dom_storage_context_(dom_storage_context),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      media_internals_(media_internals) {
  DCHECK(request_context_.get());

  if (render_widget_helper)
    render_widget_helper_->Init(render_process_id_, resource_dispatcher_host_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(plugin_host_clients_.empty());
  BrowserGpuMemoryBufferManager* gpu_memory_buffer_manager =
      BrowserGpuMemoryBufferManager::current();
  if (gpu_memory_buffer_manager)
    gpu_memory_buffer_manager->ProcessRemoved(PeerHandle(), render_process_id_);
  HostDiscardableSharedMemoryManager::current()->ProcessRemoved(
      render_process_id_);
}

void RenderMessageFilter::OnChannelClosing() {
#if defined(ENABLE_PLUGINS)
  for (std::set<OpenChannelToNpapiPluginCallback*>::iterator it =
       plugin_host_clients_.begin(); it != plugin_host_clients_.end(); ++it) {
    OpenChannelToNpapiPluginCallback* client = *it;
    if (client->host()) {
      if (client->sent_plugin_channel_request()) {
        client->host()->CancelSentRequest(client);
      } else {
        client->host()->CancelPendingRequest(client);
      }
    } else {
      plugin_service_->CancelOpenChannelToNpapiPlugin(client);
    }
    client->Cancel();
  }
#endif  // defined(ENABLE_PLUGINS)
  plugin_host_clients_.clear();
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderMessageFilter, message)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PreCacheFontCharacters,
                        OnPreCacheFontCharacters)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetProcessMemorySizes,
                        OnGetProcessMemorySizes)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GenerateRoutingID, OnGenerateRoutingID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateFullscreenWidget,
                        OnCreateFullscreenWidget)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CookiesEnabled, OnCookiesEnabled)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_LoadFont, OnLoadFont)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SaveImageFromDataURL,
                        OnSaveImageFromDataURL)
#if defined(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER(FrameHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidCreateOutOfProcessPepperInstance,
                        OnDidCreateOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDeleteOutOfProcessPepperInstance,
                        OnDidDeleteOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToPpapiBroker,
                        OnOpenChannelToPpapiBroker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PluginInstanceThrottleStateChange,
                        OnPluginInstanceThrottleStateChange)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_SwapCompositorFrame,
        RenderWidgetResizeHelper::Get()->PostRendererProcessMsg(
            render_process_id_, message))
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_UpdateRect,
        RenderWidgetResizeHelper::Get()->PostRendererProcessMsg(
            render_process_id_, message))
#endif
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateSharedMemory, OnAllocateSharedMemory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateSharedBitmap, OnAllocateSharedBitmap)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer,
        OnAllocateGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DeletedGpuMemoryBuffer,
                        OnDeletedGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_AllocatedSharedBitmap,
                        OnAllocatedSharedBitmap)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DeletedSharedBitmap,
                        OnDeletedSharedBitmap)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateLockedDiscardableSharedMemory,
        OnAllocateLockedDiscardableSharedMemory)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DeletedDiscardableSharedMemory,
                        OnDeletedDiscardableSharedMemory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidGenerateCacheableMetadata,
                        OnCacheableMetadataAvailable)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_Keygen, OnKeygen)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioHardwareConfig,
                        OnGetAudioHardwareConfig)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetMonitorColorProfile,
                        OnGetMonitorColorProfile)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaLogEvents, OnMediaLogEvents)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Are3DAPIsBlocked, OnAre3DAPIsBlocked)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidLose3DContext, OnDidLose3DContext)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RunWebAudioMediaCodec, OnWebAudioMediaCodec)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderMessageFilter::OverrideThreadForMessage(const IPC::Message& message,
                                                   BrowserThread::ID* thread) {
  if (message.type() == ViewHostMsg_MediaLogEvents::ID)
    *thread = BrowserThread::UI;
}

base::TaskRunner* RenderMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
#if defined(OS_WIN)
  // Windows monitor profile must be read from a file.
  if (message.type() == ViewHostMsg_GetMonitorColorProfile::ID)
    return BrowserThread::GetBlockingPool();
#endif
  // Always query audio device parameters on the audio thread.
  if (message.type() == ViewHostMsg_GetAudioHardwareConfig::ID)
    return audio_manager_->GetTaskRunner().get();
  return NULL;
}

bool RenderMessageFilter::OffTheRecord() const {
  return incognito_;
}

void RenderMessageFilter::OnCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id,
    int* main_frame_route_id,
    int* surface_id,
    int64* cloned_session_storage_namespace_id) {
  bool no_javascript_access;

  // Merge the additional features into the WebWindowFeatures struct before we
  // pass it on.
  blink::WebVector<blink::WebString> additional_features(
      params.additional_features.size());

  for (size_t i = 0; i < params.additional_features.size(); ++i)
    additional_features[i] = blink::WebString(params.additional_features[i]);

  blink::WebWindowFeatures features = params.features;
  features.additionalFeatures.swap(additional_features);

  bool can_create_window =
      GetContentClient()->browser()->CanCreateWindow(
          params.opener_url,
          params.opener_top_level_frame_url,
          params.opener_security_origin,
          params.window_container_type,
          params.target_url,
          params.referrer,
          params.disposition,
          features,
          params.user_gesture,
          params.opener_suppressed,
          resource_context_,
          render_process_id_,
          params.opener_id,
          params.opener_render_frame_id,
          &no_javascript_access);

  if (!can_create_window) {
    *route_id = MSG_ROUTING_NONE;
    *main_frame_route_id = MSG_ROUTING_NONE;
    *surface_id = 0;
    *cloned_session_storage_namespace_id = 0;
    return;
  }

  // This will clone the sessionStorage for namespace_id_to_clone.
  scoped_refptr<SessionStorageNamespaceImpl> cloned_namespace =
      new SessionStorageNamespaceImpl(dom_storage_context_.get(),
                                      params.session_storage_namespace_id);
  *cloned_session_storage_namespace_id = cloned_namespace->id();

  render_widget_helper_->CreateNewWindow(params,
                                         no_javascript_access,
                                         PeerHandle(),
                                         route_id,
                                         main_frame_route_id,
                                         surface_id,
                                         cloned_namespace.get());
}

void RenderMessageFilter::OnCreateWidget(int opener_id,
                                         blink::WebPopupType popup_type,
                                         int* route_id,
                                         int* surface_id) {
  render_widget_helper_->CreateNewWidget(
      opener_id, popup_type, route_id, surface_id);
}

void RenderMessageFilter::OnCreateFullscreenWidget(int opener_id,
                                                   int* route_id,
                                                   int* surface_id) {
  render_widget_helper_->CreateNewFullscreenWidget(
      opener_id, route_id, surface_id);
}

void RenderMessageFilter::OnGetProcessMemorySizes(size_t* private_bytes,
                                                  size_t* shared_bytes) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  using base::ProcessMetrics;
#if !defined(OS_MACOSX) || defined(OS_IOS)
  scoped_ptr<ProcessMetrics> metrics(ProcessMetrics::CreateProcessMetrics(
      PeerHandle()));
#else
  scoped_ptr<ProcessMetrics> metrics(ProcessMetrics::CreateProcessMetrics(
      PeerHandle(), BrowserChildProcessHost::GetPortProvider()));
#endif
  if (!metrics->GetMemoryBytes(private_bytes, shared_bytes)) {
    *private_bytes = 0;
    *shared_bytes = 0;
  }
}

void RenderMessageFilter::OnSetCookie(int render_frame_id,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& cookie) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(render_process_id_, url)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::RMF_SET_COOKIE_BAD_ORIGIN);
    return;
  }

  net::CookieOptions options;
  if (GetContentClient()->browser()->AllowSetCookie(
          url, first_party_for_cookies, cookie, resource_context_,
          render_process_id_, render_frame_id, &options)) {
    net::URLRequestContext* context = GetRequestContextForURL(url);
    // Pass a null callback since we don't care about when the 'set' completes.
    context->cookie_store()->SetCookieWithOptionsAsync(
        url, cookie, options, net::CookieStore::SetCookiesCallback());
  }
}

void RenderMessageFilter::OnGetCookies(int render_frame_id,
                                       const GURL& url,
                                       const GURL& first_party_for_cookies,
                                       IPC::Message* reply_msg) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(render_process_id_, url)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::RMF_GET_COOKIES_BAD_ORIGIN);
    delete reply_msg;
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/99242
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  net::URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->GetAllCookiesForURLAsync(
      url, base::Bind(&RenderMessageFilter::CheckPolicyForCookies, this,
                      render_frame_id, url, first_party_for_cookies,
                      reply_msg));
}

void RenderMessageFilter::OnCookiesEnabled(
    int render_frame_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool* cookies_enabled) {
  // TODO(ananta): If this render frame is associated with an automation
  // channel, aka ChromeFrame then we need to retrieve cookie settings from the
  // external host.
  *cookies_enabled = GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, net::CookieList(), resource_context_,
      render_process_id_, render_frame_id);
}

#if defined(OS_MACOSX)
void RenderMessageFilter::OnLoadFont(const FontDescriptor& font,
                                     IPC::Message* reply_msg) {
  FontLoader::Result* result = new FontLoader::Result;

  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FontLoader::LoadFont, font, result),
      base::Bind(&RenderMessageFilter::SendLoadFontReply, this, reply_msg,
                 base::Owned(result)));
}

void RenderMessageFilter::SendLoadFontReply(IPC::Message* reply,
                                            FontLoader::Result* result) {
  base::SharedMemoryHandle handle;
  if (result->font_data_size == 0 || result->font_id == 0) {
    result->font_data_size = 0;
    result->font_id = 0;
    handle = base::SharedMemory::NULLHandle();
  } else {
    result->font_data.GiveToProcess(base::GetCurrentProcessHandle(), &handle);
  }
  ViewHostMsg_LoadFont::WriteReplyParams(
      reply, result->font_data_size, handle, result->font_id);
  Send(reply);
}
#endif  // OS_MACOSX

#if defined(ENABLE_PLUGINS)
void RenderMessageFilter::OnGetPlugins(
    bool refresh,
    IPC::Message* reply_msg) {
  // Don't refresh if the specified threshold has not been passed.  Note that
  // this check is performed before off-loading to the file thread.  The reason
  // we do this is that some pages tend to request that the list of plugins be
  // refreshed at an excessive rate.  This instigates disk scanning, as the list
  // is accumulated by doing multiple reads from disk.  This effect is
  // multiplied when we have several pages requesting this operation.
  if (refresh) {
    const base::TimeDelta threshold = base::TimeDelta::FromSeconds(
        kPluginsRefreshThresholdInSeconds);
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_plugin_refresh_time_ >= threshold) {
      // Only refresh if the threshold hasn't been exceeded yet.
      PluginServiceImpl::GetInstance()->RefreshPlugins();
      last_plugin_refresh_time_ = now;
    }
  }

  PluginServiceImpl::GetInstance()->GetPlugins(
      base::Bind(&RenderMessageFilter::GetPluginsCallback, this, reply_msg));
}

void RenderMessageFilter::GetPluginsCallback(
    IPC::Message* reply_msg,
    const std::vector<WebPluginInfo>& all_plugins) {
  // Filter the plugin list.
  PluginServiceFilter* filter = PluginServiceImpl::GetInstance()->GetFilter();
  std::vector<WebPluginInfo> plugins;

  int child_process_id = -1;
  int routing_id = MSG_ROUTING_NONE;
  for (size_t i = 0; i < all_plugins.size(); ++i) {
    // Copy because the filter can mutate.
    WebPluginInfo plugin(all_plugins[i]);
    if (!filter || filter->IsPluginAvailable(child_process_id,
                                             routing_id,
                                             resource_context_,
                                             GURL(),
                                             GURL(),
                                             &plugin)) {
      plugins.push_back(plugin);
    }
  }

  ViewHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  Send(reply_msg);
}

void RenderMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const GURL& page_url,
    const std::string& mime_type,
    bool* found,
    WebPluginInfo* info,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, render_frame_id, resource_context_,
      url, page_url, mime_type, allow_wildcard,
      NULL, info, actual_mime_type);
}

void RenderMessageFilter::OnOpenChannelToPlugin(int render_frame_id,
                                                const GURL& url,
                                                const GURL& policy_url,
                                                const std::string& mime_type,
                                                IPC::Message* reply_msg) {
  OpenChannelToNpapiPluginCallback* client =
      new OpenChannelToNpapiPluginCallback(this, resource_context_, reply_msg);
  DCHECK(!ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.insert(client);
  plugin_service_->OpenChannelToNpapiPlugin(
      render_process_id_, render_frame_id,
      url, policy_url, mime_type, client);
}

void RenderMessageFilter::OnOpenChannelToPepperPlugin(
    const base::FilePath& path,
    IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPpapiPlugin(
      render_process_id_,
      path,
      profile_data_directory_,
      new OpenChannelToPpapiPluginCallback(this, resource_context_, reply_msg));
}

void RenderMessageFilter::OnDidCreateOutOfProcessPepperInstance(
    int plugin_child_id,
    int32 pp_instance,
    PepperRendererInstanceData instance_data,
    bool is_external) {
  // It's important that we supply the render process ID ourselves based on the
  // channel the message arrived on. We use the
  //   PP_Instance -> (process id, view id)
  // mapping to decide how to handle messages received from the (untrusted)
  // plugin, so an exploited renderer must not be able to insert fake mappings
  // that may allow it access to other render processes.
  DCHECK_EQ(0, instance_data.render_process_id);
  instance_data.render_process_id = render_process_id_;
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->AddInstance(pp_instance, instance_data);
  } else {
    PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
        plugin_child_id, pp_instance, instance_data);
  }
}

void RenderMessageFilter::OnDidDeleteOutOfProcessPepperInstance(
    int plugin_child_id,
    int32 pp_instance,
    bool is_external) {
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->DeleteInstance(pp_instance);
  } else {
    PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(
        plugin_child_id, pp_instance);
  }
}

void RenderMessageFilter::OnOpenChannelToPpapiBroker(
    int routing_id,
    const base::FilePath& path) {
  plugin_service_->OpenChannelToPpapiBroker(
      render_process_id_,
      path,
      new OpenChannelToPpapiBrokerCallback(this, routing_id));
}

void RenderMessageFilter::OnPluginInstanceThrottleStateChange(
    int plugin_child_id,
    int32 pp_instance,
    bool is_throttled) {
  // Feature is only implemented for non-external Plugins.
  PpapiPluginProcessHost::OnPluginInstanceThrottleStateChange(
      plugin_child_id, pp_instance, is_throttled);
}
#endif  // defined(ENABLE_PLUGINS)

void RenderMessageFilter::OnGenerateRoutingID(int* route_id) {
  *route_id = render_widget_helper_->GetNextRoutingID();
}

void RenderMessageFilter::OnGetAudioHardwareConfig(
    media::AudioParameters* input_params,
    media::AudioParameters* output_params) {
  DCHECK(input_params);
  DCHECK(output_params);
  *output_params = audio_manager_->GetDefaultOutputStreamParameters();

  // TODO(henrika): add support for all available input devices.
  *input_params = audio_manager_->GetInputStreamParameters(
      media::AudioManagerBase::kDefaultDeviceId);
}

#if defined(OS_WIN)
void RenderMessageFilter::OnGetMonitorColorProfile(std::vector<char>* profile) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  *profile = g_color_profile.Get().profile();
}
#endif

void RenderMessageFilter::DownloadUrl(int render_view_id,
                                      const GURL& url,
                                      const Referrer& referrer,
                                      const base::string16& suggested_name,
                                      const bool use_prompt) const {
  scoped_ptr<DownloadSaveInfo> save_info(new DownloadSaveInfo());
  save_info->suggested_name = suggested_name;
  save_info->prompt_for_save_location = use_prompt;
  scoped_ptr<net::URLRequest> request(
      resource_context_->GetRequestContext()->CreateRequest(
          url, net::DEFAULT_PRIORITY, NULL));
  RecordDownloadSource(INITIATED_BY_RENDERER);
  resource_dispatcher_host_->BeginDownload(
      request.Pass(),
      referrer,
      true,  // is_content_initiated
      resource_context_,
      render_process_id_,
      render_view_id,
      false,
      false,
      save_info.Pass(),
      DownloadItem::kInvalidId,
      ResourceDispatcherHostImpl::DownloadStartedCallback());
}

void RenderMessageFilter::OnDownloadUrl(int render_view_id,
                                        const GURL& url,
                                        const Referrer& referrer,
                                        const base::string16& suggested_name) {
  DownloadUrl(render_view_id, url, referrer, suggested_name, false);
}

void RenderMessageFilter::OnSaveImageFromDataURL(int render_view_id,
                                                 const std::string& url_str) {
  // Please refer to RenderViewImpl::saveImageFromDataURL().
  if (url_str.length() >= kMaxLengthOfDataURLString)
    return;

  GURL data_url(url_str);
  if (!data_url.SchemeIs(url::kDataScheme))
    return;

  DownloadUrl(render_view_id, data_url, Referrer(), base::string16(), true);
}

void RenderMessageFilter::AllocateSharedMemoryOnFileThread(
    uint32 buffer_size,
    IPC::Message* reply_msg) {
  base::SharedMemoryHandle handle;
  ChildProcessHostImpl::AllocateSharedMemory(buffer_size, PeerHandle(),
                                             &handle);
  ChildProcessHostMsg_SyncAllocateSharedMemory::WriteReplyParams(reply_msg,
                                                                 handle);
  Send(reply_msg);
}

void RenderMessageFilter::OnAllocateSharedMemory(uint32 buffer_size,
                                                 IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&RenderMessageFilter::AllocateSharedMemoryOnFileThread, this,
                 buffer_size, reply_msg));
}

void RenderMessageFilter::AllocateSharedBitmapOnFileThread(
    uint32 buffer_size,
    const cc::SharedBitmapId& id,
    IPC::Message* reply_msg) {
  base::SharedMemoryHandle handle;
  bitmap_manager_client_.AllocateSharedBitmapForChild(PeerHandle(), buffer_size,
                                                      id, &handle);
  ChildProcessHostMsg_SyncAllocateSharedBitmap::WriteReplyParams(reply_msg,
                                                                 handle);
  Send(reply_msg);
}

void RenderMessageFilter::OnAllocateSharedBitmap(uint32 buffer_size,
                                                 const cc::SharedBitmapId& id,
                                                 IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING,
      FROM_HERE,
      base::Bind(&RenderMessageFilter::AllocateSharedBitmapOnFileThread,
                 this,
                 buffer_size,
                 id,
                 reply_msg));
}

void RenderMessageFilter::OnAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    const cc::SharedBitmapId& id) {
  bitmap_manager_client_.ChildAllocatedSharedBitmap(buffer_size, handle,
                                                    PeerHandle(), id);
}

void RenderMessageFilter::OnDeletedSharedBitmap(const cc::SharedBitmapId& id) {
  bitmap_manager_client_.ChildDeletedSharedBitmap(id);
}

void RenderMessageFilter::AllocateLockedDiscardableSharedMemoryOnFileThread(
    uint32 size,
    DiscardableSharedMemoryId id,
    IPC::Message* reply_msg) {
  base::SharedMemoryHandle handle;
  HostDiscardableSharedMemoryManager::current()
      ->AllocateLockedDiscardableSharedMemoryForChild(
          PeerHandle(), render_process_id_, size, id, &handle);
  ChildProcessHostMsg_SyncAllocateLockedDiscardableSharedMemory::
      WriteReplyParams(reply_msg, handle);
  Send(reply_msg);
}

void RenderMessageFilter::OnAllocateLockedDiscardableSharedMemory(
    uint32 size,
    DiscardableSharedMemoryId id,
    IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&RenderMessageFilter::
                     AllocateLockedDiscardableSharedMemoryOnFileThread,
                 this, size, id, reply_msg));
}

void RenderMessageFilter::DeletedDiscardableSharedMemoryOnFileThread(
    DiscardableSharedMemoryId id) {
  HostDiscardableSharedMemoryManager::current()
      ->ChildDeletedDiscardableSharedMemory(id, render_process_id_);
}

void RenderMessageFilter::OnDeletedDiscardableSharedMemory(
    DiscardableSharedMemoryId id) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(
          &RenderMessageFilter::DeletedDiscardableSharedMemoryOnFileThread,
          this, id));
}

net::URLRequestContext* RenderMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::URLRequestContext* context =
      GetContentClient()->browser()->OverrideRequestContextForURL(
          url, resource_context_);
  if (!context)
    context = request_context_->GetURLRequestContext();

  return context;
}

void RenderMessageFilter::OnCacheableMetadataAvailable(
    const GURL& url,
    base::Time expected_response_time,
    const std::vector<char>& data) {
  net::HttpCache* cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  if (!cache)
    return;

  // Use the same priority for the metadata write as for script
  // resources (see defaultPriorityForResourceType() in WebKit's
  // CachedResource.cpp). Note that WebURLRequest::PriorityMedium
  // corresponds to net::LOW (see ConvertWebKitPriorityToNetPriority()
  // in weburlloader_impl.cc).
  const net::RequestPriority kPriority = net::LOW;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(data.size()));
  memcpy(buf->data(), &data.front(), data.size());
  cache->WriteMetadata(url, kPriority, expected_response_time, buf.get(),
                       data.size());
}

void RenderMessageFilter::OnKeygen(uint32 key_size_index,
                                   const std::string& challenge_string,
                                   const GURL& url,
                                   IPC::Message* reply_msg) {
  // Map displayed strings indicating level of keysecurity in the <keygen>
  // menu to the key size in bits. (See SSLKeyGeneratorChromium.cpp in WebCore.)
  int key_size_in_bits;
  switch (key_size_index) {
    case 0:
      key_size_in_bits = 2048;
      break;
    case 1:
      key_size_in_bits = 1024;
      break;
    default:
      DCHECK(false) << "Illegal key_size_index " << key_size_index;
      ViewHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
      Send(reply_msg);
      return;
  }

  resource_context_->CreateKeygenHandler(
      key_size_in_bits,
      challenge_string,
      url,
      base::Bind(
          &RenderMessageFilter::PostKeygenToWorkerThread, this, reply_msg));
}

void RenderMessageFilter::PostKeygenToWorkerThread(
    IPC::Message* reply_msg,
    scoped_ptr<net::KeygenHandler> keygen_handler) {
  VLOG(1) << "Dispatching keygen task to worker pool.";
  // Dispatch to worker pool, so we do not block the IO thread.
  if (!base::WorkerPool::PostTask(
           FROM_HERE,
           base::Bind(&RenderMessageFilter::OnKeygenOnWorkerThread,
                      this,
                      base::Passed(&keygen_handler),
                      reply_msg),
           true)) {
    NOTREACHED() << "Failed to dispatch keygen task to worker pool";
    ViewHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
    Send(reply_msg);
  }
}

void RenderMessageFilter::OnKeygenOnWorkerThread(
    scoped_ptr<net::KeygenHandler> keygen_handler,
    IPC::Message* reply_msg) {
  DCHECK(reply_msg);

  // Generate a signed public key and challenge, then send it back.
  ViewHostMsg_Keygen::WriteReplyParams(
      reply_msg,
      keygen_handler->GenKeyAndSignChallenge());
  Send(reply_msg);
}

void RenderMessageFilter::OnMediaLogEvents(
    const std::vector<media::MediaLogEvent>& events) {
  // OnMediaLogEvents() is always dispatched to the UI thread for handling.
  // See OverrideThreadForMessage().
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (media_internals_)
    media_internals_->OnMediaEvents(render_process_id_, events);
}

void RenderMessageFilter::CheckPolicyForCookies(
    int render_frame_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg,
    const net::CookieList& cookie_list) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  // Check the policy for get cookies, and pass cookie_list to the
  // TabSpecificContentSetting for logging purpose.
  if (GetContentClient()->browser()->AllowGetCookie(
          url, first_party_for_cookies, cookie_list, resource_context_,
          render_process_id_, render_frame_id)) {
    // Gets the cookies from cookie store if allowed.
    context->cookie_store()->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(),
        base::Bind(&RenderMessageFilter::SendGetCookiesResponse,
                   this, reply_msg));
  } else {
    SendGetCookiesResponse(reply_msg, std::string());
  }
}

void RenderMessageFilter::SendGetCookiesResponse(IPC::Message* reply_msg,
                                                 const std::string& cookies) {
  FrameHostMsg_GetCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderMessageFilter::OnCompletedOpenChannelToNpapiPlugin(
    OpenChannelToNpapiPluginCallback* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.erase(client);
}

void RenderMessageFilter::OnAre3DAPIsBlocked(int render_view_id,
                                             const GURL& top_origin_url,
                                             ThreeDAPIType requester,
                                             bool* blocked) {
  *blocked = GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      top_origin_url, render_process_id_, render_view_id, requester);
}

void RenderMessageFilter::OnDidLose3DContext(
    const GURL& top_origin_url,
    ThreeDAPIType /* unused */,
    int arb_robustness_status_code) {
#if defined(OS_MACOSX)
    // TODO(kbr): this file indirectly includes npapi.h, which on Mac
    // OS pulls in the system OpenGL headers. For some
    // not-yet-investigated reason this breaks the build with the 10.6
    // SDK but not 10.7. For now work around this in a way compatible
    // with the Khronos headers.
#ifndef GL_GUILTY_CONTEXT_RESET_ARB
#define GL_GUILTY_CONTEXT_RESET_ARB 0x8253
#endif
#ifndef GL_INNOCENT_CONTEXT_RESET_ARB
#define GL_INNOCENT_CONTEXT_RESET_ARB 0x8254
#endif
#ifndef GL_UNKNOWN_CONTEXT_RESET_ARB
#define GL_UNKNOWN_CONTEXT_RESET_ARB 0x8255
#endif

#endif
  GpuDataManagerImpl::DomainGuilt guilt;
  switch (arb_robustness_status_code) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_KNOWN;
      break;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN;
      break;
    default:
      // Ignore lost contexts known to be innocent.
      return;
  }

  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(
      top_origin_url, guilt);
}

#if defined(OS_WIN)
void RenderMessageFilter::OnPreCacheFontCharacters(const LOGFONT& font,
                                                   const base::string16& str) {
  // TODO(scottmg): pdf/ppapi still require the renderer to be able to precache
  // GDI fonts (http://crbug.com/383227), even when using DirectWrite.
  // Eventually this shouldn't be added and should be moved to
  // FontCacheDispatcher too. http://crbug.com/356346.

  // First, comments from FontCacheDispatcher::OnPreCacheFont do apply here too.
  // Except that for True Type fonts,
  // GetTextMetrics will not load the font in memory.
  // The only way windows seem to load properly, it is to create a similar
  // device (like the one in which we print), then do an ExtTextOut,
  // as we do in the printing thread, which is sandboxed.
  HDC hdc = CreateEnhMetaFile(NULL, NULL, NULL, NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  ExtTextOut(hdc, 0, 0, ETO_GLYPH_INDEX, 0, str.c_str(), str.length(), NULL);

  SelectObject(hdc, old_font);
  DeleteObject(font_handle);

  HENHMETAFILE metafile = CloseEnhMetaFile(hdc);

  if (metafile) {
    DeleteEnhMetaFile(metafile);
  }
}
#endif

#if defined(OS_ANDROID)
void RenderMessageFilter::OnWebAudioMediaCodec(
    base::SharedMemoryHandle encoded_data_handle,
    base::FileDescriptor pcm_output,
    uint32_t data_size) {
  // Let a WorkerPool handle this request since the WebAudio
  // MediaCodec bridge is slow and can block while sending the data to
  // the renderer.
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&media::WebAudioMediaCodecBridge::RunWebAudioMediaCodec,
                 encoded_data_handle, pcm_output, data_size),
      true);
}
#endif

void RenderMessageFilter::OnAllocateGpuMemoryBuffer(
    uint32 width,
    uint32 height,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    IPC::Message* reply) {
  DCHECK(BrowserGpuMemoryBufferManager::current());

  base::CheckedNumeric<int> size = width;
  size *= height;
  if (!size.IsValid()) {
    GpuMemoryBufferAllocated(reply, gfx::GpuMemoryBufferHandle());
    return;
  }

  BrowserGpuMemoryBufferManager::current()
      ->AllocateGpuMemoryBufferForChildProcess(
          gfx::Size(width, height),
          format,
          usage,
          PeerHandle(),
          render_process_id_,
          base::Bind(
              &RenderMessageFilter::GpuMemoryBufferAllocated, this, reply));
}

void RenderMessageFilter::GpuMemoryBufferAllocated(
    IPC::Message* reply,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer::WriteReplyParams(reply,
                                                                    handle);
  Send(reply);
}

void RenderMessageFilter::OnDeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    uint32 sync_point) {
  DCHECK(BrowserGpuMemoryBufferManager::current());

  BrowserGpuMemoryBufferManager::current()->ChildProcessDeletedGpuMemoryBuffer(
      id, PeerHandle(), render_process_id_, sync_point);
}

}  // namespace content
