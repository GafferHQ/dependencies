// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
#define CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "content/child/blink_platform_impl.h"
#include "content/common/content_export.h"
#include "content/renderer/webpublicsuffixlist_impl.h"
#include "device/vibration/vibration_manager.mojom.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBFactory.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace cc {
class ContextProvider;
}

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
class WebBatteryStatus;
class WebDeviceMotionData;
class WebDeviceOrientationData;
class WebGraphicsContext3DProvider;
class WebServiceWorkerCacheStorage;
}

namespace scheduler {
class RendererScheduler;
class WebThreadImplForRendererScheduler;
}

namespace content {
class BatteryStatusDispatcher;
class DeviceLightEventPump;
class DeviceMotionEventPump;
class DeviceOrientationEventPump;
class PlatformEventObserverBase;
class QuotaMessageFilter;
class RendererClipboardDelegate;
class RenderView;
class ThreadSafeSender;
class WebClipboardImpl;
class WebDatabaseObserverImpl;
class WebFileSystemImpl;

class CONTENT_EXPORT RendererBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  explicit RendererBlinkPlatformImpl(
      scheduler::RendererScheduler* renderer_scheduler);
  virtual ~RendererBlinkPlatformImpl();

  void set_plugin_refresh_allowed(bool plugin_refresh_allowed) {
    plugin_refresh_allowed_ = plugin_refresh_allowed;
  }
  // Platform methods:
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebMimeRegistry* mimeRegistry();
  virtual blink::WebFileUtilities* fileUtilities();
  virtual blink::WebSandboxSupport* sandboxSupport();
  virtual blink::WebCookieJar* cookieJar();
  virtual blink::WebThemeEngine* themeEngine();
  virtual blink::WebSpeechSynthesizer* createSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client);
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(
      const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual void createMessageChannel(blink::WebMessagePortChannel** channel1,
                                    blink::WebMessagePortChannel** channel2);
  virtual blink::WebPrescientNetworking* prescientNetworking();
  virtual void cacheMetadata(const blink::WebURL&, int64, const char*, size_t);
  virtual blink::WebString defaultLocale();
  virtual void suddenTerminationChanged(bool enabled);
  virtual blink::WebStorageNamespace* createLocalStorageNamespace();
  virtual blink::Platform::FileHandle databaseOpenFile(
      const blink::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const blink::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const blink::WebString& vfs_file_name);
  virtual long long databaseGetSpaceAvailableForOrigin(
      const blink::WebString& origin_identifier);
  virtual bool databaseSetFileSize(
      const blink::WebString& vfs_file_name, long long size);
  virtual blink::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index,
      const blink::WebString& challenge,
      const blink::WebURL& url);
  virtual void getPluginList(bool refresh,
                             blink::WebPluginListBuilder* builder);
  virtual blink::WebPublicSuffixList* publicSuffixList();
  virtual void screenColorProfile(blink::WebVector<char>* to_profile);
  virtual blink::WebScrollbarBehavior* scrollbarBehavior();
  virtual blink::WebIDBFactory* idbFactory();
  virtual blink::WebServiceWorkerCacheStorage* cacheStorage(
      const blink::WebString& origin_identifier);
  virtual blink::WebFileSystem* fileSystem();
  virtual bool canAccelerate2dCanvas();
  virtual bool isThreadedCompositingEnabled();
  virtual double audioHardwareSampleRate();
  virtual size_t audioHardwareBufferSize();
  virtual unsigned audioHardwareOutputChannels();
  virtual blink::WebDatabaseObserver* databaseObserver();

  virtual blink::WebAudioDevice* createAudioDevice(
      size_t buffer_size, unsigned input_channels, unsigned channels,
      double sample_rate, blink::WebAudioDevice::RenderCallback* callback,
      const blink::WebString& input_device_id);

  virtual bool loadAudioResource(
      blink::WebAudioBus* destination_bus, const char* audio_file_data,
      size_t data_size);

  virtual blink::WebMIDIAccessor*
      createMIDIAccessor(blink::WebMIDIAccessorClient* client);

  virtual blink::WebBlobRegistry* blobRegistry();
  virtual void sampleGamepads(blink::WebGamepads&);
  virtual blink::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);
  virtual blink::WebMediaStreamCenter* createMediaStreamCenter(
      blink::WebMediaStreamCenterClient* client);
  virtual bool processMemorySizesInBytes(
      size_t* private_bytes, size_t* shared_bytes);
  virtual blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes);
  virtual blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context);
  virtual blink::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const blink::WebGraphicsContext3D::Attributes& attributes,
      blink::WebGraphicsContext3D* share_context,
      blink::WebGLInfo* gl_info);
  virtual blink::WebGraphicsContext3DProvider*
      createSharedOffscreenGraphicsContext3DProvider();
  virtual blink::WebCompositorSupport* compositorSupport();
  virtual blink::WebString convertIDNToUnicode(
      const blink::WebString& host, const blink::WebString& languages);
  virtual void startListening(blink::WebPlatformEventType,
                              blink::WebPlatformEventListener*);
  virtual void stopListening(blink::WebPlatformEventType);
  virtual void queryStorageUsageAndQuota(
      const blink::WebURL& storage_partition,
      blink::WebStorageQuotaType,
      blink::WebStorageQuotaCallbacks);
  virtual void vibrate(unsigned int milliseconds);
  virtual void cancelVibration();
  virtual blink::WebThread* currentThread();
  virtual void recordRappor(const char* metric, const blink::WebString& sample);
  virtual void recordRapporURL(const char* metric, const blink::WebURL& url);

  // Set the PlatformEventObserverBase in |platform_event_observers_| associated
  // with |type| to |observer|. If there was already an observer associated to
  // the given |type|, it will be replaced.
  // Note that |observer| will be owned by this object after the call.
  void SetPlatformEventObserverForTesting(
      blink::WebPlatformEventType type,
      scoped_ptr<PlatformEventObserverBase> observer);

  // Disables the WebSandboxSupport implementation for testing.
  // Tests that do not set up a full sandbox environment should call
  // SetSandboxEnabledForTesting(false) _before_ creating any instances
  // of this class, to ensure that we don't attempt to use sandbox-related
  // file descriptors or other resources.
  //
  // Returns the previous |enable| value.
  static bool SetSandboxEnabledForTesting(bool enable);

  //  Set a double to return when setDeviceLightListener is invoked.
  static void SetMockDeviceLightDataForTesting(double data);
  // Set WebDeviceMotionData to return when setDeviceMotionListener is invoked.
  static void SetMockDeviceMotionDataForTesting(
      const blink::WebDeviceMotionData& data);
  // Set WebDeviceOrientationData to return when setDeviceOrientationListener
  // is invoked.
  static void SetMockDeviceOrientationDataForTesting(
      const blink::WebDeviceOrientationData& data);

  // Notifies blink::WebBatteryStatusListener that battery status has changed.
  void MockBatteryStatusChangedForTesting(
      const blink::WebBatteryStatus& status);

  WebDatabaseObserverImpl* web_database_observer_impl() {
    return web_database_observer_impl_.get();
  }

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  // Factory that takes a type and return PlatformEventObserverBase that matches
  // it.
  static PlatformEventObserverBase* CreatePlatformEventObserverFromType(
      blink::WebPlatformEventType type);

  // Use the data previously set via SetMockDevice...DataForTesting() and send
  // them to the registered listener.
  void SendFakeDeviceEventDataForTesting(blink::WebPlatformEventType type);
  device::VibrationManagerPtr& GetConnectedVibrationManagerService();

  scoped_ptr<scheduler::WebThreadImplForRendererScheduler> main_thread_;

  scoped_ptr<RendererClipboardDelegate> clipboard_delegate_;
  scoped_ptr<WebClipboardImpl> clipboard_;

  class FileUtilities;
  scoped_ptr<FileUtilities> file_utilities_;

  class MimeRegistry;
  scoped_ptr<MimeRegistry> mime_registry_;

#if !defined(OS_ANDROID) && !defined(OS_WIN)
  class SandboxSupport;
  scoped_ptr<SandboxSupport> sandbox_support_;
#endif

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // If true, then a GetPlugins call is allowed to rescan the disk.
  bool plugin_refresh_allowed_;

  scoped_ptr<blink::WebIDBFactory> web_idb_factory_;

  scoped_ptr<blink::WebBlobRegistry> blob_registry_;

  WebPublicSuffixListImpl public_suffix_list_;

  scoped_ptr<DeviceLightEventPump> device_light_event_pump_;
  scoped_ptr<DeviceMotionEventPump> device_motion_event_pump_;
  scoped_ptr<DeviceOrientationEventPump> device_orientation_event_pump_;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  scoped_ptr<WebDatabaseObserverImpl> web_database_observer_impl_;

  cc_blink::WebCompositorSupportImpl compositor_support_;

  scoped_ptr<blink::WebScrollbarBehavior> web_scrollbar_behavior_;

  scoped_ptr<BatteryStatusDispatcher> battery_status_dispatcher_;

  // Handle to the Vibration mojo service.
  device::VibrationManagerPtr vibration_manager_;

  IDMap<PlatformEventObserverBase, IDMapOwnPointer> platform_event_observers_;

  DISALLOW_COPY_AND_ASSIGN(RendererBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_BLINK_PLATFORM_IMPL_H_
