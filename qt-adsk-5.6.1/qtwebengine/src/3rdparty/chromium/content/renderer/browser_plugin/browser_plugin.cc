// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "cc/surfaces/surface.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/sad_plugin.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebCanvas;
using blink::WebPluginContainer;
using blink::WebPoint;
using blink::WebRect;
using blink::WebURL;
using blink::WebVector;

namespace {
using PluginContainerMap =
    std::map<blink::WebPluginContainer*, content::BrowserPlugin*>;
static base::LazyInstance<PluginContainerMap> g_plugin_container_map =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace content {

// static
BrowserPlugin* BrowserPlugin::GetFromNode(blink::WebNode& node) {
  blink::WebPluginContainer* container = node.pluginContainer();
  if (!container)
    return nullptr;

  PluginContainerMap* browser_plugins = g_plugin_container_map.Pointer();
  PluginContainerMap::iterator it = browser_plugins->find(container);
  return it == browser_plugins->end() ? nullptr : it->second;
}

BrowserPlugin::BrowserPlugin(
    RenderFrame* render_frame,
    const base::WeakPtr<BrowserPluginDelegate>& delegate)
    : attached_(false),
      render_frame_routing_id_(render_frame->GetRoutingID()),
      container_(nullptr),
      sad_guest_(nullptr),
      guest_crashed_(false),
      plugin_focused_(false),
      visible_(true),
      mouse_locked_(false),
      ready_(false),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      contents_opaque_(true),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  browser_plugin_instance_id_ =
      BrowserPluginManager::Get()->GetNextInstanceID();

  if (delegate_)
    delegate_->SetElementInstanceID(browser_plugin_instance_id_);
}

BrowserPlugin::~BrowserPlugin() {
  if (compositing_helper_.get())
    compositing_helper_->OnContainerDestroy();

  if (delegate_) {
    delegate_->DidDestroyElement();
    delegate_.reset();
  }

  BrowserPluginManager::Get()->RemoveBrowserPlugin(browser_plugin_instance_id_);
}

bool BrowserPlugin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPlugin, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER_GENERIC(BrowserPluginMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(message))
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestGone, OnGuestGone)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetContentsOpaque, OnSetContentsOpaque)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetMouseLock, OnSetMouseLock)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetTooltipText, OnSetTooltipText)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_ShouldAcceptTouchEvents,
                        OnShouldAcceptTouchEvents)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetChildFrameSurface,
                        OnSetChildFrameSurface)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPlugin::OnSetChildFrameSurface(
    int browser_plugin_instance_id,
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float scale_factor,
    const cc::SurfaceSequence& sequence) {
  if (!attached())
    return;

  EnableCompositing(true);
  DCHECK(compositing_helper_.get());

  compositing_helper_->OnSetSurface(surface_id, frame_size, scale_factor,
                                    sequence);
}

void BrowserPlugin::SendSatisfySequence(const cc::SurfaceSequence& sequence) {
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SatisfySequence(
      render_frame_routing_id_, browser_plugin_instance_id_, sequence));
}

void BrowserPlugin::UpdateDOMAttribute(const std::string& attribute_name,
                                       const base::string16& attribute_value) {
  if (!container())
    return;

  blink::WebElement element = container()->element();
  blink::WebString web_attribute_name =
      blink::WebString::fromUTF8(attribute_name);
  element.setAttribute(web_attribute_name, attribute_value);
}

void BrowserPlugin::Attach() {
  Detach();

  BrowserPluginHostMsg_Attach_Params attach_params;
  attach_params.focused = ShouldGuestBeFocused();
  attach_params.visible = visible_;
  attach_params.view_rect = view_rect();
  attach_params.is_full_page_plugin = false;
  if (container()) {
    blink::WebLocalFrame* frame = container()->element().document().frame();
    attach_params.is_full_page_plugin =
        frame->view()->mainFrame()->document().isPluginDocument();
  }
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_Attach(
      render_frame_routing_id_,
      browser_plugin_instance_id_,
      attach_params));

  attached_ = true;
}

void BrowserPlugin::Detach() {
  if (!attached())
    return;

  attached_ = false;
  guest_crashed_ = false;
  EnableCompositing(false);
  if (compositing_helper_.get()) {
    compositing_helper_->OnContainerDestroy();
    compositing_helper_ = nullptr;
  }

  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_Detach(browser_plugin_instance_id_));
}

void BrowserPlugin::DidCommitCompositorFrame() {
  if (compositing_helper_.get())
    compositing_helper_->DidCommitCompositorFrame();
}

void BrowserPlugin::OnAdvanceFocus(int browser_plugin_instance_id,
                                   bool reverse) {
  auto render_frame = RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto render_view = render_frame ? render_frame->GetRenderView() : nullptr;
  if (!render_view)
    return;
  render_view->GetWebView()->advanceFocus(reverse);
}

void BrowserPlugin::OnCompositorFrameSwapped(const IPC::Message& message) {
  if (!attached())
    return;

  BrowserPluginMsg_CompositorFrameSwapped::Param param;
  if (!BrowserPluginMsg_CompositorFrameSwapped::Read(&message, &param))
    return;
  // Note that there is no need to send ACK for this message.
  // If the guest has updated pixels then it is no longer crashed.
  guest_crashed_ = false;

  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  base::get<1>(param).frame.AssignTo(frame.get());

  EnableCompositing(true);
  compositing_helper_->OnCompositorFrameSwapped(
      frame.Pass(),
      base::get<1>(param).producing_route_id,
      base::get<1>(param).output_surface_id,
      base::get<1>(param).producing_host_id,
      base::get<1>(param).shared_memory_handle);
}

void BrowserPlugin::OnGuestGone(int browser_plugin_instance_id) {
  guest_crashed_ = true;

  // Turn off compositing so we can display the sad graphic. Changes to
  // compositing state will show up at a later time after a layout and commit.
  EnableCompositing(false);

  // Queue up showing the sad graphic to give content embedders an opportunity
  // to fire their listeners and potentially overlay the webview with custom
  // behavior. If the BrowserPlugin is destroyed in the meantime, then the
  // task will not be executed.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BrowserPlugin::ShowSadGraphic,
                            weak_ptr_factory_.GetWeakPtr()));
}

void BrowserPlugin::OnSetContentsOpaque(int browser_plugin_instance_id,
                                        bool opaque) {
  if (contents_opaque_ == opaque)
    return;
  contents_opaque_ = opaque;
  if (compositing_helper_.get())
    compositing_helper_->SetContentsOpaque(opaque);
}

void BrowserPlugin::OnSetCursor(int browser_plugin_instance_id,
                                const WebCursor& cursor) {
  cursor_ = cursor;
}

void BrowserPlugin::OnSetMouseLock(int browser_plugin_instance_id,
                                   bool enable) {
  auto render_frame = RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto render_view = static_cast<RenderViewImpl*>(
      render_frame ? render_frame->GetRenderView() : nullptr);
  if (enable) {
    if (mouse_locked_ || !render_view)
      return;
    render_view->mouse_lock_dispatcher()->LockMouse(this);
  } else {
    if (!mouse_locked_) {
      OnLockMouseACK(false);
      return;
    }
    if (!render_view)
      return;
    render_view->mouse_lock_dispatcher()->UnlockMouse(this);
  }
}

void BrowserPlugin::OnSetTooltipText(int instance_id,
                                     const base::string16& tooltip_text) {
  // Show tooltip text by setting the BrowserPlugin's |title| attribute.
  UpdateDOMAttribute("title", tooltip_text);
}

void BrowserPlugin::OnShouldAcceptTouchEvents(int browser_plugin_instance_id,
                                              bool accept) {
  if (container()) {
    container()->requestTouchEventType(
        accept ? WebPluginContainer::TouchEventRequestTypeRaw
               : WebPluginContainer::TouchEventRequestTypeNone);
  }
}

void BrowserPlugin::ShowSadGraphic() {
  // If the BrowserPlugin is scheduled to be deleted, then container_ will be
  // nullptr so we shouldn't attempt to access it.
  if (container_)
    container_->invalidate();
}

void BrowserPlugin::UpdateInternalInstanceId() {
  // This is a way to notify observers of our attributes that this plugin is
  // available in render tree.
  // TODO(lazyboy): This should be done through the delegate instead. Perhaps
  // by firing an event from there.
  UpdateDOMAttribute(
      "internalinstanceid",
      base::UTF8ToUTF16(base::IntToString(browser_plugin_instance_id_)));
}

void BrowserPlugin::UpdateGuestFocusState(blink::WebFocusType focus_type) {
  if (!attached())
    return;
  bool should_be_focused = ShouldGuestBeFocused();
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetFocus(
      browser_plugin_instance_id_,
      should_be_focused,
      focus_type));
}

bool BrowserPlugin::ShouldGuestBeFocused() const {
  bool embedder_focused = false;
  auto render_frame = RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto render_view = static_cast<RenderViewImpl*>(
      render_frame ? render_frame->GetRenderView() : nullptr);
  if (render_view)
    embedder_focused = render_view->has_focus();
  return plugin_focused_ && embedder_focused;
}

WebPluginContainer* BrowserPlugin::container() const {
  return container_;
}

bool BrowserPlugin::initialize(WebPluginContainer* container) {
  if (!container)
    return false;

  container_ = container;
  container_->setWantsWheelEvents(true);

  g_plugin_container_map.Get().insert(std::make_pair(container_, this));

  BrowserPluginManager::Get()->AddBrowserPlugin(
      browser_plugin_instance_id_, this);

  // Defer attach call so that if there's any pending browser plugin
  // destruction, then it can progress first.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BrowserPlugin::UpdateInternalInstanceId,
                            weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void BrowserPlugin::EnableCompositing(bool enable) {
  bool enabled = !!compositing_helper_.get();
  if (enabled == enable)
    return;

  if (enable) {
    DCHECK(!compositing_helper_.get());
    if (!compositing_helper_.get()) {
      compositing_helper_ = ChildFrameCompositingHelper::CreateForBrowserPlugin(
          weak_ptr_factory_.GetWeakPtr());
    }
  }
  compositing_helper_->EnableCompositing(enable);
  compositing_helper_->SetContentsOpaque(contents_opaque_);

  if (!enable) {
    DCHECK(compositing_helper_.get());
    compositing_helper_->OnContainerDestroy();
    compositing_helper_ = nullptr;
  }
}

void BrowserPlugin::destroy() {
  if (container_) {
    // The BrowserPlugin's WebPluginContainer is deleted immediately after this
    // call returns, so let's not keep a reference to it around.
    g_plugin_container_map.Get().erase(container_);
  }

  container_ = nullptr;
  // Will be a no-op if the mouse is not currently locked.
  auto render_frame = RenderFrameImpl::FromRoutingID(render_frame_routing_id());
  auto render_view = static_cast<RenderViewImpl*>(
      render_frame ? render_frame->GetRenderView() : nullptr);
  if (render_view)
    render_view->mouse_lock_dispatcher()->OnLockTargetDestroyed(this);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

v8::Local<v8::Object> BrowserPlugin::v8ScriptableObject(v8::Isolate* isolate) {
  if (!delegate_)
    return v8::Local<v8::Object>();

  return delegate_->V8ScriptableObject(isolate);
}

bool BrowserPlugin::supportsKeyboardFocus() const {
  return true;
}

bool BrowserPlugin::supportsEditCommands() const {
  return true;
}

bool BrowserPlugin::supportsInputMethod() const {
  return true;
}

bool BrowserPlugin::canProcessDrag() const {
  return true;
}

void BrowserPlugin::paint(WebCanvas* canvas, const WebRect& rect) {
  if (guest_crashed_) {
    if (!sad_guest_)  // Lazily initialize bitmap.
      sad_guest_ = GetContentClient()->renderer()->GetSadWebViewBitmap();
    // content_shell does not have the sad plugin bitmap, so we'll paint black
    // instead to make it clear that something went wrong.
    if (sad_guest_) {
      PaintSadPlugin(canvas, view_rect_, *sad_guest_);
      return;
    }
  }
  SkAutoCanvasRestore auto_restore(canvas, true);
  canvas->translate(view_rect_.x(), view_rect_.y());
  SkRect image_data_rect = SkRect::MakeXYWH(
      SkIntToScalar(0),
      SkIntToScalar(0),
      SkIntToScalar(view_rect_.width()),
      SkIntToScalar(view_rect_.height()));
  canvas->clipRect(image_data_rect);
  // Paint black or white in case we have nothing in our backing store or we
  // need to show a gutter.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(guest_crashed_ ? SK_ColorBLACK : SK_ColorWHITE);
  canvas->drawRect(image_data_rect, paint);
}

// static
bool BrowserPlugin::ShouldForwardToBrowserPlugin(
    const IPC::Message& message) {
  return IPC_MESSAGE_CLASS(message) == BrowserPluginMsgStart;
}

void BrowserPlugin::updateGeometry(const WebRect& window_rect,
                                   const WebRect& clip_rect,
                                   const WebRect& unobscured_rect,
                                   const WebVector<WebRect>& cut_outs_rects,
                                   bool is_visible) {
  gfx::Rect old_view_rect = view_rect_;
  view_rect_ = window_rect;

  if (!ready_) {
    if (delegate_)
      delegate_->Ready();
    ready_ = true;
  }

  if (delegate_ && (view_rect_.size() != old_view_rect.size()))
    delegate_->DidResizeElement(view_rect_.size());

  if (!attached())
    return;

  if (old_view_rect.size() == view_rect_.size()) {
    // Let the browser know about the updated view rect.
    BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UpdateGeometry(
        browser_plugin_instance_id_, view_rect_));
    return;
  }
}

void BrowserPlugin::updateFocus(bool focused, blink::WebFocusType focus_type) {
  plugin_focused_ = focused;
  UpdateGuestFocusState(focus_type);
}

void BrowserPlugin::updateVisibility(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (!attached())
    return;

  if (compositing_helper_.get())
    compositing_helper_->UpdateVisibility(visible);

  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_SetVisibility(
      browser_plugin_instance_id_,
      visible));
}

bool BrowserPlugin::acceptsInputEvents() {
  return true;
}

bool BrowserPlugin::handleInputEvent(const blink::WebInputEvent& event,
                                     blink::WebCursorInfo& cursor_info) {
  if (guest_crashed_ || !attached())
    return false;

  if (event.type == blink::WebInputEvent::ContextMenu)
    return true;

  if (blink::WebInputEvent::isKeyboardEventType(event.type) &&
      !edit_commands_.empty()) {
    BrowserPluginManager::Get()->Send(
        new BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent(
            browser_plugin_instance_id_,
            edit_commands_));
    edit_commands_.clear();
  }

  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(browser_plugin_instance_id_,
                                                view_rect_,
                                                &event));
  GetWebKitCursorInfo(cursor_, &cursor_info);
  return true;
}

bool BrowserPlugin::handleDragStatusUpdate(blink::WebDragStatus drag_status,
                                           const blink::WebDragData& drag_data,
                                           blink::WebDragOperationsMask mask,
                                           const blink::WebPoint& position,
                                           const blink::WebPoint& screen) {
  if (guest_crashed_ || !attached())
    return false;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_DragStatusUpdate(
        browser_plugin_instance_id_,
        drag_status,
        DropDataBuilder::Build(drag_data),
        mask,
        position));
  return true;
}

void BrowserPlugin::didReceiveResponse(
    const blink::WebURLResponse& response) {
}

void BrowserPlugin::didReceiveData(const char* data, int data_length) {
  if (delegate_)
    delegate_->DidReceiveData(data, data_length);
}

void BrowserPlugin::didFinishLoading() {
  if (delegate_)
    delegate_->DidFinishLoading();
}

void BrowserPlugin::didFailLoading(const blink::WebURLError& error) {
}

void BrowserPlugin::didFinishLoadingFrameRequest(const blink::WebURL& url,
                                                 void* notify_data) {
}

void BrowserPlugin::didFailLoadingFrameRequest(
    const blink::WebURL& url,
    void* notify_data,
    const blink::WebURLError& error) {
}

bool BrowserPlugin::executeEditCommand(const blink::WebString& name) {
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ExecuteEditCommand(
      browser_plugin_instance_id_,
      name.utf8()));

  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::executeEditCommand(const blink::WebString& name,
                                       const blink::WebString& value) {
  edit_commands_.push_back(EditCommand(name.utf8(), value.utf8()));
  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::setComposition(
    const blink::WebString& text,
    const blink::WebVector<blink::WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd) {
  if (!attached())
    return false;
  std::vector<blink::WebCompositionUnderline> std_underlines;
  for (size_t i = 0; i < underlines.size(); ++i) {
    std_underlines.push_back(underlines[i]);
  }
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_ImeSetComposition(
      browser_plugin_instance_id_,
      text.utf8(),
      std_underlines,
      selectionStart,
      selectionEnd));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

bool BrowserPlugin::confirmComposition(
    const blink::WebString& text,
    blink::WebWidget::ConfirmCompositionBehavior selectionBehavior) {
  if (!attached())
    return false;
  bool keep_selection = (selectionBehavior == blink::WebWidget::KeepSelection);
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_ImeConfirmComposition(
          browser_plugin_instance_id_,
          text.utf8(),
          keep_selection));
  // TODO(kochi): This assumes the IPC handling always succeeds.
  return true;
}

void BrowserPlugin::extendSelectionAndDelete(int before, int after) {
  if (!attached())
    return;
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_ExtendSelectionAndDelete(
          browser_plugin_instance_id_,
          before,
          after));
}

void BrowserPlugin::OnLockMouseACK(bool succeeded) {
  mouse_locked_ = succeeded;
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_LockMouse_ACK(
      browser_plugin_instance_id_,
      succeeded));
}

void BrowserPlugin::OnMouseLockLost() {
  mouse_locked_ = false;
  BrowserPluginManager::Get()->Send(new BrowserPluginHostMsg_UnlockMouse_ACK(
      browser_plugin_instance_id_));
}

bool BrowserPlugin::HandleMouseLockedInputEvent(
    const blink::WebMouseEvent& event) {
  BrowserPluginManager::Get()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(browser_plugin_instance_id_,
                                                view_rect_,
                                                &event));
  return true;
}

}  // namespace content
