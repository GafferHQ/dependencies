// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/compositor/delegated_frame_host.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/browser/renderer_host/begin_frame_observer_proxy.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/touch/selection_bound.h"
#include "ui/base/touch/touch_editing_controller.h"
#include "ui/events/gestures/motion_event_aura.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/activation_delegate.h"

namespace aura {
class WindowTracker;
namespace client {
class ScopedTooltipDisabler;
}
}

namespace cc {
class CopyOutputRequest;
class CopyOutputResult;
class DelegatedFrameData;
}

namespace gfx {
class Canvas;
class Display;
}

namespace gpu {
struct Mailbox;
}

namespace ui {
class CompositorLock;
class InputMethod;
class LocatedEvent;
class Texture;
}

namespace content {
#if defined(OS_WIN)
class LegacyRenderWidgetHostHWND;
#endif

class OverscrollController;
class RenderFrameHostImpl;
class RenderWidgetHostImpl;
class RenderWidgetHostView;

// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewAura
    : public RenderWidgetHostViewBase,
      public DelegatedFrameHostClient,
      public BeginFrameObserverProxyClient,
      public ui::TextInputClient,
      public gfx::DisplayObserver,
      public aura::WindowTreeHostObserver,
      public aura::WindowDelegate,
      public aura::client::ActivationDelegate,
      public aura::client::FocusChangeObserver,
      public aura::client::CursorClientObserver {
 public:
  // Displays and controls touch editing elements such as selection handles.
  class TouchEditingClient {
   public:
    TouchEditingClient() {}

    // Tells the client to start showing touch editing handles.
    virtual void StartTouchEditing() = 0;

    // Notifies the client that touch editing is no longer needed. |quick|
    // determines whether the handles should fade out quickly or slowly.
    virtual void EndTouchEditing(bool quick) = 0;

    // Notifies the client that the selection bounds need to be updated.
    virtual void OnSelectionOrCursorChanged(
        const ui::SelectionBound& anchor,
        const ui::SelectionBound& focus) = 0;

    // Notifies the client that the current text input type as changed.
    virtual void OnTextInputTypeChanged(ui::TextInputType type) = 0;

    // Notifies the client that an input event is about to be sent to the
    // renderer. Returns true if the client wants to stop event propagation.
    virtual bool HandleInputEvent(const ui::Event* event) = 0;

    // Notifies the client that a gesture event ack was received.
    virtual void GestureEventAck(int gesture_event_type) = 0;

    // Notifies the client that the fling has ended, so it can activate touch
    // editing if needed.
    virtual void DidStopFlinging() = 0;

    // This is called when the view is destroyed, so that the client can
    // perform any necessary clean-up.
    virtual void OnViewDestroyed() = 0;

   protected:
    virtual ~TouchEditingClient() {}
  };

  void set_touch_editing_client(TouchEditingClient* client) {
    touch_editing_client_ = client;
  }

  // When |is_guest_view_hack| is true, this view isn't really the view for
  // the |widget|, a RenderWidgetHostViewGuest is.
  //
  // TODO(lazyboy): Remove |is_guest_view_hack| once BrowserPlugin has migrated
  // to use RWHVChildFrame (http://crbug.com/330264).
  RenderWidgetHostViewAura(RenderWidgetHost* host, bool is_guest_view_hack);

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewId GetNativeViewId() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::TextInputClient* GetTextInputClient() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  void SetBackgroundColor(SkColor color) override;
  gfx::Size GetVisibleViewportSize() const override;
  void SetInsets(const gfx::Insets& insets) override;

  // Overridden from RenderWidgetHostViewBase:
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void MovePluginWindows(const std::vector<WebPluginGeometry>& moves) override;
  void Focus() override;
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
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  gfx::Size GetRequestedRendererSize() const override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
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
  bool CanSubscribeFrame() const override;
  void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) override;
  void EndFrameSubscription() override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void GetScreenInfo(blink::WebScreenInfo* results) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void WheelEventAck(const blink::WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
  scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget() override;
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  gfx::GLSurfaceHandle GetCompositingSurface() override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate) override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  void ShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                               const SkBitmap& zoomed_bitmap) override;
  bool LockMouse() override;
  void UnlockMouse() override;
  void OnSwapCompositorFrame(uint32 output_surface_id,
                             scoped_ptr<cc::CompositorFrame> frame) override;
  void DidStopFlinging() override;
  void OnDidNavigateMainFrameToNewPage() override;
  uint32_t GetSurfaceIdNamespace() override;

#if defined(OS_WIN)
  void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) override;
  gfx::NativeViewId GetParentForWindowlessPlugin() const override;
#endif

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(base::char16 ch, int flags) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32 index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretInRect(const gfx::Rect& rect) override;
  bool IsEditCommandEnabled(int command_id) override;
  void SetEditCommandForNextKeyEvent(int command_id) override;

  // Overridden from gfx::DisplayObserver:
  void OnDisplayAdded(const gfx::Display& new_display) override;
  void OnDisplayRemoved(const gfx::Display& old_display) override;
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t metrics) override;

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(gfx::Path* mask) const override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::client::ActivationDelegate:
  bool ShouldActivate() const override;

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from aura::WindowTreeHostObserver:
  void OnHostMoved(const aura::WindowTreeHost* host,
                   const gfx::Point& new_origin) override;

  void OnTextInputStateChanged(const ViewHostMsg_TextInputState_Params& params);

#if defined(OS_WIN)
  // Sets the cutout rects from constrained windows. These are rectangles that
  // windowed NPAPI plugins shouldn't paint in. Overwrites any previous cutout
  // rects.
  void UpdateConstrainedWindowRects(const std::vector<gfx::Rect>& rects);

  // Updates the cursor clip region. Used for mouse locking.
  void UpdateMouseLockRegion();

  // Notification that the LegacyRenderWidgetHostHWND was destroyed.
  void OnLegacyWindowDestroyed();
#endif

  void DisambiguationPopupRendered(const SkBitmap& result,
                                   ReadbackResponse response);

  void HideDisambiguationPopup();

  void ProcessDisambiguationGesture(ui::GestureEvent* event);

  void ProcessDisambiguationMouse(ui::MouseEvent* event);

  // Method to indicate if this instance is shutting down or closing.
  // TODO(shrikant): Discuss around to see if it makes sense to add this method
  // as part of RenderWidgetHostView.
  bool IsClosing() const { return in_shutdown_; }

  // Sets whether the overscroll controller should be enabled for this page.
  void SetOverscrollControllerEnabled(bool enabled);

  void SnapToPhysicalPixelBoundary();

  OverscrollController* overscroll_controller() const {
    return overscroll_controller_.get();
  }

  // Called when the context menu is about to be displayed.
  void OnShowContextMenu();

 protected:
  ~RenderWidgetHostViewAura() override;

  // Exposed for tests.
  aura::Window* window() { return window_; }

  DelegatedFrameHost* GetDelegatedFrameHost() const {
    return delegated_frame_host_.get();
  }
  const ui::MotionEventAura& pointer_state() const { return pointer_state_; }

 private:
  friend class RenderWidgetHostViewAuraCopyRequestTest;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           PopupRetainsCaptureAfterMouseRelease);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SetCompositionText);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventState);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           TouchEventPositionsArentRounded);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventSyncAsync);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, Resize);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SwapNotifiesWindow);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, RecreateLayers);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, OutputSurfaceIdChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SoftwareDPIChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           UpdateCursorIfOverSelf);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           VisibleViewportTest);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           OverscrollResetsOnBlur);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           FinishCompositionByMouse);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest,
                           WebContentsViewReparent);

  class WindowObserver;
  friend class WindowObserver;

  class WindowAncestorObserver;
  friend class WindowAncestorObserver;

  void UpdateCursorIfOverSelf();

  // Tracks whether SnapToPhysicalPixelBoundary() has been called.
  bool has_snapped_to_boundary() { return has_snapped_to_boundary_; }
  void ResetHasSnappedToBoundary() { has_snapped_to_boundary_ = false; }

  // Set the bounds of the window and handle size changes.  Assumes the caller
  // has already adjusted the origin of |rect| to conform to whatever coordinate
  // space is required by the aura::Window.
  void InternalSetBounds(const gfx::Rect& rect);

#if defined(OS_WIN)
  bool UsesNativeWindowFrame() const;
#endif

  ui::InputMethod* GetInputMethod() const;

  // Sends shutdown request.
  void Shutdown();

  // Returns whether the widget needs an input grab to work properly.
  bool NeedsInputGrab();

  // Returns whether the widget needs to grab mouse capture to work properly.
  bool NeedsMouseCapture();

  // Confirm existing composition text in the webpage and ask the input method
  // to cancel its ongoing composition session.
  void FinishImeCompositionSession();

  // This method computes movementX/Y and keeps track of mouse location for
  // mouse lock on all mouse move events.
  void ModifyEventMovementAndCoords(blink::WebMouseEvent* event);

  // Sends an IPC to the renderer process to communicate whether or not
  // the mouse cursor is visible anywhere on the screen.
  void NotifyRendererOfCursorVisibilityState(bool is_visible);

  // If |clip| is non-empty and and doesn't contain |rect| or |clip| is empty
  // SchedulePaint() is invoked for |rect|.
  void SchedulePaintIfNotInClip(const gfx::Rect& rect, const gfx::Rect& clip);

  // Helper method to determine if, in mouse locked mode, the cursor should be
  // moved to center.
  bool ShouldMoveToCenter();

  // Called after |window_| is parented to a WindowEventDispatcher.
  void AddedToRootWindow();

  // Called prior to removing |window_| from a WindowEventDispatcher.
  void RemovingFromRootWindow();

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  gfx::Size DelegatedFrameHostDesiredSizeInDIP() const override;
  bool DelegatedFrameCanCreateResizeLock() const override;
  scoped_ptr<ResizeLock> DelegatedFrameHostCreateResizeLock(
      bool defer_compositor_lock) override;
  void DelegatedFrameHostResizeLockWasReleased() override;
  void DelegatedFrameHostSendCompositorSwapAck(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) override;
  void DelegatedFrameHostSendReclaimCompositorResources(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) override;
  void DelegatedFrameHostOnLostCompositorResources() override;
  void DelegatedFrameHostUpdateVSyncParameters(
      const base::TimeTicks& timebase,
      const base::TimeDelta& interval) override;

  // BeginFrameObserverProxyClient implementation.
  void SendBeginFrame(const cc::BeginFrameArgs& args) override;

  // Detaches |this| from the input method object.
  void DetachFromInputMethod();

  // Before calling RenderWidgetHost::ForwardKeyboardEvent(), this method
  // calls our keybindings handler against the event and send matched
  // edit commands to renderer instead.
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Dismisses a Web Popup on a mouse or touch press outside the popup and its
  // parent.
  void ApplyEventFilterForPopupExit(ui::LocatedEvent* event);

  // Converts |rect| from window coordinate to screen coordinate.
  gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const;

  // Converts |rect| from screen coordinate to window coordinate.
  gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const;

  // Helper function to set keyboard focus to the main window.
  void SetKeyboardFocus();

  // Called when RenderWidget wants to start BeginFrame scheduling or stop.
  void OnSetNeedsBeginFrames(bool needs_begin_frames);

  RenderFrameHostImpl* GetFocusedFrame();

  // Returns true if the |event| passed in can be forwarded to the renderer.
  bool CanRendererHandleEvent(const ui::MouseEvent* event,
                              bool mouse_locked,
                              bool selection_popup);

  // Called when the parent window bounds change.
  void HandleParentBoundsChanged();

  // Called when the parent window hierarchy for our window changes.
  void ParentHierarchyChanged();

  // The model object.
  RenderWidgetHostImpl* host_;

  aura::Window* window_;

  scoped_ptr<DelegatedFrameHost> delegated_frame_host_;

  scoped_ptr<WindowObserver> window_observer_;

  // Tracks the ancestors of the RWHVA window for window location changes.
  scoped_ptr<WindowAncestorObserver> ancestor_window_observer_;

  // Are we in the process of closing?  Tracked so fullscreen views can avoid
  // sending a second shutdown request to the host when they lose the focus
  // after requesting shutdown for another reason (e.g. Escape key).
  bool in_shutdown_;

  // True if in the process of handling a window bounds changed notification.
  bool in_bounds_changed_;

  // Is this a fullscreen view?
  bool is_fullscreen_;

  // Our parent host view, if this is a popup.  NULL otherwise.
  RenderWidgetHostViewAura* popup_parent_host_view_;

  // Our child popup host. NULL if we do not have a child popup.
  RenderWidgetHostViewAura* popup_child_host_view_;

  class EventFilterForPopupExit;
  friend class EventFilterForPopupExit;
  scoped_ptr<ui::EventHandler> event_filter_for_popup_exit_;

  // True when content is being loaded. Used to show an hourglass cursor.
  bool is_loading_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // Stores the current state of the active pointers targeting this
  // object.
  ui::MotionEventAura pointer_state_;

  // The current text input type.
  ui::TextInputType text_input_type_;
  // The current text input mode corresponding to HTML5 inputmode attribute.
  ui::TextInputMode text_input_mode_;
  // The current text input flags.
  int text_input_flags_;
  bool can_compose_inline_;

  // Bounds for the selection.
  ui::SelectionBound selection_anchor_;
  ui::SelectionBound selection_focus_;

  // The current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Indicates if there is onging composition text.
  bool has_composition_text_;

  // Whether return characters should be passed on to the RenderWidgetHostImpl.
  bool accept_return_character_;

  // Current tooltip text.
  base::string16 tooltip_;

  // The size and scale of the last software compositing frame that was swapped.
  gfx::Size last_swapped_software_frame_size_;
  float last_swapped_software_frame_scale_factor_;

  // If non-NULL we're in OnPaint() and this is the supplied canvas.
  gfx::Canvas* paint_canvas_;

  // Used to record the last position of the mouse.
  // While the mouse is locked, they store the last known position just as mouse
  // lock was entered.
  // Relative to the upper-left corner of the view.
  gfx::Point unlocked_mouse_position_;
  // Relative to the upper-left corner of the screen.
  gfx::Point unlocked_global_mouse_position_;
  // Last cursor position relative to screen. Used to compute movementX/Y.
  gfx::Point global_mouse_position_;
  // In mouse locked mode, we synthetically move the mouse cursor to the center
  // of the window when it reaches the window borders to avoid it going outside.
  // This flag is used to differentiate between these synthetic mouse move
  // events vs. normal mouse move events.
  bool synthetic_move_sent_;

  // Used to track the state of the window we're created from. Only used when
  // created fullscreen.
  scoped_ptr<aura::WindowTracker> host_tracker_;

  // Used to track the last cursor visibility update that was sent to the
  // renderer via NotifyRendererOfCursorVisibilityState().
  enum CursorVisibilityState {
    UNKNOWN,
    VISIBLE,
    NOT_VISIBLE,
  };
  CursorVisibilityState cursor_visibility_state_in_renderer_;

#if defined(OS_WIN)
  // The list of rectangles from constrained windows over this view. Windowed
  // NPAPI plugins shouldn't draw over them.
  std::vector<gfx::Rect> constrained_rects_;

  typedef std::map<HWND, WebPluginGeometry> PluginWindowMoves;
  // Contains information about each windowed plugin's clip and cutout rects (
  // from the renderer). This is needed because when the transient windows
  // over this view changes, we need this information in order to create a new
  // region for the HWND.
  PluginWindowMoves plugin_window_moves_;

  // The LegacyRenderWidgetHostHWND class provides a dummy HWND which is used
  // for accessibility, as the container for windowless plugins like
  // Flash/Silverlight, etc and for legacy drivers for trackpoints/trackpads,
  // etc.
  // The LegacyRenderWidgetHostHWND instance is created during the first call
  // to RenderWidgetHostViewAura::InternalSetBounds. The instance is destroyed
  // when the LegacyRenderWidgetHostHWND hwnd is destroyed.
  content::LegacyRenderWidgetHostHWND* legacy_render_widget_host_HWND_;

  // Set to true if the legacy_render_widget_host_HWND_ instance was destroyed
  // by Windows. This could happen if the browser window was destroyed by
  // DestroyWindow for e.g. This flag helps ensure that we don't try to create
  // the LegacyRenderWidgetHostHWND instance again as that would be a futile
  // exercise.
  bool legacy_window_destroyed_;

  // Set to true when a context menu is being displayed. Reset to false when
  // a mouse leave is received in this context.
  bool showing_context_menu_;
#endif

  bool has_snapped_to_boundary_;

  TouchEditingClient* touch_editing_client_;

  scoped_ptr<OverscrollController> overscroll_controller_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  gfx::Insets insets_;

  std::vector<ui::LatencyInfo> software_latency_info_;

  scoped_ptr<aura::client::ScopedTooltipDisabler> tooltip_disabler_;

  // True when this view acts as a platform view hack for a
  // RenderWidgetHostViewGuest.
  bool is_guest_view_hack_;

  gfx::Rect disambiguation_target_rect_;

  // The last scroll offset when we start to render the link disambiguation
  // view, so we can ensure the window hasn't moved between copying from the
  // compositing surface and showing the disambiguation popup.
  gfx::Vector2dF disambiguation_scroll_offset_;

  BeginFrameObserverProxy begin_frame_observer_proxy_;

  // This flag when set ensures that we send over a notification to blink that
  // the current view has focus. Defaults to false.
  bool set_focus_on_mouse_down_;

  base::WeakPtrFactory<RenderWidgetHostViewAura> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
