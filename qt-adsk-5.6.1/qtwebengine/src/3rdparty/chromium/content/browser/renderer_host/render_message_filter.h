// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "content/common/host_discardable_shared_memory_manager.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/three_d_api_types.h"
#include "ipc/message_filter.h"
#include "media/audio/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_MACOSX)
#include <IOSurface/IOSurfaceAPI.h>
#include "base/mac/scoped_cftyperef.h"
#include "content/common/mac/font_loader.h"
#endif

#if defined(OS_ANDROID)
#include "base/threading/worker_pool.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/common/pepper_renderer_instance_data.h"
#endif

struct FontDescriptor;
struct ViewHostMsg_CreateWindow_Params;

namespace blink {
struct WebScreenInfo;
}

namespace base {
class ProcessMetrics;
class SharedMemory;
class TaskRunner;
}

namespace gfx {
struct GpuMemoryBufferHandle;
}

namespace media {
class AudioManager;
struct MediaLogEvent;
}

namespace net {
class KeygenHandler;
class URLRequestContext;
class URLRequestContextGetter;
}

namespace content {
class BrowserContext;
class DOMStorageContextWrapper;
class MediaInternals;
class PluginServiceImpl;
class RenderWidgetHelper;
class ResourceContext;
class ResourceDispatcherHostImpl;
struct Referrer;
struct WebPluginInfo;

// This class filters out incoming IPC messages for the renderer process on the
// IPC thread.
class CONTENT_EXPORT RenderMessageFilter : public BrowserMessageFilter {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      PluginServiceImpl * plugin_service,
                      BrowserContext* browser_context,
                      net::URLRequestContextGetter* request_context,
                      RenderWidgetHelper* render_widget_helper,
                      media::AudioManager* audio_manager,
                      MediaInternals* media_internals,
                      DOMStorageContextWrapper* dom_storage_context);

  // IPC::MessageFilter methods:
  void OnChannelClosing() override;

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;

  bool OffTheRecord() const;

  int render_process_id() const { return render_process_id_; }

  // Returns the correct net::URLRequestContext depending on what type of url is
  // given.
  // Only call on the IO thread.
  net::URLRequestContext* GetRequestContextForURL(const GURL& url);

 protected:
  ~RenderMessageFilter() override;

  // This method will be overridden by TestSaveImageFromDataURL class for test.
  virtual void DownloadUrl(int render_view_id,
                           const GURL& url,
                           const Referrer& referrer,
                           const base::string16& suggested_name,
                           const bool use_prompt) const;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<RenderMessageFilter>;

  class OpenChannelToNpapiPluginCallback;

  void OnGetProcessMemorySizes(size_t* private_bytes, size_t* shared_bytes);
  void OnCreateWindow(const ViewHostMsg_CreateWindow_Params& params,
                      int* route_id,
                      int* main_frame_route_id,
                      int* surface_id,
                      int64* cloned_session_storage_namespace_id);
  void OnCreateWidget(int opener_id,
                      blink::WebPopupType popup_type,
                      int* route_id,
                      int* surface_id);
  void OnCreateFullscreenWidget(int opener_id,
                                int* route_id,
                                int* surface_id);
  void OnSetCookie(int render_frame_id,
                   const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);
  void OnGetCookies(int render_frame_id,
                    const GURL& url,
                    const GURL& first_party_for_cookies,
                    IPC::Message* reply_msg);
  void OnCookiesEnabled(int render_frame_id,
                        const GURL& url,
                        const GURL& first_party_for_cookies,
                        bool* cookies_enabled);

#if defined(OS_MACOSX)
  // Messages for OOP font loading.
  void OnLoadFont(const FontDescriptor& font, IPC::Message* reply_msg);
  void SendLoadFontReply(IPC::Message* reply, FontLoader::Result* result);
#endif

#if defined(OS_WIN)
  void OnPreCacheFontCharacters(const LOGFONT& log_font,
                                const base::string16& characters);
#endif

#if defined(ENABLE_PLUGINS)
  void OnGetPlugins(bool refresh, IPC::Message* reply_msg);
  void GetPluginsCallback(IPC::Message* reply_msg,
                          const std::vector<WebPluginInfo>& plugins);
  void OnGetPluginInfo(int render_frame_id,
                       const GURL& url,
                       const GURL& policy_url,
                       const std::string& mime_type,
                       bool* found,
                       WebPluginInfo* info,
                       std::string* actual_mime_type);
  void OnOpenChannelToPlugin(int render_frame_id,
                             const GURL& url,
                             const GURL& policy_url,
                             const std::string& mime_type,
                             IPC::Message* reply_msg);
  void OnOpenChannelToPepperPlugin(const base::FilePath& path,
                                   IPC::Message* reply_msg);
  void OnDidCreateOutOfProcessPepperInstance(
      int plugin_child_id,
      int32 pp_instance,
      PepperRendererInstanceData instance_data,
      bool is_external);
  void OnDidDeleteOutOfProcessPepperInstance(int plugin_child_id,
                                             int32 pp_instance,
                                             bool is_external);
  void OnOpenChannelToPpapiBroker(int routing_id,
                                  const base::FilePath& path);
  void OnPluginInstanceThrottleStateChange(int plugin_child_id,
                                           int32 pp_instance,
                                           bool is_throttled);
#endif  // defined(ENABLE_PLUGINS)
  void OnGenerateRoutingID(int* route_id);
  void OnDownloadUrl(int render_view_id,
                     const GURL& url,
                     const Referrer& referrer,
                     const base::string16& suggested_name);
  void OnSaveImageFromDataURL(int render_view_id, const std::string& url_str);

  void OnGetAudioHardwareConfig(media::AudioParameters* input_params,
                                media::AudioParameters* output_params);

#if defined(OS_WIN)
  // Used to look up the monitor color profile.
  void OnGetMonitorColorProfile(std::vector<char>* profile);
#endif

  // Used to ask the browser to allocate a block of shared memory for the
  // renderer to send back data in, since shared memory can't be created
  // in the renderer on POSIX due to the sandbox.
  void AllocateSharedMemoryOnFileThread(uint32 buffer_size,
                                        IPC::Message* reply_msg);
  void OnAllocateSharedMemory(uint32 buffer_size, IPC::Message* reply_msg);
  void AllocateSharedBitmapOnFileThread(uint32 buffer_size,
                                        const cc::SharedBitmapId& id,
                                        IPC::Message* reply_msg);
  void OnAllocateSharedBitmap(uint32 buffer_size,
                              const cc::SharedBitmapId& id,
                              IPC::Message* reply_msg);
  void OnAllocatedSharedBitmap(size_t buffer_size,
                               const base::SharedMemoryHandle& handle,
                               const cc::SharedBitmapId& id);
  void OnDeletedSharedBitmap(const cc::SharedBitmapId& id);
  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

  // Browser side discardable shared memory allocation.
  void AllocateLockedDiscardableSharedMemoryOnFileThread(
      uint32 size,
      DiscardableSharedMemoryId id,
      IPC::Message* reply_message);
  void OnAllocateLockedDiscardableSharedMemory(uint32 size,
                                               DiscardableSharedMemoryId id,
                                               IPC::Message* reply_message);
  void DeletedDiscardableSharedMemoryOnFileThread(DiscardableSharedMemoryId id);
  void OnDeletedDiscardableSharedMemory(DiscardableSharedMemoryId id);

  void OnCacheableMetadataAvailable(const GURL& url,
                                    base::Time expected_response_time,
                                    const std::vector<char>& data);
  void OnKeygen(uint32 key_size_index, const std::string& challenge_string,
                const GURL& url, IPC::Message* reply_msg);
  void PostKeygenToWorkerThread(IPC::Message* reply_msg,
                                scoped_ptr<net::KeygenHandler> keygen_handler);
  void OnKeygenOnWorkerThread(scoped_ptr<net::KeygenHandler> keygen_handler,
                              IPC::Message* reply_msg);
  void OnMediaLogEvents(const std::vector<media::MediaLogEvent>&);

  // Check the policy for getting cookies. Gets the cookies if allowed.
  void CheckPolicyForCookies(int render_frame_id,
                             const GURL& url,
                             const GURL& first_party_for_cookies,
                             IPC::Message* reply_msg,
                             const net::CookieList& cookie_list);

  // Writes the cookies to reply messages, and sends the message.
  // Callback functions for getting cookies from cookie store.
  void SendGetCookiesResponse(IPC::Message* reply_msg,
                              const std::string& cookies);

  bool CheckBenchmarkingEnabled() const;
  bool CheckPreparsedJsCachingEnabled() const;
  void OnCompletedOpenChannelToNpapiPlugin(
      OpenChannelToNpapiPluginCallback* client);

  void OnAre3DAPIsBlocked(int render_view_id,
                          const GURL& top_origin_url,
                          ThreeDAPIType requester,
                          bool* blocked);
  void OnDidLose3DContext(const GURL& top_origin_url,
                          ThreeDAPIType context_type,
                          int arb_robustness_status_code);

#if defined(OS_ANDROID)
  void OnWebAudioMediaCodec(base::SharedMemoryHandle encoded_data_handle,
                            base::FileDescriptor pcm_output,
                            uint32_t data_size);
#endif

  void OnAllocateGpuMemoryBuffer(uint32 width,
                                 uint32 height,
                                 gfx::GpuMemoryBuffer::Format format,
                                 gfx::GpuMemoryBuffer::Usage usage,
                                 IPC::Message* reply);
  void GpuMemoryBufferAllocated(IPC::Message* reply,
                                const gfx::GpuMemoryBufferHandle& handle);
  void OnDeletedGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                uint32 sync_point);

  // Cached resource request dispatcher host and plugin service, guaranteed to
  // be non-null if Init succeeds. We do not own the objects, they are managed
  // by the BrowserProcess, which has a wider scope than we do.
  ResourceDispatcherHostImpl* resource_dispatcher_host_;
  PluginServiceImpl* plugin_service_;
  base::FilePath profile_data_directory_;

  HostSharedBitmapManagerClient bitmap_manager_client_;

  // Contextual information to be used for requests created here.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The ResourceContext which is to be used on the IO thread.
  ResourceContext* resource_context_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // Whether this process is used for incognito contents.
  // This doesn't belong here; http://crbug.com/89628
  bool incognito_;

  // Initialized to 0, accessed on FILE thread only.
  base::TimeTicks last_plugin_refresh_time_;

  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;

  int render_process_id_;

  std::set<OpenChannelToNpapiPluginCallback*> plugin_host_clients_;

  media::AudioManager* audio_manager_;
  MediaInternals* media_internals_;

  DISALLOW_COPY_AND_ASSIGN(RenderMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
