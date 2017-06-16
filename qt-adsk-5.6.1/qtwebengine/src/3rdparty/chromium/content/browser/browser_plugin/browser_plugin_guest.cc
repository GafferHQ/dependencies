// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_guest.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/guest_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined(OS_MACOSX)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#include "content/common/frame_messages.h"
#endif

namespace content {

class BrowserPluginGuest::EmbedderVisibilityObserver
    : public WebContentsObserver {
 public:
  explicit EmbedderVisibilityObserver(BrowserPluginGuest* guest)
      : WebContentsObserver(guest->embedder_web_contents()),
        browser_plugin_guest_(guest) {
  }

  ~EmbedderVisibilityObserver() override {}

  // WebContentsObserver implementation.
  void WasShown() override {
    browser_plugin_guest_->EmbedderVisibilityChanged(true);
  }

  void WasHidden() override {
    browser_plugin_guest_->EmbedderVisibilityChanged(false);
  }

 private:
  BrowserPluginGuest* browser_plugin_guest_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderVisibilityObserver);
};

BrowserPluginGuest::BrowserPluginGuest(bool has_render_view,
                                       WebContentsImpl* web_contents,
                                       BrowserPluginGuestDelegate* delegate)
    : WebContentsObserver(web_contents),
      owner_web_contents_(nullptr),
      attached_(false),
      has_attached_since_surface_set_(false),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      focused_(false),
      mouse_locked_(false),
      pending_lock_request_(false),
      guest_visible_(false),
      embedder_visible_(true),
      is_full_page_plugin_(false),
      has_render_view_(has_render_view),
      is_in_destruction_(false),
      initialized_(false),
      last_text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      last_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
      last_input_flags_(0),
      last_can_compose_inline_(true),
      guest_proxy_routing_id_(MSG_ROUTING_NONE),
      last_drag_status_(blink::WebDragStatusUnknown),
      seen_embedder_system_drag_ended_(false),
      seen_embedder_drag_source_ended_at_(false),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  DCHECK(web_contents);
  DCHECK(delegate);
  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Create"));
  web_contents->SetBrowserPluginGuest(this);
  delegate->SetGuestHost(this);
}

int BrowserPluginGuest::GetGuestProxyRoutingID() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    // We don't use the proxy to send postMessage in --site-per-process, since
    // we use the contentWindow directly from the frame element instead.
    return MSG_ROUTING_NONE;
  }

  if (guest_proxy_routing_id_ != MSG_ROUTING_NONE)
    return guest_proxy_routing_id_;

  // Create a swapped out RenderView for the guest in the embedder renderer
  // process, so that the embedder can access the guest's window object.
  // On reattachment, we can reuse the same swapped out RenderView because
  // the embedder process will always be the same even if the embedder
  // WebContents changes.
  //
  // TODO(fsamuel): Make sure this works for transferring guests across
  // owners in different processes. We probably need to clear the
  // |guest_proxy_routing_id_| and perform any necessary cleanup on Detach
  // to enable this.
  SiteInstance* owner_site_instance = owner_web_contents_->GetSiteInstance();
  guest_proxy_routing_id_ =
      GetWebContents()->CreateSwappedOutRenderView(owner_site_instance);

  return guest_proxy_routing_id_;
}

int BrowserPluginGuest::LoadURLWithParams(
    const NavigationController::LoadURLParams& load_params) {
  GetWebContents()->GetController().LoadURLWithParams(load_params);
  return GetGuestProxyRoutingID();
}

void BrowserPluginGuest::SizeContents(const gfx::Size& new_size) {
  GetWebContents()->GetView()->SizeContents(new_size);
}

void BrowserPluginGuest::WillDestroy() {
  is_in_destruction_ = true;
  owner_web_contents_ = nullptr;
  attached_ = false;
}

void BrowserPluginGuest::Init() {
  if (initialized_)
    return;
  initialized_ = true;

  // TODO(fsamuel): Initiailization prior to attachment should be behind a
  // command line flag once we introduce experimental guest types that rely on
  // this functionality.
  if (!delegate_->CanRunInDetachedState())
    return;

  WebContentsImpl* owner_web_contents = static_cast<WebContentsImpl*>(
      delegate_->GetOwnerWebContents());
  owner_web_contents->CreateBrowserPluginEmbedderIfNecessary();
  InitInternal(BrowserPluginHostMsg_Attach_Params(), owner_web_contents);
}

base::WeakPtr<BrowserPluginGuest> BrowserPluginGuest::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BrowserPluginGuest::SetFocus(RenderWidgetHost* rwh,
                                  bool focused,
                                  blink::WebFocusType focus_type) {
  focused_ = focused;
  if (!rwh)
    return;

  if ((focus_type == blink::WebFocusTypeForward) ||
      (focus_type == blink::WebFocusTypeBackward)) {
    static_cast<RenderViewHostImpl*>(RenderViewHost::From(rwh))->
        SetInitialFocus(focus_type == blink::WebFocusTypeBackward);
  }
  rwh->Send(new InputMsg_SetFocus(rwh->GetRoutingID(), focused));
  if (!focused && mouse_locked_)
    OnUnlockMouse();

  // Restore the last seen state of text input to the view.
  RenderWidgetHostViewBase* rwhv = static_cast<RenderWidgetHostViewBase*>(
      rwh->GetView());
  SendTextInputTypeChangedToView(rwhv);
}

void BrowserPluginGuest::SetTooltipText(const base::string16& tooltip_text) {
  if (tooltip_text == current_tooltip_text_)
    return;
  current_tooltip_text_ = tooltip_text;

  SendMessageToEmbedder(new BrowserPluginMsg_SetTooltipText(
      browser_plugin_instance_id_, tooltip_text));
}

bool BrowserPluginGuest::LockMouse(bool allowed) {
  if (!attached() || (mouse_locked_ == allowed))
    return false;

  return embedder_web_contents()->GotResponseToLockMouseRequest(allowed);
}

WebContentsImpl* BrowserPluginGuest::CreateNewGuestWindow(
    const WebContents::CreateParams& params) {
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(delegate_->CreateNewGuestWindow(params));
  DCHECK(new_contents);
  return new_contents;
}

bool BrowserPluginGuest::OnMessageReceivedFromEmbedder(
    const IPC::Message& message) {
  RenderWidgetHostViewGuest* rwhv = static_cast<RenderWidgetHostViewGuest*>(
      web_contents()->GetRenderWidgetHostView());
  // Until the guest is attached, it should not be handling input events.
  if (attached() && rwhv && rwhv->OnMessageReceivedFromEmbedder(
          message,
          static_cast<RenderViewHostImpl*>(
              embedder_web_contents()->GetRenderViewHost()))) {
    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CompositorFrameSwappedACK,
                        OnCompositorFrameSwappedACK)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_DragStatusUpdate,
                        OnDragStatusUpdate)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ExecuteEditCommand,
                        OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ExtendSelectionAndDelete,
                        OnExtendSelectionAndDelete)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ImeConfirmComposition,
                        OnImeConfirmComposition)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ImeSetComposition,
                        OnImeSetComposition)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_LockMouse_ACK, OnLockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ReclaimCompositorResources,
                        OnReclaimCompositorResources)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetVisibility, OnSetVisibility)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UnlockMouse_ACK, OnUnlockMouseAck)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UpdateGeometry, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SatisfySequence, OnSatisfySequence)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_RequireSequence, OnRequireSequence)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginGuest::InitInternal(
    const BrowserPluginHostMsg_Attach_Params& params,
    WebContentsImpl* owner_web_contents) {
  focused_ = params.focused;
  OnSetFocus(browser_plugin::kInstanceIDNone,
             focused_,
             blink::WebFocusTypeNone);

  guest_visible_ = params.visible;
  UpdateVisibility();

  is_full_page_plugin_ = params.is_full_page_plugin;
  guest_window_rect_ = params.view_rect;

  if (owner_web_contents_ != owner_web_contents) {
    WebContentsViewGuest* new_view = nullptr;
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSitePerProcess)) {
      new_view =
          static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
    }

    if (owner_web_contents_ && new_view)
      new_view->OnGuestDetached(owner_web_contents_->GetView());

    // Once a BrowserPluginGuest has an embedder WebContents, it's considered to
    // be attached.
    owner_web_contents_ = owner_web_contents;
    if (new_view)
      new_view->OnGuestAttached(owner_web_contents_->GetView());
  }

  RendererPreferences* renderer_prefs =
      GetWebContents()->GetMutableRendererPrefs();
  std::string guest_user_agent_override = renderer_prefs->user_agent_override;
  // Copy renderer preferences (and nothing else) from the embedder's
  // WebContents to the guest.
  //
  // For GTK and Aura this is necessary to get proper renderer configuration
  // values for caret blinking interval, colors related to selection and
  // focus.
  *renderer_prefs = *owner_web_contents_->GetMutableRendererPrefs();
  renderer_prefs->user_agent_override = guest_user_agent_override;

  // We would like the guest to report changes to frame names so that we can
  // update the BrowserPlugin's corresponding 'name' attribute.
  // TODO(fsamuel): Remove this once http://crbug.com/169110 is addressed.
  renderer_prefs->report_frame_name_changes = true;
  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  renderer_prefs->browser_handles_all_top_level_requests = false;
  // Disable "client blocked" error page for browser plugin.
  renderer_prefs->disable_client_blocked_error_page = true;

  embedder_visibility_observer_.reset(new EmbedderVisibilityObserver(this));

  DCHECK(GetWebContents()->GetRenderViewHost());

  // Initialize the device scale factor by calling |NotifyScreenInfoChanged|.
  auto render_widget_host =
      RenderWidgetHostImpl::From(GetWebContents()->GetRenderViewHost());
  render_widget_host->NotifyScreenInfoChanged();

  // TODO(chrishtr): this code is wrong. The navigate_on_drag_drop field will
  // be reset again the next time preferences are updated.
  WebPreferences prefs =
      GetWebContents()->GetRenderViewHost()->GetWebkitPreferences();
  prefs.navigate_on_drag_drop = false;
  GetWebContents()->GetRenderViewHost()->UpdateWebkitPreferences(prefs);
}

BrowserPluginGuest::~BrowserPluginGuest() {
}

// static
BrowserPluginGuest* BrowserPluginGuest::Create(
    WebContentsImpl* web_contents,
    BrowserPluginGuestDelegate* delegate) {
  return new BrowserPluginGuest(
      web_contents->HasOpener(), web_contents, delegate);
}

// static
bool BrowserPluginGuest::IsGuest(WebContentsImpl* web_contents) {
  return web_contents && web_contents->GetBrowserPluginGuest();
}

// static
bool BrowserPluginGuest::IsGuest(RenderViewHostImpl* render_view_host) {
  return render_view_host && IsGuest(
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(
          render_view_host)));
}

RenderWidgetHostView* BrowserPluginGuest::GetOwnerRenderWidgetHostView() {
  if (!owner_web_contents_)
    return nullptr;
  return owner_web_contents_->GetRenderWidgetHostView();
}

void BrowserPluginGuest::UpdateVisibility() {
  OnSetVisibility(browser_plugin_instance_id(), visible());
}

BrowserPluginGuestManager*
BrowserPluginGuest::GetBrowserPluginGuestManager() const {
  return GetWebContents()->GetBrowserContext()->GetGuestManager();
}

void BrowserPluginGuest::EmbedderVisibilityChanged(bool visible) {
  embedder_visible_ = visible;
  UpdateVisibility();
}

void BrowserPluginGuest::PointerLockPermissionResponse(bool allow) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_SetMouseLock(browser_plugin_instance_id(), allow));
}

void BrowserPluginGuest::UpdateGuestSizeIfNecessary(
    const gfx::Size& frame_size, float scale_factor) {
  gfx::Size view_size(
      gfx::ToFlooredSize(gfx::ScaleSize(frame_size, 1.0f / scale_factor)));

  if (last_seen_view_size_ != view_size) {
    delegate_->GuestSizeChanged(view_size);
    last_seen_view_size_ = view_size;
  }
}

// TODO(wjmaclean): Remove this once any remaining users of this pathway
// are gone.
void BrowserPluginGuest::SwapCompositorFrame(
    uint32 output_surface_id,
    int host_process_id,
    int host_routing_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  cc::RenderPass* root_pass =
      frame->delegated_frame_data->render_pass_list.back();
  UpdateGuestSizeIfNecessary(root_pass->output_rect.size(),
                             frame->metadata.device_scale_factor);

  last_pending_frame_.reset(new FrameMsg_CompositorFrameSwapped_Params());
  frame->AssignTo(&last_pending_frame_->frame);
  last_pending_frame_->output_surface_id = output_surface_id;
  last_pending_frame_->producing_route_id = host_routing_id;
  last_pending_frame_->producing_host_id = host_process_id;

  SendMessageToEmbedder(
      new BrowserPluginMsg_CompositorFrameSwapped(
          browser_plugin_instance_id(), *last_pending_frame_));
}

void BrowserPluginGuest::SetChildFrameSurface(
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float scale_factor,
    const cc::SurfaceSequence& sequence) {
  has_attached_since_surface_set_ = false;
  SendMessageToEmbedder(new BrowserPluginMsg_SetChildFrameSurface(
      browser_plugin_instance_id(), surface_id, frame_size, scale_factor,
      sequence));
}

void BrowserPluginGuest::OnSatisfySequence(
    int instance_id,
    const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  cc::SurfaceManager* manager = GetSurfaceManager();
  manager->DidSatisfySequences(sequence.id_namespace, &sequences);
}

void BrowserPluginGuest::OnRequireSequence(
    int instance_id,
    const cc::SurfaceId& id,
    const cc::SurfaceSequence& sequence) {
  cc::SurfaceManager* manager = GetSurfaceManager();
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

void BrowserPluginGuest::SetContentsOpaque(bool opaque) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_SetContentsOpaque(
          browser_plugin_instance_id(), opaque));
}

bool BrowserPluginGuest::Find(int request_id,
                              const base::string16& search_text,
                              const blink::WebFindOptions& options) {
  return delegate_->Find(request_id, search_text, options);
}

bool BrowserPluginGuest::StopFinding(StopFindAction action) {
  return delegate_->StopFinding(action);
}

WebContentsImpl* BrowserPluginGuest::GetWebContents() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

gfx::Point BrowserPluginGuest::GetScreenCoordinates(
    const gfx::Point& relative_position) const {
  if (!attached())
    return relative_position;

  gfx::Point screen_pos(relative_position);
  screen_pos += guest_window_rect_.OffsetFromOrigin();
  if (embedder_web_contents()->GetBrowserPluginGuest()) {
     BrowserPluginGuest* embedder_guest =
        embedder_web_contents()->GetBrowserPluginGuest();
     screen_pos += embedder_guest->guest_window_rect_.OffsetFromOrigin();
  }
  return screen_pos;
}

void BrowserPluginGuest::SendMessageToEmbedder(IPC::Message* msg) {
  // During tests, attache() may be true when there is no owner_web_contents_;
  // in this case just queue any messages we receive.
  if (!attached() || !owner_web_contents_) {
    // Some pages such as data URLs, javascript URLs, and about:blank
    // do not load external resources and so they load prior to attachment.
    // As a result, we must save all these IPCs until attachment and then
    // forward them so that the embedder gets a chance to see and process
    // the load events.
    pending_messages_.push_back(linked_ptr<IPC::Message>(msg));
    return;
  }
  owner_web_contents_->Send(msg);
}

void BrowserPluginGuest::DragSourceEndedAt(int client_x, int client_y,
    int screen_x, int screen_y, blink::WebDragOperation operation) {
  web_contents()->GetRenderViewHost()->DragSourceEndedAt(client_x, client_y,
      screen_x, screen_y, operation);
  seen_embedder_drag_source_ended_at_ = true;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::EndSystemDragIfApplicable() {
  // Ideally we'd want either WebDragStatusDrop or WebDragStatusLeave...
  // Call guest RVH->DragSourceSystemDragEnded() correctly on the guest where
  // the drag was initiated. Calling DragSourceSystemDragEnded() correctly
  // means we call it in all cases and also make sure we only call it once.
  // This ensures that the input state of the guest stays correct, otherwise
  // it will go stale and won't accept any further input events.
  //
  // The strategy used here to call DragSourceSystemDragEnded() on the RVH
  // is when the following conditions are met:
  //   a. Embedder has seen SystemDragEnded()
  //   b. Embedder has seen DragSourceEndedAt()
  //   c. The guest has seen some drag status update other than
  //      WebDragStatusUnknown. Note that this step should ideally be done
  //      differently: The guest has seen at least one of
  //      {WebDragStatusOver, WebDragStatusDrop}. However, if a user drags
  //      a source quickly outside of <webview> bounds, then the
  //      BrowserPluginGuest never sees any of these drag status updates,
  //      there we just check whether we've seen any drag status update or
  //      not.
  if (last_drag_status_ != blink::WebDragStatusOver &&
      seen_embedder_drag_source_ended_at_ && seen_embedder_system_drag_ended_) {
    RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost());
    guest_rvh->DragSourceSystemDragEnded();
    last_drag_status_ = blink::WebDragStatusUnknown;
    seen_embedder_system_drag_ended_ = false;
    seen_embedder_drag_source_ended_at_ = false;
    dragged_url_ = GURL();
  }
}

void BrowserPluginGuest::EmbedderSystemDragEnded() {
  seen_embedder_system_drag_ended_ = true;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::SendQueuedMessages() {
  if (!attached())
    return;

  while (!pending_messages_.empty()) {
    linked_ptr<IPC::Message> message_ptr = pending_messages_.front();
    pending_messages_.pop_front();
    SendMessageToEmbedder(message_ptr.release());
  }
}

void BrowserPluginGuest::SendTextInputTypeChangedToView(
    RenderWidgetHostViewBase* guest_rwhv) {
  if (!guest_rwhv)
    return;

  if (!owner_web_contents_) {
    // If we were showing an interstitial, then we can end up here during
    // embedder shutdown or when the embedder navigates to a different page.
    // The call stack is roughly:
    // BrowserPluginGuest::SetFocus()
    // content::InterstitialPageImpl::Hide()
    // content::InterstitialPageImpl::DontProceed().
    //
    // TODO(lazyboy): Write a WebUI test once http://crbug.com/463674 is fixed.
    return;
  }

  guest_rwhv->TextInputTypeChanged(last_text_input_type_, last_input_mode_,
                                   last_can_compose_inline_, last_input_flags_);
  // Enable input method for guest if it's enabled for the embedder.
  if (!static_cast<RenderViewHostImpl*>(
           owner_web_contents_->GetRenderViewHost())->input_method_active()) {
    return;
  }
  RenderViewHostImpl* guest_rvh =
      static_cast<RenderViewHostImpl*>(GetWebContents()->GetRenderViewHost());
  guest_rvh->SetInputMethodActive(true);
}

void BrowserPluginGuest::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.DidNavigate"));
}

void BrowserPluginGuest::RenderViewReady() {
  RenderViewHost* rvh = GetWebContents()->GetRenderViewHost();
  // TODO(fsamuel): Investigate whether it's possible to update state earlier
  // here (see http://crbug.com/158151).
  Send(new InputMsg_SetFocus(routing_id(), focused_));
  UpdateVisibility();

  RenderWidgetHostImpl::From(rvh)->set_hung_renderer_delay(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs));
}

void BrowserPluginGuest::RenderProcessGone(base::TerminationStatus status) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_GuestGone(browser_plugin_instance_id()));
  switch (status) {
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Killed"));
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Crashed"));
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      RecordAction(
          base::UserMetricsAction("BrowserPlugin.Guest.AbnormalDeath"));
      break;
    default:
      break;
  }
}

// static
bool BrowserPluginGuest::ShouldForwardToBrowserPluginGuest(
    const IPC::Message& message) {
  return (message.type() != BrowserPluginHostMsg_Attach::ID) &&
      (IPC_MESSAGE_CLASS(message) == BrowserPluginMsgStart);
}

bool BrowserPluginGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  // In --site-per-process, we do not need most of BrowserPluginGuest to drive
  // inner WebContents.
  // Right now InputHostMsg_ImeCompositionRangeChanged hits NOTREACHED() in
  // RWHVChildFrame, so we're disabling message handling entirely here.
  // TODO(lazyboy): Fix this as part of http://crbug.com/330264. The required
  // parts of code from this class should be extracted to a separate class for
  // --site-per-process.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return false;
  }

  IPC_BEGIN_MESSAGE_MAP(BrowserPluginGuest, message)
    IPC_MESSAGE_HANDLER(InputHostMsg_ImeCancelComposition,
                        OnImeCancelComposition)
#if defined(OS_MACOSX) || defined(USE_AURA)
    IPC_MESSAGE_HANDLER(InputHostMsg_ImeCompositionRangeChanged,
                        OnImeCompositionRangeChanged)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse, OnLockMouse)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TakeFocus, OnTakeFocus)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputTypeChanged,
                        OnTextInputTypeChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse, OnUnlockMouse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool BrowserPluginGuest::OnMessageReceived(const IPC::Message& message,
                                           RenderFrameHost* render_frame_host) {
  // This will eventually be the home for more IPC handlers that depend on
  // RenderFrameHost. Until more are moved here, though, the IPC_* macros won't
  // compile if there are no handlers for a platform. So we have both #if guards
  // around the whole thing (unfortunate but temporary), and #if guards where
  // they belong, only around the one IPC handler. TODO(avi): Move more of the
  // frame-based handlers to this function and remove the outer #if layer.
#if defined(OS_MACOSX)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(BrowserPluginGuest, message,
                                   render_frame_host)
#if defined(OS_MACOSX)
    // MacOS X creates and populates platform-specific select drop-down menus
    // whereas other platforms merely create a popup window that the guest
    // renderer process paints inside.
    IPC_MESSAGE_HANDLER(FrameHostMsg_ShowPopup, OnShowPopup)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else
  return false;
#endif
}

void BrowserPluginGuest::Attach(
    int browser_plugin_instance_id,
    WebContentsImpl* embedder_web_contents,
    const BrowserPluginHostMsg_Attach_Params& params) {
  browser_plugin_instance_id_ = browser_plugin_instance_id;
  // If a guest is detaching from one container and attaching to another
  // container, then late arriving ACKs may be lost if the mapping from
  // |browser_plugin_instance_id| to |guest_instance_id| changes. Thus we
  // ensure that we always get new frames on attachment by ACKing the pending
  // frame if it's still waiting on the ACK.
  if (last_pending_frame_) {
    cc::CompositorFrameAck ack;
    RenderWidgetHostImpl::SendSwapCompositorFrameAck(
        last_pending_frame_->producing_route_id,
        last_pending_frame_->output_surface_id,
        last_pending_frame_->producing_host_id,
        ack);
    last_pending_frame_.reset();
  }

  // The guest is owned by the embedder. Attach is queued up so we cannot
  // change embedders before attach completes. If the embedder goes away,
  // so does the guest and so we will never call WillAttachComplete because
  // we have a weak ptr.
  delegate_->WillAttach(embedder_web_contents, browser_plugin_instance_id,
                        params.is_full_page_plugin,
                        base::Bind(&BrowserPluginGuest::OnWillAttachComplete,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   embedder_web_contents, params));
}

void BrowserPluginGuest::OnWillAttachComplete(
    WebContentsImpl* embedder_web_contents,
    const BrowserPluginHostMsg_Attach_Params& params) {
  bool use_site_per_process = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSitePerProcess);
  // If a RenderView has already been created for this new window, then we need
  // to initialize the browser-side state now so that the RenderFrameHostManager
  // does not create a new RenderView on navigation.
  if (!use_site_per_process && has_render_view_) {
    // This will trigger a callback to RenderViewReady after a round-trip IPC.
    static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost())->Init();
    WebContentsViewGuest* web_contents_view =
        static_cast<WebContentsViewGuest*>(GetWebContents()->GetView());
    if (!web_contents()->GetRenderViewHost()->GetView()) {
      web_contents_view->CreateViewForWidget(
          web_contents()->GetRenderViewHost(), true);
    }
  }

  InitInternal(params, embedder_web_contents);

  attached_ = true;
  has_attached_since_surface_set_ = true;
  SendQueuedMessages();

  delegate_->DidAttach(GetGuestProxyRoutingID());

  if (!use_site_per_process) {
    has_render_view_ = true;

    // Enable input method for guest if it's enabled for the embedder.
    if (static_cast<RenderViewHostImpl*>(
            owner_web_contents_->GetRenderViewHost())->input_method_active()) {
      RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
          GetWebContents()->GetRenderViewHost());
      guest_rvh->SetInputMethodActive(true);
    }
  }

  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Attached"));
}

void BrowserPluginGuest::OnCompositorFrameSwappedACK(
    int browser_plugin_instance_id,
    const FrameHostMsg_CompositorFrameSwappedACK_Params& params) {
  RenderWidgetHostImpl::SendSwapCompositorFrameAck(params.producing_route_id,
                                                   params.output_surface_id,
                                                   params.producing_host_id,
                                                   params.ack);
  last_pending_frame_.reset();
}

void BrowserPluginGuest::OnDetach(int browser_plugin_instance_id) {
  if (!attached())
    return;

  // This tells BrowserPluginGuest to queue up all IPCs to BrowserPlugin until
  // it's attached again.
  attached_ = false;

  delegate_->DidDetach();
}

void BrowserPluginGuest::OnDragStatusUpdate(int browser_plugin_instance_id,
                                            blink::WebDragStatus drag_status,
                                            const DropData& drop_data,
                                            blink::WebDragOperationsMask mask,
                                            const gfx::Point& location) {
  RenderViewHost* host = GetWebContents()->GetRenderViewHost();
  auto embedder = owner_web_contents_->GetBrowserPluginEmbedder();
  switch (drag_status) {
    case blink::WebDragStatusEnter:
      // Only track the URL being dragged over the guest if the link isn't
      // coming from the guest.
      if (!embedder->DragEnteredGuest(this))
        dragged_url_ = drop_data.url;
      host->DragTargetDragEnter(drop_data, location, location, mask, 0);
      break;
    case blink::WebDragStatusOver:
      host->DragTargetDragOver(location, location, mask, 0);
      break;
    case blink::WebDragStatusLeave:
      embedder->DragLeftGuest(this);
      host->DragTargetDragLeave();
      break;
    case blink::WebDragStatusDrop:
      host->DragTargetDrop(location, location, 0);
      if (dragged_url_.is_valid()) {
        delegate_->DidDropLink(dragged_url_);
        dragged_url_ = GURL();
      }
      break;
    case blink::WebDragStatusUnknown:
      NOTREACHED();
  }
  last_drag_status_ = drag_status;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::OnExecuteEditCommand(int browser_plugin_instance_id,
                                              const std::string& name) {
  RenderFrameHost* focused_frame = web_contents()->GetFocusedFrame();
  if (!focused_frame)
    return;

  focused_frame->Send(new InputMsg_ExecuteNoValueEditCommand(
      focused_frame->GetRoutingID(), name));
}

void BrowserPluginGuest::OnImeSetComposition(
    int browser_plugin_instance_id,
    const std::string& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  Send(new InputMsg_ImeSetComposition(routing_id(),
                                      base::UTF8ToUTF16(text), underlines,
                                      selection_start, selection_end));
}

void BrowserPluginGuest::OnImeConfirmComposition(
    int browser_plugin_instance_id,
    const std::string& text,
    bool keep_selection) {
  Send(new InputMsg_ImeConfirmComposition(routing_id(),
                                          base::UTF8ToUTF16(text),
                                          gfx::Range::InvalidRange(),
                                          keep_selection));
}

void BrowserPluginGuest::OnExtendSelectionAndDelete(
    int browser_plugin_instance_id,
    int before,
    int after) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      web_contents()->GetFocusedFrame());
  if (rfh)
    rfh->ExtendSelectionAndDelete(before, after);
}

void BrowserPluginGuest::OnReclaimCompositorResources(
    int browser_plugin_instance_id,
    const FrameHostMsg_ReclaimCompositorResources_Params& params) {
  RenderWidgetHostImpl::SendReclaimCompositorResources(params.route_id,
                                                       params.output_surface_id,
                                                       params.renderer_host_id,
                                                       params.ack);
}

void BrowserPluginGuest::OnLockMouse(bool user_gesture,
                                     bool last_unlocked_by_target,
                                     bool privileged) {
  if (pending_lock_request_) {
    // Immediately reject the lock because only one pointerLock may be active
    // at a time.
    Send(new ViewMsg_LockMouse_ACK(routing_id(), false));
    return;
  }

  pending_lock_request_ = true;

  delegate_->RequestPointerLockPermission(
      user_gesture,
      last_unlocked_by_target,
      base::Bind(&BrowserPluginGuest::PointerLockPermissionResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BrowserPluginGuest::OnLockMouseAck(int browser_plugin_instance_id,
                                        bool succeeded) {
  Send(new ViewMsg_LockMouse_ACK(routing_id(), succeeded));
  pending_lock_request_ = false;
  if (succeeded)
    mouse_locked_ = true;
}

void BrowserPluginGuest::OnSetFocus(int browser_plugin_instance_id,
                                    bool focused,
                                    blink::WebFocusType focus_type) {
  RenderWidgetHostView* rwhv = web_contents()->GetRenderWidgetHostView();
  RenderWidgetHost* rwh = rwhv ? rwhv->GetRenderWidgetHost() : nullptr;
  SetFocus(rwh, focused, focus_type);
}

void BrowserPluginGuest::OnSetEditCommandsForNextKeyEvent(
    int browser_plugin_instance_id,
    const std::vector<EditCommand>& edit_commands) {
  Send(new InputMsg_SetEditCommandsForNextKeyEvent(routing_id(),
                                                   edit_commands));
}

void BrowserPluginGuest::OnSetVisibility(int browser_plugin_instance_id,
                                         bool visible) {
  guest_visible_ = visible;
  if (embedder_visible_ && guest_visible_)
    GetWebContents()->WasShown();
  else
    GetWebContents()->WasHidden();
}

void BrowserPluginGuest::OnUnlockMouse() {
  SendMessageToEmbedder(
      new BrowserPluginMsg_SetMouseLock(browser_plugin_instance_id(), false));
}

void BrowserPluginGuest::OnUnlockMouseAck(int browser_plugin_instance_id) {
  // mouse_locked_ could be false here if the lock attempt was cancelled due
  // to window focus, or for various other reasons before the guest was informed
  // of the lock's success.
  if (mouse_locked_)
    Send(new ViewMsg_MouseLockLost(routing_id()));
  mouse_locked_ = false;
}

void BrowserPluginGuest::OnUpdateGeometry(int browser_plugin_instance_id,
                                          const gfx::Rect& view_rect) {
  // The plugin has moved within the embedder without resizing or the
  // embedder/container's view rect changing.
  guest_window_rect_ = view_rect;
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      GetWebContents()->GetRenderViewHost());
  if (rvh)
    rvh->SendScreenRects();
}

void BrowserPluginGuest::OnHasTouchEventHandlers(bool accept) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_ShouldAcceptTouchEvents(
          browser_plugin_instance_id(), accept));
}

#if defined(OS_MACOSX)
void BrowserPluginGuest::OnShowPopup(
    RenderFrameHost* render_frame_host,
    const FrameHostMsg_ShowPopup_Params& params) {
  gfx::Rect translated_bounds(params.bounds);
  translated_bounds.Offset(guest_window_rect_.OffsetFromOrigin());
  BrowserPluginPopupMenuHelper popup_menu_helper(
      owner_web_contents_->GetRenderViewHost(), render_frame_host);
  popup_menu_helper.ShowPopupMenu(translated_bounds,
                                  params.item_height,
                                  params.item_font_size,
                                  params.selected_item,
                                  params.popup_items,
                                  params.right_aligned,
                                  params.allow_multiple_selection);
}
#endif

void BrowserPluginGuest::OnShowWidget(int route_id,
                                      const gfx::Rect& initial_rect) {
  GetWebContents()->ShowCreatedWidget(route_id, initial_rect);
}

void BrowserPluginGuest::OnTakeFocus(bool reverse) {
  SendMessageToEmbedder(
      new BrowserPluginMsg_AdvanceFocus(browser_plugin_instance_id(), reverse));
}

void BrowserPluginGuest::OnTextInputTypeChanged(ui::TextInputType type,
                                                ui::TextInputMode input_mode,
                                                bool can_compose_inline,
                                                int flags) {
  // Save the state of text input so we can restore it on focus.
  last_text_input_type_ = type;
  last_input_mode_ = input_mode;
  last_input_flags_ = flags;
  last_can_compose_inline_ = can_compose_inline;

  SendTextInputTypeChangedToView(
      static_cast<RenderWidgetHostViewBase*>(
          web_contents()->GetRenderWidgetHostView()));
}

void BrowserPluginGuest::OnImeCancelComposition() {
  static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView())->ImeCancelComposition();
}

#if defined(OS_MACOSX) || defined(USE_AURA)
void BrowserPluginGuest::OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) {
  static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView())->ImeCompositionRangeChanged(
          range, character_bounds);
}
#endif

}  // namespace content
