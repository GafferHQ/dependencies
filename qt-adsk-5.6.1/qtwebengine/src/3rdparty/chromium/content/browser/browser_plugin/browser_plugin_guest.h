// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest is the browser side of a browser <--> embedder
// renderer channel. A BrowserPlugin (a WebPlugin) is on the embedder
// renderer side of browser <--> embedder renderer communication.
//
// BrowserPluginGuest lives on the UI thread of the browser process. Any
// messages about the guest render process that the embedder might be interested
// in receiving should be listened for here.
//
// BrowserPluginGuest is a WebContentsObserver for the guest WebContents.
// BrowserPluginGuest operates under the assumption that the guest will be
// accessible through only one RenderViewHost for the lifetime of
// the guest WebContents. Thus, cross-process navigation is not supported.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <map>
#include <queue>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/common/edit_command.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/guest_host.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/platform/WebFocusType.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

struct BrowserPluginHostMsg_Attach_Params;
struct FrameHostMsg_CompositorFrameSwappedACK_Params;
struct FrameHostMsg_ReclaimCompositorResources_Params;
struct FrameMsg_CompositorFrameSwapped_Params;

#if defined(OS_MACOSX)
struct FrameHostMsg_ShowPopup_Params;
#endif

namespace cc {
class CompositorFrame;
struct SurfaceId;
struct SurfaceSequence;
}  // namespace cc

namespace gfx {
class Range;
}  // namespace gfx

namespace content {

class BrowserPluginGuestManager;
class RenderViewHostImpl;
class RenderWidgetHost;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class SiteInstance;
struct DropData;

// A browser plugin guest provides functionality for WebContents to operate in
// the guest role and implements guest-specific overrides for ViewHostMsg_*
// messages.
//
// When a guest is initially created, it is in an unattached state. That is,
// it is not visible anywhere and has no embedder WebContents assigned.
// A BrowserPluginGuest is said to be "attached" if it has an embedder.
// A BrowserPluginGuest can also create a new unattached guest via
// CreateNewWindow. The newly created guest will live in the same partition,
// which means it can share storage and can script this guest.
//
// Note: in --site-per-process, all IPCs sent out from this class will be
// dropped on the floor since we don't have a BrowserPlugin.
class CONTENT_EXPORT BrowserPluginGuest : public GuestHost,
                                          public WebContentsObserver {
 public:
  ~BrowserPluginGuest() override;

  // The WebContents passed into the factory method here has not been
  // initialized yet and so it does not yet hold a SiteInstance.
  // BrowserPluginGuest must be constructed and installed into a WebContents
  // prior to its initialization because WebContents needs to determine what
  // type of WebContentsView to construct on initialization. The content
  // embedder needs to be aware of |guest_site_instance| on the guest's
  // construction and so we pass it in here.
  static BrowserPluginGuest* Create(WebContentsImpl* web_contents,
                                    BrowserPluginGuestDelegate* delegate);

  // Returns whether the given WebContents is a BrowserPlugin guest.
  static bool IsGuest(WebContentsImpl* web_contents);

  // Returns whether the given RenderviewHost is a BrowserPlugin guest.
  static bool IsGuest(RenderViewHostImpl* render_view_host);

  // BrowserPluginGuest::Init is called after the associated guest WebContents
  // initializes. If this guest cannot navigate without being attached to a
  // container, then this call is a no-op. For guest types that can be
  // navigated, this call adds the associated RenderWdigetHostViewGuest to the
  // view hierachy and sets up the appropriate RendererPreferences so that this
  // guest can navigate and resize offscreen.
  void Init();

  // Returns a WeakPtr to this BrowserPluginGuest.
  base::WeakPtr<BrowserPluginGuest> AsWeakPtr();

  // Sets the focus state of the current RenderWidgetHostView.
  void SetFocus(RenderWidgetHost* rwh,
                bool focused,
                blink::WebFocusType focus_type);

  // Sets the tooltip text.
  void SetTooltipText(const base::string16& tooltip_text);

  // Sets the lock state of the pointer. Returns true if |allowed| is true and
  // the mouse has been successfully locked.
  bool LockMouse(bool allowed);

  // Return true if the mouse is locked.
  bool mouse_locked() const { return mouse_locked_; }

  // Called when the embedder WebContents changes visibility.
  void EmbedderVisibilityChanged(bool visible);

  // Creates a new guest WebContentsImpl with the provided |params| with |this|
  // as the |opener|.
  WebContentsImpl* CreateNewGuestWindow(
      const WebContents::CreateParams& params);

  // Creates, if necessary, and returns the routing ID of a proxy for the guest
  // in the owner's renderer process.
  int GetGuestProxyRoutingID();

  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int browser_plugin_instance_id() const { return browser_plugin_instance_id_; }

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message);

  WebContentsImpl* embedder_web_contents() const {
    return attached_ ? owner_web_contents_ : nullptr;
  }

  // Returns the embedder's RenderWidgetHostView if it is available.
  // Returns nullptr otherwise.
  RenderWidgetHostView* GetOwnerRenderWidgetHostView();

  bool focused() const { return focused_; }
  bool visible() const { return guest_visible_; }
  bool is_in_destruction() { return is_in_destruction_; }

  void UpdateVisibility();

  BrowserPluginGuestManager* GetBrowserPluginGuestManager() const;

  // WebContentsObserver implementation.
  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;

  void RenderViewReady() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

  // GuestHost implementation.
  int LoadURLWithParams(
      const NavigationController::LoadURLParams& load_params) override;
  void SizeContents(const gfx::Size& new_size) override;
  void WillDestroy() override;

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContentsImpl* GetWebContents() const;

  gfx::Point GetScreenCoordinates(const gfx::Point& relative_position) const;

  // Helper to send messages to embedder. If this guest is not yet attached,
  // then IPCs will be queued until attachment.
  void SendMessageToEmbedder(IPC::Message* msg);

  // Returns whether the guest is attached to an embedder.
  bool attached() const { return attached_; }

  // Returns true when an attachment has taken place since the last time the
  // compositor surface was set.
  bool has_attached_since_surface_set() const {
    return has_attached_since_surface_set_;
  }

  // Attaches this BrowserPluginGuest to the provided |embedder_web_contents|
  // and initializes the guest with the provided |params|. Attaching a guest
  // to an embedder implies that this guest's lifetime is no longer managed
  // by its opener, and it can begin loading resources.
  void Attach(int browser_plugin_instance_id,
              WebContentsImpl* embedder_web_contents,
              const BrowserPluginHostMsg_Attach_Params& params);

  // Returns whether BrowserPluginGuest is interested in receiving the given
  // |message|.
  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);

  void DragSourceEndedAt(int client_x, int client_y, int screen_x,
      int screen_y, blink::WebDragOperation operation);

  // Called when the drag started by this guest ends at an OS-level.
  void EmbedderSystemDragEnded();
  void EndSystemDragIfApplicable();

  void RespondToPermissionRequest(int request_id,
                                  bool should_allow,
                                  const std::string& user_input);

  void PointerLockPermissionResponse(bool allow);

  // The next three functions are virtual for test purposes.
  virtual void UpdateGuestSizeIfNecessary(const gfx::Size& frame_size,
                                          float scale_factor);
  virtual void SwapCompositorFrame(uint32 output_surface_id,
                                   int host_process_id,
                                   int host_routing_id,
                                   scoped_ptr<cc::CompositorFrame> frame);
  virtual void SetChildFrameSurface(const cc::SurfaceId& surface_id,
                                    const gfx::Size& frame_size,
                                    float scale_factor,
                                    const cc::SurfaceSequence& sequence);

  void SetContentsOpaque(bool opaque);

  // Find the given |search_text| in the page. Returns true if the find request
  // is handled by this browser plugin guest.
  bool Find(int request_id,
            const base::string16& search_text,
            const blink::WebFindOptions& options);
  bool StopFinding(StopFindAction action);

 protected:

  // BrowserPluginGuest is a WebContentsObserver of |web_contents| and
  // |web_contents| has to stay valid for the lifetime of BrowserPluginGuest.
  // Constructor protected for testing.
  BrowserPluginGuest(bool has_render_view,
                     WebContentsImpl* web_contents,
                     BrowserPluginGuestDelegate* delegate);

  // Protected for testing.
  void set_has_attached_since_surface_set_for_test(bool has_attached) {
    has_attached_since_surface_set_ = has_attached;
  }

  void set_attached_for_test(bool attached) {
    attached_ = attached;
  }

 private:
  class EmbedderVisibilityObserver;

  void InitInternal(const BrowserPluginHostMsg_Attach_Params& params,
                    WebContentsImpl* owner_web_contents);

  bool InAutoSizeBounds(const gfx::Size& size) const;

  void OnSatisfySequence(int instance_id, const cc::SurfaceSequence& sequence);
  void OnRequireSequence(int instance_id,
                         const cc::SurfaceId& id,
                         const cc::SurfaceSequence& sequence);
  // Message handlers for messages from embedder.
  void OnCompositorFrameSwappedACK(
      int instance_id,
      const FrameHostMsg_CompositorFrameSwappedACK_Params& params);
  void OnDetach(int instance_id);
  // Handles drag events from the embedder.
  // When dragging, the drag events go to the embedder first, and if the drag
  // happens on the browser plugin, then the plugin sends a corresponding
  // drag-message to the guest. This routes the drag-message to the guest
  // renderer.
  void OnDragStatusUpdate(int instance_id,
                          blink::WebDragStatus drag_status,
                          const DropData& drop_data,
                          blink::WebDragOperationsMask drag_mask,
                          const gfx::Point& location);
  // Instructs the guest to execute an edit command decoded in the embedder.
  void OnExecuteEditCommand(int instance_id,
                            const std::string& command);

  // Returns compositor resources reclaimed in the embedder to the guest.
  void OnReclaimCompositorResources(
      int instance_id,
      const FrameHostMsg_ReclaimCompositorResources_Params& params);

  void OnLockMouse(bool user_gesture,
                   bool last_unlocked_by_target,
                   bool privileged);
  void OnLockMouseAck(int instance_id, bool succeeded);
  // Resizes the guest's web contents.
  void OnSetFocus(int instance_id,
                  bool focused,
                  blink::WebFocusType focus_type);
  // Sets the name of the guest so that other guests in the same partition can
  // access it.
  void OnSetName(int instance_id, const std::string& name);
  // Updates the size state of the guest.
  void OnSetEditCommandsForNextKeyEvent(
      int instance_id,
      const std::vector<EditCommand>& edit_commands);
  // The guest WebContents is visible if both its embedder is visible and
  // the browser plugin element is visible. If either one is not then the
  // WebContents is marked as hidden. A hidden WebContents will consume
  // fewer GPU and CPU resources.
  //
  // When every WebContents in a RenderProcessHost is hidden, it will lower
  // the priority of the process (see RenderProcessHostImpl::WidgetHidden).
  //
  // It will also send a message to the guest renderer process to cleanup
  // resources such as dropping back buffers and adjusting memory limits (if in
  // compositing mode, see CCLayerTreeHost::setVisible).
  //
  // Additionally, it will slow down Javascript execution and garbage
  // collection. See RenderThreadImpl::IdleHandler (executed when hidden) and
  // RenderThreadImpl::IdleHandlerInForegroundTab (executed when visible).
  void OnSetVisibility(int instance_id, bool visible);
  void OnUnlockMouse();
  void OnUnlockMouseAck(int instance_id);
  void OnUpdateGeometry(int instance_id, const gfx::Rect& view_rect);

  void OnTextInputTypeChanged(ui::TextInputType type,
                              ui::TextInputMode input_mode,
                              bool can_compose_inline,
                              int flags);
  void OnImeSetComposition(
      int instance_id,
      const std::string& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void OnImeConfirmComposition(
      int instance_id,
      const std::string& text,
      bool keep_selection);
  void OnExtendSelectionAndDelete(int instance_id, int before, int after);
  void OnImeCancelComposition();
#if defined(OS_MACOSX) || defined(USE_AURA)
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds);
#endif

  // Message handlers for messages from guest.
  void OnHandleInputEventAck(
      blink::WebInputEvent::Type event_type,
      InputEventAckState ack_result);
  void OnHasTouchEventHandlers(bool accept);
#if defined(OS_MACOSX)
  // On MacOS X popups are painted by the browser process. We handle them here
  // so that they are positioned correctly.
  void OnShowPopup(RenderFrameHost* render_frame_host,
                   const FrameHostMsg_ShowPopup_Params& params);
#endif
  void OnShowWidget(int route_id, const gfx::Rect& initial_rect);
  void OnTakeFocus(bool reverse);
  void OnUpdateFrameName(int frame_id,
                         bool is_top_level,
                         const std::string& name);

  // Called when WillAttach is complete.
  void OnWillAttachComplete(WebContentsImpl* embedder_web_contents,
                            const BrowserPluginHostMsg_Attach_Params& params);

  // Forwards all messages from the |pending_messages_| queue to the embedder.
  void SendQueuedMessages();

  void SendTextInputTypeChangedToView(RenderWidgetHostViewBase* guest_rwhv);

  // The last tooltip that was set with SetTooltipText().
  base::string16 current_tooltip_text_;

  scoped_ptr<EmbedderVisibilityObserver> embedder_visibility_observer_;
  WebContentsImpl* owner_web_contents_;

  // Indicates whether this guest has been attached to a container.
  bool attached_;

  // Used to signal if a browser plugin has been attached since the last time
  // the compositing surface was set.
  bool has_attached_since_surface_set_;

  // An identifier that uniquely identifies a browser plugin within an embedder.
  int browser_plugin_instance_id_;
  gfx::Rect guest_window_rect_;
  bool focused_;
  bool mouse_locked_;
  bool pending_lock_request_;
  bool guest_visible_;
  bool embedder_visible_;
  // Whether the browser plugin is inside a plugin document.
  bool is_full_page_plugin_;

  // Indicates that this BrowserPluginGuest has associated renderer-side state.
  // This is used to determine whether or not to create a new RenderView when
  // this guest is attached. A BrowserPluginGuest would have renderer-side state
  // prior to attachment if it is created via a call to window.open and
  // maintains a JavaScript reference to its opener.
  bool has_render_view_;

  // Last seen size of guest contents (by SwapCompositorFrame).
  gfx::Size last_seen_view_size_;

  bool is_in_destruction_;

  // BrowserPluginGuest::Init can only be called once. This flag allows it to
  // exit early if it's already been called.
  bool initialized_;

  // Text input type states.
  ui::TextInputType last_text_input_type_;
  ui::TextInputMode last_input_mode_;
  int last_input_flags_;
  bool last_can_compose_inline_;

  // The is the routing ID for a swapped out RenderView for the guest
  // WebContents in the embedder's process.
  int guest_proxy_routing_id_;
  // Last seen state of drag status update.
  blink::WebDragStatus last_drag_status_;
  // Whether or not our embedder has seen a SystemDragEnded() call.
  bool seen_embedder_system_drag_ended_;
  // Whether or not our embedder has seen a DragSourceEndedAt() call.
  bool seen_embedder_drag_source_ended_at_;

  // Indicates the URL dragged into the guest if any.
  GURL dragged_url_;

  // Guests generate frames and send a CompositorFrameSwapped (CFS) message
  // indicating the next frame is ready to be positioned and composited.
  // Subsequent frames are not generated until the IPC is ACKed. We would like
  // to ensure that the guest generates frames on attachment so we directly ACK
  // an unACKed CFS. ACKs could get lost between the time a guest is detached
  // from a container and the time it is attached elsewhere. This mitigates this
  // race by ensuring the guest is ACKed on attachment.
  scoped_ptr<FrameMsg_CompositorFrameSwapped_Params> last_pending_frame_;

  // This is a queue of messages that are destined to be sent to the embedder
  // once the guest is attached to a particular embedder.
  std::deque<linked_ptr<IPC::Message> > pending_messages_;

  BrowserPluginGuestDelegate* const delegate_;

  // Weak pointer used to ask GeolocationPermissionContext about geolocation
  // permission.
  base::WeakPtrFactory<BrowserPluginGuest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
