// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_

#include <deque>
#include <map>

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/renderer/message_delivery_policy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/WebKit/public/web/WebTextInputInfo.h"
#include "third_party/WebKit/public/web/WebTouchAction.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

struct ViewHostMsg_UpdateRect_Params;
struct ViewMsg_Resize_Params;
class ViewHostMsg_UpdateRect;

namespace IPC {
class SyncMessage;
class SyncMessageFilter;
}

namespace blink {
struct WebDeviceEmulationParams;
class WebFrameWidget;
class WebGestureEvent;
class WebKeyboardEvent;
class WebLocalFrame;
class WebMouseEvent;
class WebNode;
struct WebPoint;
class WebTouchEvent;
class WebView;
}

namespace cc {
struct InputHandlerScrollResult;
class OutputSurface;
class SwapPromise;
}

namespace gfx {
class Range;
}

namespace content {
class CompositorDependencies;
class ExternalPopupMenu;
class FrameSwapMessageQueue;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderFrameProxy;
class RenderWidgetCompositor;
class RenderWidgetTest;
class ResizingModeSelector;
struct ContextMenuParams;
struct DidOverscrollParams;
struct WebPluginGeometry;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      NON_EXPORTED_BASE(virtual public blink::WebWidgetClient),
      public base::RefCounted<RenderWidget> {
 public:
  // Creates a new RenderWidget.  The opener_id is the routing ID of the
  // RenderView that this widget lives inside.
  static RenderWidget* Create(int32 opener_id,
                              CompositorDependencies* compositor_deps,
                              blink::WebPopupType popup_type,
                              const blink::WebScreenInfo& screen_info);

  // Creates a new RenderWidget that will be attached to a RenderFrame.
  static RenderWidget* CreateForFrame(int routing_id,
                                      int surface_id,
                                      bool hidden,
                                      const blink::WebScreenInfo& screen_info,
                                      CompositorDependencies* compositor_deps,
                                      blink::WebLocalFrame* frame);

  static blink::WebWidget* CreateWebFrameWidget(RenderWidget* render_widget,
                                                blink::WebLocalFrame* frame);

  // Creates a WebWidget based on the popup type.
  static blink::WebWidget* CreateWebWidget(RenderWidget* render_widget);

  int32 routing_id() const { return routing_id_; }
  int32 surface_id() const { return surface_id_; }
  CompositorDependencies* compositor_deps() const { return compositor_deps_; }
  blink::WebWidget* webwidget() const { return webwidget_; }
  gfx::Size size() const { return size_; }
  bool has_focus() const { return has_focus_; }
  bool is_fullscreen_granted() const { return is_fullscreen_granted_; }
  blink::WebDisplayMode display_mode() const { return display_mode_; }
  bool is_hidden() const { return is_hidden_; }
  bool handling_input_event() const { return handling_input_event_; }
  // Temporary for debugging purposes...
  bool closing() const { return closing_; }
  bool is_swapped_out() { return is_swapped_out_; }
  bool for_oopif() { return for_oopif_; }
  ui::MenuSourceType context_menu_source_type() {
    return context_menu_source_type_;
  }
  bool has_host_context_menu_location() {
    return has_host_context_menu_location_;
  }
  gfx::Point host_context_menu_location() {
    return host_context_menu_location_;
  }

  // ScreenInfo exposed so it can be passed to subframe RenderWidgets.
  blink::WebScreenInfo screen_info() const { return screen_info_; }

  // Functions to track out-of-process frames for special notifications.
  void RegisterRenderFrameProxy(RenderFrameProxy* proxy);
  void UnregisterRenderFrameProxy(RenderFrameProxy* proxy);

  // Functions to track all RenderFrame objects associated with this
  // RenderWidget.
  void RegisterRenderFrame(RenderFrameImpl* frame);
  void UnregisterRenderFrame(RenderFrameImpl* frame);

#if defined(VIDEO_HOLE)
  void RegisterVideoHoleFrame(RenderFrameImpl* frame);
  void UnregisterVideoHoleFrame(RenderFrameImpl* frame);
#endif  // defined(VIDEO_HOLE)

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // blink::WebWidgetClient
  virtual void didAutoResize(const blink::WebSize& new_size);
  virtual void initializeLayerTreeView();
  virtual blink::WebLayerTreeView* layerTreeView();
  virtual void didFocus();
  virtual void didBlur();
  virtual void didChangeCursor(const blink::WebCursorInfo&);
  virtual void closeWidgetSoon();
  virtual void show(blink::WebNavigationPolicy);
  virtual blink::WebRect windowRect();
  virtual void setToolTipText(const blink::WebString& text,
                              blink::WebTextDirection hint);
  virtual void setWindowRect(const blink::WebRect&);
  virtual blink::WebRect windowResizerRect();
  virtual blink::WebRect rootWindowRect();
  virtual blink::WebScreenInfo screenInfo();
  virtual float deviceScaleFactor();
  virtual void resetInputMethod();
  virtual void didHandleGestureEvent(const blink::WebGestureEvent& event,
                                     bool event_cancelled);
  virtual void didOverscroll(
      const blink::WebFloatSize& unusedDelta,
      const blink::WebFloatSize& accumulatedRootOverScroll,
      const blink::WebFloatPoint& position,
      const blink::WebFloatSize& velocity);
  virtual void showImeIfNeeded();

#if defined(OS_ANDROID)
  // Notifies that a tap was not consumed, so showing a UI for the unhandled
  // tap may be needed.
  // Performs various checks on the given WebNode to apply heuristics to
  // determine if triggering is appropriate.
  virtual void showUnhandledTapUIIfNeeded(
      const blink::WebPoint& tapped_position,
      const blink::WebNode& tapped_node,
      bool page_changed) override;
#endif

  // Begins the compositor's scheduler to start producing frames.
  void StartCompositor();

  // Stop compositing.
  void WillCloseLayerTreeView();

  // Called when a plugin is moved.  These events are queued up and sent with
  // the next paint or scroll message to the host.
  void SchedulePluginMove(const WebPluginGeometry& move);

  // Called when a plugin window has been destroyed, to make sure the currently
  // pending moves don't try to reference it.
  void CleanupWindowInPluginMoves(gfx::PluginWindowHandle window);

  RenderWidgetCompositor* compositor() const;

  const ui::LatencyInfo* current_event_latency_info() const {
    return current_event_latency_info_;
  }

  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback);

  // Callback for use with synthetic gestures (e.g. BeginSmoothScroll).
  typedef base::Callback<void()> SyntheticGestureCompletionCallback;

  // Send a synthetic gesture to the browser to be queued to the synthetic
  // gesture controller.
  void QueueSyntheticGesture(
      scoped_ptr<SyntheticGestureParams> gesture_params,
      const SyntheticGestureCompletionCallback& callback);

  // Close the underlying WebWidget.
  virtual void Close();

  // Deliveres |message| together with compositor state change updates. The
  // exact behavior depends on |policy|.
  // This mechanism is not a drop-in replacement for IPC: messages sent this way
  // will not be automatically available to BrowserMessageFilter, for example.
  // FIFO ordering is preserved between messages enqueued with the same
  // |policy|, the ordering between messages enqueued for different policies is
  // undefined.
  //
  // |msg| message to send, ownership of |msg| is transferred.
  // |policy| see the comment on MessageDeliveryPolicy.
  void QueueMessage(IPC::Message* msg, MessageDeliveryPolicy policy);

  // Handle common setup/teardown for handling IME events.
  void StartHandlingImeEvent();
  void FinishHandlingImeEvent();

  // Returns whether we currently should handle an IME event.
  bool ShouldHandleImeEvent();

  // Called by the compositor when page scale animation completed.
  virtual void DidCompletePageScaleAnimation() {}

  // When paused in debugger, we send ack for mouse event early. This ensures
  // that we continue receiving mouse moves and pass them to debugger. Returns
  // whether we are paused in mouse move event and have sent the ack.
  bool SendAckForMouseMoveFromDebugger();

  // When resumed from pause in debugger while handling mouse move,
  // we should not send an extra ack (see SendAckForMouseMoveFromDebugger).
  void IgnoreAckForMouseMoveFromDebugger();

  // ScreenMetricsEmulator class manages screen emulation inside a render
  // widget. This includes resizing, placing view on the screen at desired
  // position, changing device scale factor, and scaling down the whole
  // widget if required to fit into the browser window.
  class ScreenMetricsEmulator;

  void SetPopupOriginAdjustmentsForEmulation(ScreenMetricsEmulator* emulator);
  gfx::Rect AdjustValidationMessageAnchor(const gfx::Rect& anchor);

  // Indicates that the compositor is about to begin a frame. This is primarily
  // to signal to flow control mechanisms that a frame is beginning, not to
  // perform actual painting work.
  void WillBeginCompositorFrame();

  // Notifies about a compositor frame commit operation having finished.
  virtual void DidCommitCompositorFrame();

  // Notifies that the draw commands for a committed frame have been issued.
  void DidCommitAndDrawCompositorFrame();

  // Notifies that the compositor has posted a swapbuffers operation to the GPU
  // process.
  void DidCompleteSwapBuffers();

  void ScheduleComposite();
  void ScheduleCompositeWithForcedRedraw();

  // Called by the compositor in single-threaded mode when a swap is posted,
  // completes or is aborted.
  void OnSwapBuffersPosted();
  void OnSwapBuffersComplete();
  void OnSwapBuffersAborted();

  // Checks if the text input state and compose inline mode have been changed.
  // If they are changed, the new value will be sent to the browser process.
  void UpdateTextInputType();

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  void OnShowHostContextMenu(ContextMenuParams* params);

#if defined(OS_ANDROID) || defined(USE_AURA)
  enum ShowIme {
    SHOW_IME_IF_NEEDED,
    NO_SHOW_IME,
  };

  enum ChangeSource {
    FROM_NON_IME,
    FROM_IME,
  };

  // |show_ime| should be SHOW_IME_IF_NEEDED iff the update may cause the ime to
  // be displayed, e.g. after a tap on an input field on mobile.
  // |change_source| should be FROM_NON_IME when the renderer has to wait for
  // the browser to acknowledge the change before the renderer handles any more
  // IME events. This is when the text change did not originate from the IME in
  // the browser side, such as changes by JavaScript or autofill.
  void UpdateTextInputState(ShowIme show_ime, ChangeSource change_source);
#endif

  // Called when animations due to focus change have completed (if any). Can be
  // called from the renderer, browser, or compositor.
  virtual void FocusChangeComplete() {}

  // Checks if the composition range or composition character bounds have been
  // changed. If they are changed, the new value will be sent to the browser
  // process. This method does nothing when the browser process is not able to
  // handle composition range and composition character bounds.
  void UpdateCompositionInfo(bool should_update_range);

#if defined(OS_ANDROID)
  void DidChangeBodyBackgroundColor(SkColor bg_color);
  bool DoesRecordFullLayer() const;
#endif

  bool host_closing() const { return host_closing_; }

 protected:
  // Friend RefCounted so that the dtor can be non-public. Using this class
  // without ref-counting is an error.
  friend class base::RefCounted<RenderWidget>;
  // For unit tests.
  friend class RenderWidgetTest;

  enum ResizeAck {
    SEND_RESIZE_ACK,
    NO_RESIZE_ACK,
  };

  RenderWidget(blink::WebPopupType popup_type,
               const blink::WebScreenInfo& screen_info,
               bool swapped_out,
               bool hidden,
               bool never_visible);

  ~RenderWidget() override;

  // Initializes this view with the given opener.  CompleteInit must be called
  // later.
  bool Init(int32 opener_id, CompositorDependencies* compositor_deps);

  // Called by Init and subclasses to perform initialization.
  bool DoInit(int32 opener_id,
              CompositorDependencies* compositor_deps,
              blink::WebWidget* web_widget,
              IPC::SyncMessage* create_widget_message);

  // Finishes creation of a pending view started with Init.
  void CompleteInit();

  // Sets whether this RenderWidget has been swapped out to be displayed by
  // a RenderWidget in a different process.  If so, no new IPC messages will be
  // sent (only ACKs) and the process is free to exit when there are no other
  // active RenderWidgets.
  void SetSwappedOut(bool is_swapped_out);

  // Allows the process to exit once the unload handler has finished, if there
  // are no other active RenderWidgets.
  void WasSwappedOut();

  void FlushPendingInputEventAck();
  void DoDeferredClose();
  void DoDeferredSetWindowRect(const blink::WebRect& pos);
  void NotifyOnClose();

  // Resizes the render widget.
  void Resize(const gfx::Size& new_size,
              const gfx::Size& physical_backing_size,
              bool top_controls_shrink_blink_size,
              float top_controls_height,
              const gfx::Size& visible_viewport_size,
              const gfx::Rect& resizer_rect,
              bool is_fullscreen_granted,
              blink::WebDisplayMode display_mode,
              ResizeAck resize_ack);
  // Used to force the size of a window when running layout tests.
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);
  virtual void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params);
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void SetExternalPopupOriginAdjustmentsForEmulation(
      ExternalPopupMenu* popup, ScreenMetricsEmulator* emulator);
#endif

  // RenderWidget IPC message handlers
  void OnHandleInputEvent(const blink::WebInputEvent* event,
                          const ui::LatencyInfo& latency_info,
                          bool keyboard_shortcut);
  void OnCursorVisibilityChange(bool is_visible);
  void OnMouseCaptureLost();
  virtual void OnSetFocus(bool enable);
  void OnClose();
  void OnCreatingNewAck();
  virtual void OnResize(const ViewMsg_Resize_Params& params);
  void OnEnableDeviceEmulation(const blink::WebDeviceEmulationParams& params);
  void OnDisableDeviceEmulation();
  void OnColorProfile(const std::vector<char>& color_profile);
  void OnChangeResizeRect(const gfx::Rect& resizer_rect);
  virtual void OnWasHidden();
  virtual void OnWasShown(bool needs_repainting,
                          const ui::LatencyInfo& latency_info);
  void OnCreateVideoAck(int32 video_id);
  void OnUpdateVideoAck(int32 video_id);
  void OnRequestMoveAck();
  void OnSetInputMethodActive(bool is_active);
  virtual void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  virtual void OnImeConfirmComposition(const base::string16& text,
                                       const gfx::Range& replacement_range,
                                       bool keep_selection);
  void OnRepaint(gfx::Size size_to_paint);
  void OnSyntheticGestureCompleted();
  void OnSetTextDirection(blink::WebTextDirection direction);
  void OnGetFPS();
  void OnUpdateScreenRects(const gfx::Rect& view_screen_rect,
                           const gfx::Rect& window_screen_rect);
  void OnShowImeIfNeeded();
  void OnSetSurfaceIdNamespace(uint32_t surface_id_namespace);

#if defined(OS_ANDROID)
  // Whenever an IME event that needs an acknowledgement is sent to the browser,
  // the number of outstanding IME events that needs acknowledgement should be
  // incremented. All IME events will be dropped until we receive an ack from
  // the browser.
  void IncrementOutstandingImeEventAcks();

  // Called by the browser process for every required IME acknowledgement.
  void OnImeEventAck();
#endif

  // Notify the compositor about a change in viewport size. This should be
  // used only with auto resize mode WebWidgets, as normal WebWidgets should
  // go through OnResize.
  void AutoResizeCompositor();

  virtual void SetDeviceScaleFactor(float device_scale_factor);
  virtual bool SetDeviceColorProfile(const std::vector<char>& color_profile);
  virtual void ResetDeviceColorProfileForTesting();

  virtual void OnOrientationChange();

  // Override points to notify derived classes that a paint has happened.
  // DidInitiatePaint happens when that has completed, and subsequent rendering
  // won't affect the painted content. DidFlushPaint happens once we've received
  // the ACK that the screen has been updated. For a given paint operation,
  // these overrides will always be called in the order DidInitiatePaint,
  // DidFlushPaint.
  virtual void DidInitiatePaint() {}
  virtual void DidFlushPaint() {}

  virtual GURL GetURLForGraphicsContext3D();

  // Gets the scroll offset of this widget, if this widget has a notion of
  // scroll offset.
  virtual gfx::Vector2d GetScrollOffset();

  // Sets the "hidden" state of this widget.  All accesses to is_hidden_ should
  // use this method so that we can properly inform the RenderThread of our
  // state.
  void SetHidden(bool hidden);

  void WillToggleFullscreen();
  void DidToggleFullscreen();

  bool next_paint_is_resize_ack() const;
  void set_next_paint_is_resize_ack();
  void set_next_paint_is_repaint_ack();

  // QueueMessage implementation extracted into a static method for easy
  // testing.
  static scoped_ptr<cc::SwapPromise> QueueMessageImpl(
      IPC::Message* msg,
      MessageDeliveryPolicy policy,
      FrameSwapMessageQueue* frame_swap_message_queue,
      scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
      int source_frame_number);

  // Override point to obtain that the current input method state and caret
  // position.
  virtual ui::TextInputType GetTextInputType();
  virtual ui::TextInputType WebKitToUiTextInputType(
      blink::WebTextInputType type);

  // Override point to obtain that the current composition character bounds.
  // In the case of surrogate pairs, the character is treated as two characters:
  // the bounds for first character is actual one, and the bounds for second
  // character is zero width rectangle.
  virtual void GetCompositionCharacterBounds(
      std::vector<gfx::Rect>* character_bounds);

  // Returns the range of the text that is being composed or the selection if
  // the composition does not exist.
  virtual void GetCompositionRange(gfx::Range* range);

  // Returns true if the composition range or composition character bounds
  // should be sent to the browser process.
  bool ShouldUpdateCompositionInfo(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds);

  // Override point to obtain that the current input method state about
  // composition text.
  virtual bool CanComposeInline();

  // Tells the renderer it does not have focus. Used to prevent us from getting
  // the focus on our own when the browser did not focus us.
  void ClearFocus();

  // Set the pending window rect.
  // Because the real render_widget is hosted in another process, there is
  // a time period where we may have set a new window rect which has not yet
  // been processed by the browser.  So we maintain a pending window rect
  // size.  If JS code sets the WindowRect, and then immediately calls
  // GetWindowRect() we'll use this pending window rect as the size.
  void SetPendingWindowRect(const blink::WebRect& r);

  // Called by OnHandleInputEvent() to notify subclasses that a key event was
  // just handled.
  virtual void DidHandleKeyEvent() {}

  // Called by OnHandleInputEvent() to notify subclasses that a mouse event is
  // about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit or trigger DidHandleMouseEvent().
  virtual bool WillHandleMouseEvent(const blink::WebMouseEvent& event);

  // Called by OnHandleInputEvent() to notify subclasses that a gesture event is
  // about to be handled.
  // Returns true if no further handling is needed. In that case, the event
  // won't be sent to WebKit.
  virtual bool WillHandleGestureEvent(const blink::WebGestureEvent& event);

  // Called by OnHandleInputEvent() to forward a mouse wheel event to the
  // compositor thread, to effect the elastic overscroll effect.
  void ObserveWheelEventAndResult(const blink::WebMouseWheelEvent& wheel_event,
                                  const gfx::Vector2dF& wheel_unused_delta,
                                  bool event_processed);

  // Check whether the WebWidget has any touch event handlers registered
  // at the given point.
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const;

  // Check whether the WebWidget has any touch event handlers registered.
  virtual void hasTouchEventHandlers(bool has_handlers);

  // Tell the browser about the actions permitted for a new touch point.
  virtual void setTouchAction(blink::WebTouchAction touch_action);

  // Called when value of focused text field gets dirty, e.g. value is modified
  // by script, not by user input.
  virtual void didUpdateTextOfFocusedElementByNonUserInput();

  // Creates a 3D context associated with this view.
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateGraphicsContext3D();

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost. When MSG_ROUTING_NONE, no messages may be sent.
  int32 routing_id_;

  int32 surface_id_;

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* compositor_deps_;

  // We are responsible for destroying this object via its Close method.
  // May be NULL when the window is closing.
  blink::WebWidget* webwidget_;

  // This is lazily constructed and must not outlive webwidget_.
  scoped_ptr<RenderWidgetCompositor> compositor_;

  // Set to the ID of the view that initiated creating this view, if any. When
  // the view was initiated by the browser (the common case), this will be
  // MSG_ROUTING_NONE. This is used in determining ownership when opening
  // child tabs. See RenderWidget::createWebViewWithRequest.
  //
  // This ID may refer to an invalid view if that view is closed before this
  // view is.
  int32 opener_id_;

  // The rect where this view should be initially shown.
  gfx::Rect initial_rect_;

  bool init_complete_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  WebCursor current_cursor_;

  // The size of the RenderWidget.
  gfx::Size size_;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  gfx::Size physical_backing_size_;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar (always false on platforms where URL-bar hiding isn't supported).
  bool top_controls_shrink_blink_size_;

  // The height of the top controls (always 0 on platforms where URL-bar hiding
  // isn't supported).
  float top_controls_height_;

  // The size of the visible viewport in DPI-adjusted pixels.
  gfx::Size visible_viewport_size_;

  // The area that must be reserved for drawing the resize corner.
  gfx::Rect resizer_rect_;

  // Flags for the next ViewHostMsg_UpdateRect message.
  int next_paint_flags_;

  // Whether the WebWidget is in auto resize mode, which is used for example
  // by extension popups.
  bool auto_resize_mode_;

  // True if we need to send an UpdateRect message to notify the browser about
  // an already-completed auto-resize.
  bool need_update_rect_for_auto_resize_;

  // Set to true if we should ignore RenderWidget::Show calls.
  bool did_show_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  bool never_visible_;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted_;

  // Indicates the display mode.
  blink::WebDisplayMode display_mode_;

  // Indicates whether we have been focused/unfocused by the browser.
  bool has_focus_;

  // Are we currently handling an input event?
  bool handling_input_event_;

  // Used to intercept overscroll notifications while an event is being
  // handled. If the event causes overscroll, the overscroll metadata can be
  // bundled in the event ack, saving an IPC.  Note that we must continue
  // supporting overscroll IPC notifications due to fling animation updates.
  scoped_ptr<DidOverscrollParams>* handling_event_overscroll_;

  // Are we currently handling an ime event?
  bool handling_ime_event_;

  // Type of the input event we are currently handling.
  blink::WebInputEvent::Type handling_event_type_;

  // Whether we should not send ack for the current mouse move.
  bool ignore_ack_for_mouse_move_from_debugger_;

  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_;

  // True if it is known that the host is in the process of being shut down.
  bool host_closing_;

  // Whether this RenderWidget is currently swapped out, such that the view is
  // being rendered by another process.  If all RenderWidgets in a process are
  // swapped out, the process can exit.
  bool is_swapped_out_;

  // TODO(simonhong): Remove this when we enable BeginFrame scheduling for
  // OOPIF(crbug.com/471411).
  // Whether this RenderWidget is for an out-of-process iframe or not.
  bool for_oopif_;

  // Indicates if an input method is active in the browser process.
  bool input_method_is_active_;

  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_;

  // The kind of popup this widget represents, NONE if not a popup.
  blink::WebPopupType popup_type_;

  // Holds all the needed plugin window moves for a scroll.
  typedef std::vector<WebPluginGeometry> WebPluginGeometryVector;
  WebPluginGeometryVector plugin_window_moves_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_;
  blink::WebRect pending_window_rect_;

  // The screen rects of the view and the window that contains it.
  gfx::Rect view_screen_rect_;
  gfx::Rect window_screen_rect_;

  scoped_ptr<IPC::Message> pending_input_event_ack_;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  bool suppress_next_char_events_;

  // Properties of the screen hosting this RenderWidget instance.
  blink::WebScreenInfo screen_info_;

  // The device scale factor. This value is computed from the DPI entries in
  // |screen_info_| on some platforms, and defaults to 1 on other platforms.
  float device_scale_factor_;

  // The device color profile on supported platforms.
  std::vector<char> device_color_profile_;

  // State associated with synthetic gestures. Synthetic gestures are processed
  // in-order, so a queue is sufficient to identify the correct state for a
  // completed gesture.
  std::queue<SyntheticGestureCompletionCallback>
      pending_synthetic_gesture_callbacks_;

  const ui::LatencyInfo* current_event_latency_info_;

  uint32 next_output_surface_id_;

#if defined(OS_ANDROID)
  // Indicates value in the focused text field is in dirty state, i.e. modified
  // by script etc., not by user input.
  bool text_field_is_dirty_;

  // A counter for number of outstanding messages from the renderer to the
  // browser regarding IME-type events that have not been acknowledged by the
  // browser. If this value is not 0 IME events will be dropped.
  int outstanding_ime_acks_;

  // The background color of the document body element. This is used as the
  // default background color for filling the screen areas for which we don't
  // have the actual content.
  SkColor body_background_color_;
#endif

  scoped_ptr<ScreenMetricsEmulator> screen_metrics_emulator_;

  // Popups may be displaced when screen metrics emulation is enabled.
  // These values are used to properly adjust popup position.
  gfx::PointF popup_view_origin_for_emulation_;
  gfx::PointF popup_screen_origin_for_emulation_;
  float popup_origin_scale_for_emulation_;

  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;
  scoped_ptr<ResizingModeSelector> resizing_mode_selector_;

  // Lists of RenderFrameProxy objects that need to be notified of
  // compositing-related events (e.g. DidCommitCompositorFrame).
  base::ObserverList<RenderFrameProxy> render_frame_proxies_;
#if defined(VIDEO_HOLE)
  base::ObserverList<RenderFrameImpl> video_hole_frames_;
#endif  // defined(VIDEO_HOLE)

  // A list of RenderFrames associated with this RenderWidget. Notifications
  // are sent to each frame in the list for events such as changing
  // visibility state for example.
  base::ObserverList<RenderFrameImpl> render_frames_;

  ui::MenuSourceType context_menu_source_type_;
  bool has_host_context_menu_location_;
  gfx::Point host_context_menu_location_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_
