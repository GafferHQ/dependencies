// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/media/media_player_messages_enums_android.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/android/media_info_loader.h"
#include "content/renderer/media/android/media_source_delegate.h"
#include "content/renderer/media/android/stream_texture_factory.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/android/media_player_android.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_keys.h"
#include "media/base/time_delta_interpolator.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/cdm/proxy_decryptor.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebContentDecryptionModule;
class WebContentDecryptionModuleResult;
class WebFrame;
class WebMediaPlayerClient;
class WebURL;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace gpu {
struct MailboxHolder;
}

namespace media {
class CdmContext;
class CdmFactory;
class MediaLog;
class MediaPermission;
class WebContentDecryptionModuleImpl;
class WebMediaPlayerDelegate;
}

namespace content {

class RendererCdmManager;
class RendererMediaPlayerManager;

// This class implements blink::WebMediaPlayer by keeping the android
// media player in the browser process. It listens to all the status changes
// sent from the browser process and sends playback controls to the media
// player.
class WebMediaPlayerAndroid : public blink::WebMediaPlayer,
                              public cc::VideoFrameProvider,
                              public RenderFrameObserver,
                              public StreamTextureFactoryContextObserver {
 public:
  // Construct a WebMediaPlayerAndroid object. This class communicates with the
  // MediaPlayerAndroid object in the browser process through |proxy|.
  // TODO(qinmin): |frame| argument is used to determine whether the current
  // player can enter fullscreen. This logic should probably be moved into
  // blink, so that enterFullscreen() will not be called if another video is
  // already in fullscreen.
  WebMediaPlayerAndroid(
      blink::WebFrame* frame,
      blink::WebMediaPlayerClient* client,
      base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
      RendererMediaPlayerManager* player_manager,
      media::CdmFactory* cdm_factory,
      media::MediaPermission* media_permission,
      blink::WebContentDecryptionModule* initial_cdm,
      scoped_refptr<StreamTextureFactory> factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      media::MediaLog* media_log);
  virtual ~WebMediaPlayerAndroid();

  // blink::WebMediaPlayer implementation.
  virtual void enterFullscreen();

  // Resource loading.
  virtual void load(LoadType load_type,
                    const blink::WebURL& url,
                    CORSMode cors_mode);

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual void seek(double seconds);
  virtual bool supportsSave() const;
  virtual void setRate(double rate);
  virtual void setVolume(double volume);
  virtual void setSinkId(const blink::WebString& device_id,
                         media::WebSetSinkIdCB* raw_web_callbacks);
  virtual void requestRemotePlayback();
  virtual void requestRemotePlaybackControl();
  virtual blink::WebTimeRanges buffered() const;
  virtual blink::WebTimeRanges seekable() const;

  // Poster image, as defined in the <video> element.
  virtual void setPoster(const blink::WebURL& poster) override;

  // Methods for painting.
  // FIXME: This path "only works" on Android. It is a workaround for the
  // issue that Skia could not handle Android's GL_TEXTURE_EXTERNAL_OES texture
  // internally. It should be removed and replaced by the normal paint path.
  // https://code.google.com/p/skia/issues/detail?id=1189
  virtual void paint(blink::WebCanvas* canvas,
                     const blink::WebRect& rect,
                     unsigned char alpha,
                     SkXfermode::Mode mode);

  bool copyVideoTextureToPlatformTexture(
      blink::WebGraphicsContext3D* web_graphics_context,
      unsigned int texture,
      unsigned int internal_format,
      unsigned int type,
      bool premultiply_alpha,
      bool flip_y) override;

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const;
  virtual bool hasAudio() const;

  virtual bool isRemote() const;

  // Dimensions of the video.
  virtual blink::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual double duration() const;
  virtual double timelineOffset() const;
  virtual double currentTime() const;

  virtual bool didLoadingProgress();

  // Internal states of loading and network.
  virtual blink::WebMediaPlayer::NetworkState networkState() const;
  virtual blink::WebMediaPlayer::ReadyState readyState() const;

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;

  virtual double mediaTimeForTimeValue(double timeValue) const;

  // Provide statistics.
  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  // cc::VideoFrameProvider implementation. These methods are running on the
  // compositor thread.
  void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;
  scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;

  // Media player callback handlers.
  void OnMediaMetadataChanged(base::TimeDelta duration, int width,
                              int height, bool success);
  void OnPlaybackComplete();
  void OnBufferingUpdate(int percentage);
  void OnSeekRequest(const base::TimeDelta& time_to_seek);
  void OnSeekComplete(const base::TimeDelta& current_time);
  void OnMediaError(int error_type);
  void OnVideoSizeChanged(int width, int height);
  void OnDurationChanged(const base::TimeDelta& duration);

  // Called to update the current time.
  void OnTimeUpdate(base::TimeDelta current_timestamp,
                    base::TimeTicks current_time_ticks);

  // Functions called when media player status changes.
  void OnConnectedToRemoteDevice(const std::string& remote_playback_message);
  void OnDisconnectedFromRemoteDevice();
  void OnDidExitFullscreen();
  void OnMediaPlayerPlay();
  void OnMediaPlayerPause();
  void OnRemoteRouteAvailabilityChanged(bool routes_available);

  // StreamTextureFactoryContextObserver implementation.
  void ResetStreamTextureProxy() override;

  // Called when the player is released.
  virtual void OnPlayerReleased();

  // This function is called by the RendererMediaPlayerManager to pause the
  // video and release the media player and surface texture when we switch tabs.
  // However, the actual GlTexture is not released to keep the video screenshot.
  virtual void ReleaseMediaResources();

  // RenderFrameObserver implementation.
  void OnDestruct() override;

#if defined(VIDEO_HOLE)
  // Calculate the boundary rectangle of the media player (i.e. location and
  // size of the video frame).
  // Returns true if the geometry has been changed since the last call.
  bool UpdateBoundaryRectangle();

  const gfx::RectF GetBoundaryRectangle();
#endif  // defined(VIDEO_HOLE)

  virtual MediaKeyException generateKeyRequest(
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);
  virtual MediaKeyException addKey(
      const blink::WebString& key_system,
      const unsigned char* key,
      unsigned key_length,
      const unsigned char* init_data,
      unsigned init_data_length,
      const blink::WebString& session_id);
  virtual MediaKeyException cancelKeyRequest(
      const blink::WebString& key_system,
      const blink::WebString& session_id);

  virtual void setContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result);

  void OnKeyAdded(const std::string& session_id);
  void OnKeyError(const std::string& session_id,
                  media::MediaKeys::KeyError error_code,
                  uint32 system_code);
  void OnKeyMessage(const std::string& session_id,
                    const std::vector<uint8>& message,
                    const GURL& destination_url);

  void OnMediaSourceOpened(blink::WebMediaSource* web_media_source);

  void OnEncryptedMediaInitData(media::EmeInitDataType init_data_type,
                                const std::vector<uint8>& init_data);

  // Called when a decoder detects that the key needed to decrypt the stream
  // is not available.
  void OnWaitingForDecryptionKey();

 protected:
  // Helper method to update the playing state.
  void UpdatePlayingState(bool is_playing_);

  // Helper methods for posting task for setting states and update WebKit.
  void UpdateNetworkState(blink::WebMediaPlayer::NetworkState state);
  void UpdateReadyState(blink::WebMediaPlayer::ReadyState state);
  void TryCreateStreamTextureProxyIfNeeded();
  void DoCreateStreamTexture();

  // Helper method to reestablish the surface texture peer for android
  // media player.
  void EstablishSurfaceTexturePeer();

  // Requesting whether the surface texture peer needs to be reestablished.
  void SetNeedsEstablishPeer(bool needs_establish_peer);

 private:
  void InitializePlayer(const GURL& url,
                        const GURL& first_party_for_cookies,
                        bool allowed_stored_credentials,
                        int demuxer_client_id);
  void Pause(bool is_media_related_action);
  void DrawRemotePlaybackText(const std::string& remote_playback_message);
  void ReallocateVideoFrame();
  void SetCurrentFrameInternal(scoped_refptr<media::VideoFrame>& frame);
  void RemoveSurfaceTextureAndProxy();
  void DidLoadMediaInfo(MediaInfoLoader::Status status,
                        const GURL& redirected_url,
                        const GURL& first_party_for_cookies,
                        bool allow_stored_credentials);
  bool IsKeySystemSupported(const std::string& key_system);
  bool IsLocalResource();

  // Actually do the work for generateKeyRequest/addKey so they can easily
  // report results to UMA.
  MediaKeyException GenerateKeyRequestInternal(const std::string& key_system,
                                               const unsigned char* init_data,
                                               unsigned init_data_length);
  MediaKeyException AddKeyInternal(const std::string& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const std::string& session_id);
  MediaKeyException CancelKeyRequestInternal(const std::string& key_system,
                                             const std::string& session_id);

  // Called when |cdm_context| is ready.
  void OnCdmContextReady(media::CdmContext* cdm_context);

  // Sets the CDM. Should only be called when |is_player_initialized_| is true
  // and a new non-null |cdm_context_| is available. Fires |cdm_attached_cb_|
  // with the result after the CDM is attached.
  void SetCdmInternal(const media::CdmAttachedCB& cdm_attached_cb);

  // Requests that this object notifies when a decryptor is ready through the
  // |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  // Called when the ContentDecryptionModule has been attached to the
  // pipeline/decoders.
  void ContentDecryptionModuleAttached(
      blink::WebContentDecryptionModuleResult result,
      bool success);

  bool IsHLSStream() const;

  blink::WebFrame* const frame_;

  blink::WebMediaPlayerClient* const client_;

  // |delegate_| is used to notify the browser process of the player status, so
  // that the browser process can control screen locks.
  // TODO(qinmin): Currently android mediaplayer takes care of the screen
  // lock. So this is only used for media source. Will apply this to regular
  // media tag once http://crbug.com/247892 is fixed.
  base::WeakPtr<media::WebMediaPlayerDelegate> delegate_;

  // Save the list of buffered time ranges.
  blink::WebTimeRanges buffered_;

  // Size of the video.
  blink::WebSize natural_size_;

  // Size that has been sent to StreamTexture.
  blink::WebSize cached_stream_texture_size_;

  // The video frame object used for rendering by the compositor.
  scoped_refptr<media::VideoFrame> current_frame_;
  base::Lock current_frame_lock_;

  base::ThreadChecker main_thread_checker_;

  // Message loop for media thread.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // URL of the media file to be fetched.
  GURL url_;

  // URL of the media file after |media_info_loader_| resolves all the
  // redirections.
  GURL redirected_url_;

  // Media duration.
  base::TimeDelta duration_;

  // Flag to remember if we have a trusted duration_ value provided by
  // MediaSourceDelegate notifying OnDurationChanged(). In this case, ignore
  // any subsequent duration value passed to OnMediaMetadataChange().
  bool ignore_metadata_duration_change_;

  // Seek gets pending if another seek is in progress. Only last pending seek
  // will have effect.
  bool pending_seek_;
  base::TimeDelta pending_seek_time_;

  // Internal seek state.
  bool seeking_;
  base::TimeDelta seek_time_;

  // Whether loading has progressed since the last call to didLoadingProgress.
  bool did_loading_progress_;

  // Manages this object and delegates player calls to the browser process.
  // Owned by RenderFrameImpl.
  RendererMediaPlayerManager* const player_manager_;

  // TODO(xhwang): Remove |cdm_factory_| when prefixed EME is deprecated. See
  // http://crbug.com/249976
  media::CdmFactory* const cdm_factory_;

  media::MediaPermission* media_permission_;

  // Player ID assigned by the |player_manager_|.
  int player_id_;

  // Current player states.
  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  // GL texture ID allocated to the video.
  unsigned int texture_id_;

  // GL texture mailbox for texture_id_ to provide in the VideoFrame, and sync
  // point for when the mailbox was produced.
  gpu::Mailbox texture_mailbox_;

  // Stream texture ID allocated to the video.
  unsigned int stream_id_;

  // Whether the media player has been initialized.
  bool is_player_initialized_;

  // Whether the media player is playing.
  bool is_playing_;

  // Whether media player needs to re-establish the surface texture peer.
  bool needs_establish_peer_;

  // Whether the video size info is available.
  bool has_size_info_;

  const scoped_refptr<base::SingleThreadTaskRunner> compositor_loop_;

  // Object for allocating stream textures.
  scoped_refptr<StreamTextureFactory> stream_texture_factory_;

  // Object for calling back the compositor thread to repaint the video when a
  // frame available. It should be initialized on the compositor thread.
  // Accessed on main thread and on compositor thread when main thread is
  // blocked.
  ScopedStreamTextureProxy stream_texture_proxy_;

  // Whether media player needs external surface.
  // Only used for the VIDEO_HOLE logic.
  bool needs_external_surface_;

  // Whether the player is in fullscreen.
  bool is_fullscreen_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  // Accessed on main thread and on compositor thread when main thread is
  // blocked.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  scoped_ptr<cc_blink::WebLayerImpl> video_weblayer_;

#if defined(VIDEO_HOLE)
  // A rectangle represents the geometry of video frame, when computed last
  // time.
  gfx::RectF last_computed_rect_;

  // Whether to use the video overlay for all embedded video.
  // True only for testing.
  bool force_use_overlay_embedded_video_;
#endif  // defined(VIDEO_HOLE)

  MediaPlayerHostMsg_Initialize_Type player_type_;

  // Whether the browser is currently connected to a remote media player.
  bool is_remote_;

  scoped_refptr<media::MediaLog> media_log_;

  scoped_ptr<MediaInfoLoader> info_loader_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  std::string current_key_system_;

  // Temporary for EME v0.1. Not needed for unprefixed EME, and can be removed
  // when prefixed EME is removed.
  media::EmeInitDataType init_data_type_;

  // Manages decryption keys and decrypts encrypted frames.
  scoped_ptr<media::ProxyDecryptor> proxy_decryptor_;

  // Non-owned pointer to the CdmContext. Updated in the constructor,
  // generateKeyRequest() or setContentDecryptionModule().
  media::CdmContext* cdm_context_;

  // This is only Used by Clear Key key system implementation, where a renderer
  // side CDM will be used. This is similar to WebMediaPlayerImpl. For other key
  // systems, a browser side CDM will be used and we set CDM by calling
  // player_manager_->SetCdm() directly.
  media::DecryptorReadyCB decryptor_ready_cb_;

  SkBitmap bitmap_;

  // Whether stored credentials are allowed to be passed to the server.
  bool allow_stored_credentials_;

  // Whether the resource is local.
  bool is_local_resource_;

  // base::TickClock used by |interpolator_|.
  base::DefaultTickClock default_tick_clock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  media::TimeDeltaInterpolator interpolator_;

  scoped_ptr<MediaSourceDelegate> media_source_delegate_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebMediaPlayerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
