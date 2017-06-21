// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include <vector>

#include "base/command_line.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"
#include "ui/base/win/atl_module.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerWin(initial_tree, delegate, factory);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      tracked_scroll_object_(NULL),
      focus_event_on_root_needed_(false) {
  ui::win::CreateATLModuleIfNeeded();
  Initialize(initial_tree);
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  if (tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerWin::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state =
      (1 << ui::AX_STATE_ENABLED) |
      (1 << ui::AX_STATE_READ_ONLY) |
      (1 << ui::AX_STATE_BUSY);

  ui::AXTreeUpdate update;
  update.nodes.push_back(empty_document);
  return update;
}

HWND BrowserAccessibilityManagerWin::GetParentHWND() {
  if (!delegate_)
    return NULL;
  return delegate_->AccessibilityGetAcceleratedWidget();
}

IAccessible* BrowserAccessibilityManagerWin::GetParentIAccessible() {
  if (!delegate_)
    return NULL;
  return delegate_->AccessibilityGetNativeViewAccessible();
}

void BrowserAccessibilityManagerWin::MaybeCallNotifyWinEvent(
    DWORD event, BrowserAccessibility* node) {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (!delegate) {
    // This line and other LOG(WARNING) lines are temporary, to debug
    // flaky failures in DumpAccessibilityEvent* tests.
    // http://crbug.com/440579
    LOG(WARNING) << "Not firing AX event because of no delegate";
    return;
  }

  if (!node->IsNative())
    return;

  HWND hwnd = delegate->AccessibilityGetAcceleratedWidget();
  if (!hwnd) {
    LOG(WARNING) << "Not firing AX event because of no hwnd";
    return;
  }

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to Windows.
  if (node->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return;

  // It doesn't make sense to fire a REORDER event on a leaf node; that
  // happens when the node has internal children line inline text boxes.
  if (event == EVENT_OBJECT_REORDER && node->PlatformIsLeaf())
    return;

  // Don't fire focus, or load complete notifications if the
  // window isn't focused, because that can confuse screen readers into
  // entering their "browse" mode.
  if ((event == EVENT_OBJECT_FOCUS ||
       event == IA2_EVENT_DOCUMENT_LOAD_COMPLETE) &&
      (!delegate_->AccessibilityViewHasFocus())) {
    return;
  }

  // NVDA gets confused if we focus the main document element when it hasn't
  // finished loading and it has no children at all, so suppress that event.
  if (event == EVENT_OBJECT_FOCUS &&
      node == GetRoot() &&
      node->PlatformChildCount() == 0 &&
      !node->HasState(ui::AX_STATE_BUSY) &&
      !node->GetBoolAttribute(ui::AX_ATTR_DOC_LOADED)) {
    return;
  }

  // If a focus event is needed on the root, fire that first before
  // this event.
  if (event == EVENT_OBJECT_FOCUS && node == GetRoot())
    focus_event_on_root_needed_ = false;
  else if (focus_event_on_root_needed_)
    OnWindowFocused();

  LONG child_id = node->ToBrowserAccessibilityWin()->unique_id_win();
  ::NotifyWinEvent(event, hwnd, OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::OnWindowFocused() {
  // This is called either when this web frame gets focused, or when
  // the root of the accessibility tree changes. In both cases, we need
  // to fire a focus event on the root and then on the focused element
  // within the page, if different.

  // Set this flag so that we'll keep trying to fire these focus events
  // if they're not successful this time.
  focus_event_on_root_needed_ = true;

  if (!delegate_ || !delegate_->AccessibilityViewHasFocus())
    return;

  // Try to fire a focus event on the root first and then the focused node.
  // This will clear focus_event_on_root_needed_ if successful.
  if (focus_ != tree_->root())
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, GetRoot());
  BrowserAccessibilityManager::OnWindowFocused();
}

void BrowserAccessibilityManagerWin::UserIsReloading() {
  MaybeCallNotifyWinEvent(IA2_EVENT_DOCUMENT_RELOAD, GetRoot());
}

void BrowserAccessibilityManagerWin::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  BrowserAccessibilityDelegate* root_delegate = GetDelegateFromRootManager();
  if (!root_delegate || !root_delegate->AccessibilityGetAcceleratedWidget()) {
    LOG(WARNING) << "Not firing AX event because of no root_delegate or hwnd";
    return;
  }

  // Don't fire events when this document might be stale as the user has
  // started navigating to a new document.
  if (user_is_navigating_away_)
    return;

  // Inline text boxes are an internal implementation detail, we don't
  // expose them to Windows.
  if (node->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return;

  // Don't fire focus, blur, or load complete notifications if the
  // window isn't focused, because that can confuse screen readers into
  // entering their "browse" mode.
  if ((event_type == ui::AX_EVENT_FOCUS ||
       event_type == ui::AX_EVENT_BLUR ||
       event_type == ui::AX_EVENT_LOAD_COMPLETE) &&
      !root_delegate->AccessibilityViewHasFocus()) {
    return;
  }

  // NVDA gets confused if we focus the main document element when it hasn't
  // finished loading and it has no children at all, so suppress that event.
  if (event_type == ui::AX_EVENT_FOCUS &&
      node == GetRoot() &&
      node->PlatformChildCount() == 0 &&
      !node->HasState(ui::AX_STATE_BUSY) &&
      !node->GetBoolAttribute(ui::AX_ATTR_DOC_LOADED)) {
    return;
  }

  // If a focus event is needed on the root, fire that first before
  // this event.
  if (event_type == ui::AX_EVENT_FOCUS && node == GetRoot())
    focus_event_on_root_needed_ = false;
  else if (focus_event_on_root_needed_)
    OnWindowFocused();

  LONG event_id = EVENT_MIN;
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      event_id = IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
      break;
    case ui::AX_EVENT_ALERT:
      event_id = EVENT_SYSTEM_ALERT;
      break;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case ui::AX_EVENT_BLUR:
      // Equivalent to focus on the root.
      event_id = EVENT_OBJECT_FOCUS;
      node = GetRoot();
      break;
    case ui::AX_EVENT_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ui::AX_EVENT_FOCUS:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      if (node->GetBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_BUSY))
        return;
      event_id = EVENT_OBJECT_LIVEREGIONCHANGED;
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      event_id = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      event_id = EVENT_SYSTEM_SCROLLINGEND;
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      event_id = EVENT_SYSTEM_SCROLLINGSTART;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_SELECTIONWITHIN;
      break;
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
      event_id = IA2_EVENT_TEXT_CARET_MOVED;
      break;
    default:
      // Not all WebKit accessibility events result in a Windows
      // accessibility notification.
      break;
  }

  if (event_id != EVENT_MIN) {
    // Pass the node's unique id in the |child_id| argument to NotifyWinEvent;
    // the AT client will then call get_accChild on the HWND's accessibility
    // object and pass it that same id, which we can use to retrieve the
    // IAccessible for this node.
    MaybeCallNotifyWinEvent(event_id, node);
  }

  // If this is a layout complete notification (sent when a container scrolls)
  // and there is a descendant tracked object, send a notification on it.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  if (event_type == ui::AX_EVENT_LAYOUT_COMPLETE &&
      tracked_scroll_object_ &&
      tracked_scroll_object_->IsDescendantOf(node)) {
    MaybeCallNotifyWinEvent(
        IA2_EVENT_VISIBLE_DATA_CHANGED, tracked_scroll_object_);
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::OnNodeCreated(ui::AXTree* tree,
                                                   ui::AXNode* node) {
  BrowserAccessibilityManager::OnNodeCreated(tree, node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (!obj)
    return;
  if (!obj->IsNative())
    return;
  LONG unique_id_win = obj->ToBrowserAccessibilityWin()->unique_id_win();
  unique_id_to_ax_id_map_[unique_id_win] = obj->GetId();
}

void BrowserAccessibilityManagerWin::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                         ui::AXNode* node) {
  BrowserAccessibilityManager::OnNodeWillBeDeleted(tree, node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (!obj)
    return;
  if (!obj->IsNative())
    return;
  unique_id_to_ax_id_map_.erase(
      obj->ToBrowserAccessibilityWin()->unique_id_win());
  if (obj == tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(
      tree, root_changed, changes);

  if (root_changed) {
    // In order to make screen readers aware of the new accessibility root,
    // we need to fire a focus event on it.
    OnWindowFocused();
  }

  // Do a sequence of Windows-specific updates on each node. Each one is
  // done in a single pass that must complete before the next step starts.
  // The first step moves win_attributes_ to old_win_attributes_ and then
  // recomputes all of win_attributes_ other than IAccessibleText.
  for (size_t i = 0; i < changes.size(); ++i) {
    BrowserAccessibility* obj = GetFromAXNode(changes[i].node);
    if (obj && obj->IsNative() && !obj->PlatformIsChildOfLeaf())
      obj->ToBrowserAccessibilityWin()->UpdateStep1ComputeWinAttributes();
  }

  // The next step updates the hypertext of each node, which is a
  // concatenation of all of its child text nodes, so it can't run until
  // the text of all of the nodes was computed in the previous step.
  for (size_t i = 0; i < changes.size(); ++i) {
    BrowserAccessibility* obj = GetFromAXNode(changes[i].node);
    if (obj && obj->IsNative() && !obj->PlatformIsChildOfLeaf())
      obj->ToBrowserAccessibilityWin()->UpdateStep2ComputeHypertext();
  }

  // The third step fires events on nodes based on what's changed - like
  // if the name, value, or description changed, or if the hypertext had
  // text inserted or removed. It's able to figure out exactly what changed
  // because we still have old_win_attributes_ populated.
  // This step has to run after the previous two steps complete because the
  // client may walk the tree when it receives any of these events.
  // At the end, it deletes old_win_attributes_ since they're not needed
  // anymore.
  for (size_t i = 0; i < changes.size(); ++i) {
    BrowserAccessibility* obj = GetFromAXNode(changes[i].node);
    if (obj && obj->IsNative() && !obj->PlatformIsChildOfLeaf()) {
      obj->ToBrowserAccessibilityWin()->UpdateStep3FireEvents(
          changes[i].type == AXTreeDelegate::SUBTREE_CREATED);
    }
  }
}

void BrowserAccessibilityManagerWin::TrackScrollingObject(
    BrowserAccessibilityWin* node) {
  if (tracked_scroll_object_)
    tracked_scroll_object_->Release();
  tracked_scroll_object_ = node;
  tracked_scroll_object_->AddRef();
}

BrowserAccessibilityWin* BrowserAccessibilityManagerWin::GetFromUniqueIdWin(
    LONG unique_id_win) {
  base::hash_map<LONG, int32>::iterator iter =
      unique_id_to_ax_id_map_.find(unique_id_win);
  if (iter != unique_id_to_ax_id_map_.end()) {
    BrowserAccessibility* result = GetFromID(iter->second);
    if (result && result->IsNative())
      return result->ToBrowserAccessibilityWin();
  }

  // Also search all child frames, such as out-of-process iframes or
  // guest browser plugins.
  if (delegate()) {
    std::vector<BrowserAccessibilityManager*> child_frames;
    delegate()->AccessibilityGetAllChildFrames(&child_frames);
    for (size_t i = 0; i < child_frames.size(); ++i) {
      BrowserAccessibilityManagerWin* child_manager =
          child_frames[i]->ToBrowserAccessibilityManagerWin();
      BrowserAccessibilityWin* result =
          child_manager->GetFromUniqueIdWin(unique_id_win);
      if (result)
        return result;
    }
  }

  return NULL;
}

}  // namespace content
