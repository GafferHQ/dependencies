// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_DELEGATED_FRAME_HOST_H_
#define CONTENT_BROWSER_COMPOSITOR_DELEGATED_FRAME_HOST_H_

#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/output/copy_output_result.h"
#include "cc/surfaces/surface_factory_client.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_process_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner_delegate.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace base {
class TickClock;
}

namespace cc {
class SurfaceFactory;
enum class SurfaceDrawStatus;
}

namespace media {
class VideoFrame;
}

namespace content {

class DelegatedFrameHost;
class ReadbackYUVInterface;
class RenderWidgetHostViewFrameSubscriber;
class RenderWidgetHostImpl;
class ResizeLock;

// The DelegatedFrameHostClient is the interface from the DelegatedFrameHost,
// which manages delegated frames, and the ui::Compositor being used to
// display them.
class CONTENT_EXPORT DelegatedFrameHostClient {
 public:
  virtual ui::Layer* DelegatedFrameHostGetLayer() const = 0;
  virtual bool DelegatedFrameHostIsVisible() const = 0;
  virtual gfx::Size DelegatedFrameHostDesiredSizeInDIP() const = 0;

  virtual bool DelegatedFrameCanCreateResizeLock() const = 0;
  virtual scoped_ptr<ResizeLock> DelegatedFrameHostCreateResizeLock(
      bool defer_compositor_lock) = 0;
  virtual void DelegatedFrameHostResizeLockWasReleased() = 0;

  virtual void DelegatedFrameHostSendCompositorSwapAck(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) = 0;
  virtual void DelegatedFrameHostSendReclaimCompositorResources(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) = 0;
  virtual void DelegatedFrameHostOnLostCompositorResources() = 0;

  virtual void DelegatedFrameHostUpdateVSyncParameters(
      const base::TimeTicks& timebase,
      const base::TimeDelta& interval) = 0;
};

// The DelegatedFrameHost is used to host all of the RenderWidgetHostView state
// and functionality that is associated with delegated frames being sent from
// the RenderWidget. The DelegatedFrameHost will push these changes through to
// the ui::Compositor associated with its DelegatedFrameHostClient.
class CONTENT_EXPORT DelegatedFrameHost
    : public ui::CompositorObserver,
      public ui::CompositorVSyncManager::Observer,
      public ui::LayerOwnerDelegate,
      public ImageTransportFactoryObserver,
      public DelegatedFrameEvictorClient,
      public cc::DelegatedFrameResourceCollectionClient,
      public cc::SurfaceFactoryClient,
      public base::SupportsWeakPtr<DelegatedFrameHost> {
 public:
  DelegatedFrameHost(DelegatedFrameHostClient* client);
  ~DelegatedFrameHost() override;

  // ui::CompositorObserver implementation.
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingAborted(ui::Compositor* compositor) override;
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // ui::CompositorVSyncManager::Observer implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  // ui::LayerOwnerObserver implementation.
  void OnLayerRecreated(ui::Layer* old_layer, ui::Layer* new_layer) override;

  // ImageTransportFactoryObserver implementation.
  void OnLostResources() override;

  // DelegatedFrameEvictorClient implementation.
  void EvictDelegatedFrame() override;

  // cc::DelegatedFrameProviderClient implementation.
  void UnusedResourcesAreAvailable() override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  bool CanCopyToBitmap() const;

  // Public interface exposed to RenderWidgetHostView.

  // Note: |satisfies_sequences| is cleared in calls to this function.
  void SwapDelegatedFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::DelegatedFrameData> frame_data,
      float frame_device_scale_factor,
      const std::vector<ui::LatencyInfo>& latency_info,
      std::vector<uint32_t>* satifies_sequences);
  void WasHidden();
  void WasShown(const ui::LatencyInfo& latency_info);
  void WasResized();
  bool HasSavedFrame();
  gfx::Size GetRequestedRendererSize() const;
  void SetCompositor(ui::Compositor* compositor);
  void ResetCompositor();
  void SetVSyncParameters(const base::TimeTicks& timebase,
                          const base::TimeDelta& interval);
  // Note: |src_subset| is specified in DIP dimensions while |output_size|
  // expects pixels.
  void CopyFromCompositingSurface(const gfx::Rect& src_subrect,
                                  const gfx::Size& output_size,
                                  ReadbackRequestCallback& callback,
                                  const SkColorType preferred_color_type);
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);
  bool CanCopyToVideoFrame() const;
  bool CanSubscribeFrame() const;
  void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber);
  void EndFrameSubscription();
  bool HasFrameSubscriber() const { return frame_subscriber_; }
  uint32_t GetSurfaceIdNamespace();

  // Exposed for tests.
  cc::DelegatedFrameProvider* FrameProviderForTesting() const {
    return frame_provider_.get();
  }
  cc::SurfaceId SurfaceIdForTesting() const { return surface_id_; }
  void OnCompositingDidCommitForTesting(ui::Compositor* compositor) {
    OnCompositingDidCommit(compositor);
  }
  bool ReleasedFrontLockActiveForTesting() const {
    return !!released_front_lock_.get();
  }
  void SetRequestCopyOfOutputCallbackForTesting(
      const base::Callback<void(scoped_ptr<cc::CopyOutputRequest>)>& callback) {
    request_copy_of_output_callback_for_testing_ = callback;
  }

 private:
  friend class DelegatedFrameHostClient;
  friend class RenderWidgetHostViewAuraCopyRequestTest;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);

  RenderWidgetHostViewFrameSubscriber* frame_subscriber() const {
    return frame_subscriber_.get();
  }
  bool ShouldCreateResizeLock();
  void LockResources();
  void UnlockResources();
  void RequestCopyOfOutput(scoped_ptr<cc::CopyOutputRequest> request);

  bool ShouldSkipFrame(gfx::Size size_in_dip) const;

  // Lazily grab a resize lock if the aura window size doesn't match the current
  // frame size, to give time to the renderer.
  void MaybeCreateResizeLock();

  // Checks if the resize lock can be released because we received an new frame.
  void CheckResizeLock();

  // Run all on compositing commit callbacks.
  void RunOnCommitCallbacks();

  // Add on compositing commit callback.
  void AddOnCommitCallbackAndDisableLocks(const base::Closure& callback);

  // Called after async thumbnailer task completes.  Scales and crops the result
  // of the copy.
  static void CopyFromCompositingSurfaceHasResult(
      const gfx::Size& dst_size_in_pixel,
      const SkColorType color_type,
      ReadbackRequestCallback& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareTextureCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkColorType color_type,
      ReadbackRequestCallback& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareBitmapCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkColorType color_type,
      ReadbackRequestCallback& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceHasResultForVideo(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      scoped_refptr<media::VideoFrame> video_frame,
      const base::Callback<void(bool)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceFinishedForVideo(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      const base::Callback<void(bool)>& callback,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      scoped_ptr<cc::SingleReleaseCallback> release_callback,
      bool result);
  static void ReturnSubscriberTexture(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      uint32 sync_point);

  void SendDelegatedFrameAck(uint32 output_surface_id);
  void SurfaceDrawn(uint32 output_surface_id, cc::SurfaceDrawStatus drawn);
  void SendReturnedDelegatedResources(uint32 output_surface_id);

  // Called to consult the current |frame_subscriber_|, to determine and maybe
  // initiate a copy-into-video-frame request.
  void DidReceiveFrameFromRenderer(const gfx::Rect& damage_rect);

  DelegatedFrameHostClient* const client_;
  ui::Compositor* compositor_;

  // True if this renders into a Surface, false if it renders into a delegated
  // layer.
  bool use_surfaces_;

  std::vector<base::Closure> on_compositing_did_commit_callbacks_;

  // The vsync manager we are observing for changes, if any.
  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;

  // The current VSync timebase and interval. These are zero until the first
  // call to SetVSyncParameters().
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  // Overridable tick clock used for testing functions using current time.
  scoped_ptr<base::TickClock> tick_clock_;

  // With delegated renderer, this is the last output surface, used to
  // disambiguate resources with the same id coming from different output
  // surfaces.
  uint32 last_output_surface_id_;

  // The number of delegated frame acks that are pending, to delay resource
  // returns until the acks are sent.
  int pending_delegated_ack_count_;

  // True after a delegated frame has been skipped, until a frame is not
  // skipped.
  bool skipped_frames_;
  std::vector<ui::LatencyInfo> skipped_latency_info_list_;

  // Holds delegated resources that have been given to a DelegatedFrameProvider,
  // and gives back resources when they are no longer in use for return to the
  // renderer.
  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;

  // Provides delegated frame updates to the cc::DelegatedRendererLayer.
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;

  // State for rendering into a Surface.
  scoped_ptr<cc::SurfaceIdAllocator> id_allocator_;
  scoped_ptr<cc::SurfaceFactory> surface_factory_;
  cc::SurfaceId surface_id_;
  gfx::Size current_surface_size_;
  float current_scale_factor_;
  cc::ReturnedResourceArray surface_returned_resources_;

  // This lock is the one waiting for a frame of the right size to come back
  // from the renderer/GPU process. It is set from the moment the aura window
  // got resized, to the moment we committed the renderer frame of the same
  // size. It keeps track of the size we expect from the renderer, and locks the
  // compositor, as well as the UI for a short time to give a chance to the
  // renderer of producing a frame of the right size.
  scoped_ptr<ResizeLock> resize_lock_;

  // Keeps track of the current frame size.
  gfx::Size current_frame_size_in_dip_;

  // This lock is for waiting for a front surface to become available to draw.
  scoped_refptr<ui::CompositorLock> released_front_lock_;

  enum CanLockCompositorState {
    YES_CAN_LOCK,
    // We locked, so at some point we'll need to kick a frame.
    YES_DID_LOCK,
    // No. A lock timed out, we need to kick a new frame before locking again.
    NO_PENDING_RENDERER_FRAME,
    // No. We've got a frame, but it hasn't been committed.
    NO_PENDING_COMMIT,
  };
  CanLockCompositorState can_lock_compositor_;

  base::TimeTicks last_draw_ended_;

  // Subscriber that listens to frame presentation events.
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> frame_subscriber_;
  std::vector<scoped_refptr<OwnedMailbox> > idle_frame_subscriber_textures_;

  // Callback used to pass the output request to the layer or to a function
  // specified by a test.
  base::Callback<void(scoped_ptr<cc::CopyOutputRequest>)>
      request_copy_of_output_callback_for_testing_;

  // YUV readback pipeline.
  scoped_ptr<content::ReadbackYUVInterface>
      yuv_readback_pipeline_;

  scoped_ptr<DelegatedFrameEvictor> delegated_frame_evictor_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_DELEGATED_FRAME_HOST_H_
