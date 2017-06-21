// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_

#include <map>
#include <queue>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/output/begin_frame_args.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/renderer_host/input/stylus_text_selector.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/android/window_android_observer.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/touch_selection/touch_selection_controller.h"

struct ViewHostMsg_TextInputState_Params;

namespace cc {
class CopyOutputResult;
class DelegatedFrameProvider;
class DelegatedRendererLayer;
class Layer;
class SurfaceFactory;
class SurfaceIdAllocator;
enum class SurfaceDrawStatus;
}

namespace blink {
class WebExternalTextureLayer;
class WebTouchEvent;
class WebMouseEvent;
}

namespace content {
class ContentViewCoreImpl;
class OverscrollControllerAndroid;
class RenderWidgetHost;
class RenderWidgetHostImpl;
struct DidOverscrollParams;
struct NativeWebKeyboardEvent;

// -----------------------------------------------------------------------------
// See comments in render_widget_host_view.h about this class and its members.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT RenderWidgetHostViewAndroid
    : public RenderWidgetHostViewBase,
      public cc::DelegatedFrameResourceCollectionClient,
      public cc::SurfaceFactoryClient,
      public ui::GestureProviderClient,
      public ui::WindowAndroidObserver,
      public DelegatedFrameEvictorClient,
      public StylusTextSelectorClient,
      public ui::TouchSelectionControllerClient {
 public:
  RenderWidgetHostViewAndroid(RenderWidgetHostImpl* widget,
                              ContentViewCoreImpl* content_view_core);
  ~RenderWidgetHostViewAndroid() override;

  void Blur();

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewId GetNativeViewId() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void MovePluginWindows(const std::vector<WebPluginGeometry>& moves) override;
  void Focus() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  gfx::Size GetPhysicalBackingSize() const override;
  bool DoTopControlsShrinkBlinkSize() const override;
  float GetTopControlsHeight() const override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void TextInputTypeChanged(ui::TextInputType type,
                            ui::TextInputMode input_mode,
                            bool can_compose_inline,
                            int flags) override;
  void ImeCancelComposition() override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  void FocusedNodeChanged(bool is_editable_node) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  void AcceleratedSurfaceInitialized(int route_id) override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void SetBackgroundColor(SkColor color) override;
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      ReadbackRequestCallback& callback,
      const SkColorType preferred_color_type) override;
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) override;
  bool CanCopyToVideoFrame() const override;
  void GetScreenInfo(blink::WebScreenInfo* results) override;
  gfx::Rect GetBoundsInRootWindow() override;
  gfx::GLSurfaceHandle GetCompositingSurface() override;
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  void OnSetNeedsFlushInput() override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate) override;
  bool LockMouse() override;
  void UnlockMouse() override;
  void OnSwapCompositorFrame(uint32 output_surface_id,
                             scoped_ptr<cc::CompositorFrame> frame) override;
  void DidOverscroll(const DidOverscrollParams& params) override;
  void DidStopFlinging() override;
  uint32_t GetSurfaceIdNamespace() override;
  void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                               const SkBitmap& zoomed_bitmap) override;
  scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget() override;
  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          size_t start_offset,
                                          size_t end_offset) override;
  void OnDidNavigateMainFrameToNewPage() override;

  // cc::DelegatedFrameResourceCollectionClient implementation.
  void UnusedResourcesAreAvailable() override;

  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;

  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  // ui::WindowAndroidObserver implementation.
  void OnCompositingDidCommit() override;
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnVSync(base::TimeTicks frame_time,
               base::TimeDelta vsync_period) override;
  void OnAnimate(base::TimeTicks begin_frame_time) override;
  void OnActivityStopped() override;
  void OnActivityStarted() override;

  // DelegatedFrameEvictor implementation
  void EvictDelegatedFrame() override;

  // StylusTextSelectorClient implementation.
  void OnStylusSelectBegin(float x0, float y0, float x1, float y1) override;
  void OnStylusSelectUpdate(float x, float y) override;
  void OnStylusSelectEnd() override;
  void OnStylusSelectTap(base::TimeTicks time, float x, float y) override;

  // ui::TouchSelectionControllerClient implementation.
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  scoped_ptr<ui::TouchHandleDrawable> CreateDrawable() override;

  // Non-virtual methods
  void SetContentViewCore(ContentViewCoreImpl* content_view_core);
  SkColor GetCachedBackgroundColor() const;
  void SendKeyEvent(const NativeWebKeyboardEvent& event);
  void SendMouseEvent(const blink::WebMouseEvent& event);
  void SendMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  void SendGestureEvent(const blink::WebGestureEvent& event);

  void OnTextInputStateChanged(const ViewHostMsg_TextInputState_Params& params);
  void OnDidChangeBodyBackgroundColor(SkColor color);
  void OnStartContentIntent(const GURL& content_url);
  void OnSetNeedsBeginFrames(bool enabled);
  void OnSmartClipDataExtracted(const base::string16& text,
                                const base::string16& html,
                                const gfx::Rect rect);

  bool OnTouchEvent(const ui::MotionEvent& event);
  bool OnTouchHandleEvent(const ui::MotionEvent& event);
  void ResetGestureDetection();
  void SetDoubleTapSupportEnabled(bool enabled);
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  long GetNativeImeAdapter();

  void WasResized();

  void GetScaledContentBitmap(float scale,
                              SkColorType preferred_color_type,
                              gfx::Rect src_subrect,
                              ReadbackRequestCallback& result_callback);

  scoped_refptr<cc::Layer> CreateDelegatedLayer() const;

  bool HasValidFrame() const;

  void MoveCaret(const gfx::Point& point);
  void DismissTextHandles();
  void SetTextHandlesTemporarilyHidden(bool hidden);
  void OnShowingPastePopup(const gfx::PointF& point);
  void OnShowUnhandledTapUIIfNeeded(int x_dip, int y_dip);

  void SynchronousFrameMetadata(
      const cc::CompositorFrameMetadata& frame_metadata);

  void SetOverlayVideoMode(bool enabled);

  typedef base::Callback<
      void(const base::string16& content, int start_offset, int end_offset)>
      TextSurroundingSelectionCallback;
  void SetTextSurroundingSelectionCallback(
      const TextSurroundingSelectionCallback& callback);

  static void OnContextLost();

 private:
  void RunAckCallbacks(cc::SurfaceDrawStatus status);

  void DestroyDelegatedContent();
  void CheckOutputSurfaceChanged(uint32 output_surface_id);
  void SubmitFrame(scoped_ptr<cc::DelegatedFrameData> frame_data);
  void SwapDelegatedFrame(uint32 output_surface_id,
                          scoped_ptr<cc::DelegatedFrameData> frame_data);
  void SendDelegatedFrameAck(uint32 output_surface_id);
  void SendReturnedDelegatedResources(uint32 output_surface_id);

  void OnFrameMetadataUpdated(
      const cc::CompositorFrameMetadata& frame_metadata);
  void ComputeContentsSize(const cc::CompositorFrameMetadata& frame_metadata);

  void ShowInternal();
  void HideInternal(bool hide_frontbuffer, bool stop_observing_root_window);
  void AttachLayers();
  void RemoveLayers();

  // Called after async screenshot task completes. Scales and crops the result
  // of the copy.
  static void PrepareTextureCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      SkColorType color_type,
      const base::TimeTicks& start_time,
      ReadbackRequestCallback& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareTextureCopyOutputResultForDelegatedReadback(
      const gfx::Size& dst_size_in_pixel,
      SkColorType color_type,
      const base::TimeTicks& start_time,
      scoped_refptr<cc::Layer> readback_layer,
      ReadbackRequestCallback& callback,
      scoped_ptr<cc::CopyOutputResult> result);

  // DevTools ScreenCast support for Android WebView.
  void SynchronousCopyContents(const gfx::Rect& src_subrect_in_pixel,
                               const gfx::Size& dst_size_in_pixel,
                               ReadbackRequestCallback& callback,
                               const SkColorType color_type);

  // If we have locks on a frame during a ContentViewCore swap or a context
  // lost, the frame is no longer valid and we can safely release all the locks.
  // Use this method to release all the locks.
  void ReleaseLocksOnSurface();

  // Drop any incoming frames from the renderer when there are locks on the
  // current frame.
  void RetainFrame(uint32 output_surface_id,
                   scoped_ptr<cc::CompositorFrame> frame);

  void InternalSwapCompositorFrame(uint32 output_surface_id,
                                   scoped_ptr<cc::CompositorFrame> frame);
  void OnLostResources();

  enum VSyncRequestType {
    FLUSH_INPUT = 1 << 0,
    BEGIN_FRAME = 1 << 1,
    PERSISTENT_BEGIN_FRAME = 1 << 2
  };
  void RequestVSyncUpdate(uint32 requests);
  void StartObservingRootWindow();
  void StopObservingRootWindow();
  void SendBeginFrame(base::TimeTicks frame_time, base::TimeDelta vsync_period);
  bool Animate(base::TimeTicks frame_time);
  void RequestDisallowInterceptTouchEvent();

  // The model object.
  RenderWidgetHostImpl* host_;

  bool use_surfaces_;

  // Used to control action dispatch at the next |OnVSync()| call.
  uint32 outstanding_vsync_requests_;

  bool is_showing_;

  // ContentViewCoreImpl is our interface to the view system.
  ContentViewCoreImpl* content_view_core_;

  // Cache the WindowAndroid instance exposed by ContentViewCore to avoid
  // calling into ContentViewCore when it is being detached from the
  // WebContents during destruction. The WindowAndroid has stronger lifetime
  // guarantees, and should be safe to use for observer detachment.
  // This will be non-null iff |content_view_core_| is non-null.
  ui::WindowAndroid* content_view_core_window_android_;

  ImeAdapterAndroid ime_adapter_android_;

  // Body background color of the underlying document.
  SkColor cached_background_color_;

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;
  scoped_refptr<cc::Layer> layer_;

  scoped_ptr<cc::SurfaceIdAllocator> id_allocator_;
  scoped_ptr<cc::SurfaceFactory> surface_factory_;
  cc::SurfaceId surface_id_;
  gfx::Size current_surface_size_;
  cc::ReturnedResourceArray surface_returned_resources_;

  // The most recent texture size that was pushed to the texture layer.
  gfx::Size texture_size_in_layer_;

  // The most recent content size that was pushed to the texture layer.
  gfx::Size content_size_in_layer_;

  // The output surface id of the last received frame.
  uint32_t last_output_surface_id_;


  std::queue<base::Closure> ack_callbacks_;

  // Used to control and render overscroll-related effects.
  scoped_ptr<OverscrollControllerAndroid> overscroll_controller_;

  // Provides gesture synthesis given a stream of touch events (derived from
  // Android MotionEvent's) and touch event acks.
  ui::FilteredGestureProvider gesture_provider_;

  // Handles gesture based text selection
  StylusTextSelector stylus_text_selector_;

  // Manages selection handle rendering and manipulation.
  // This will always be NULL if |content_view_core_| is NULL.
  scoped_ptr<ui::TouchSelectionController> selection_controller_;

  int accelerated_surface_route_id_;

  // Size to use if we have no backing ContentViewCore
  gfx::Size default_size_;

  const bool using_browser_compositor_;

  scoped_ptr<DelegatedFrameEvictor> frame_evictor_;

  size_t locks_on_frame_count_;
  bool observing_root_window_;

  struct LastFrameInfo {
    LastFrameInfo(uint32 output_id,
                  scoped_ptr<cc::CompositorFrame> output_frame);
    ~LastFrameInfo();
    uint32 output_surface_id;
    scoped_ptr<cc::CompositorFrame> frame;
  };

  scoped_ptr<LastFrameInfo> last_frame_info_;

  TextSurroundingSelectionCallback text_surrounding_selection_callback_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  base::WeakPtrFactory<RenderWidgetHostViewAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAndroid);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_ANDROID_H_
