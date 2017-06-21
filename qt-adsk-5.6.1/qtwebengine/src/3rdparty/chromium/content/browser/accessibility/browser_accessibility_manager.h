// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/native_widget_types.h"

struct AccessibilityHostMsg_EventParams;
struct AccessibilityHostMsg_LocationChangeParams;

namespace content {
class BrowserAccessibility;
class BrowserAccessibilityManager;
#if defined(OS_ANDROID)
class BrowserAccessibilityManagerAndroid;
#endif
#if defined(OS_WIN)
class BrowserAccessibilityManagerWin;
#endif

// For testing.
CONTENT_EXPORT ui::AXTreeUpdate MakeAXTreeUpdate(
    const ui::AXNodeData& node,
    const ui::AXNodeData& node2 = ui::AXNodeData(),
    const ui::AXNodeData& node3 = ui::AXNodeData(),
    const ui::AXNodeData& node4 = ui::AXNodeData(),
    const ui::AXNodeData& node5 = ui::AXNodeData(),
    const ui::AXNodeData& node6 = ui::AXNodeData(),
    const ui::AXNodeData& node7 = ui::AXNodeData(),
    const ui::AXNodeData& node8 = ui::AXNodeData(),
    const ui::AXNodeData& node9 = ui::AXNodeData());

// Class that can perform actions on behalf of the BrowserAccessibilityManager.
// Note: BrowserAccessibilityManager should never cache any of the return
// values from any of these interfaces, especially those that return pointers.
// They may only be valid within this call stack. That policy eliminates any
// concerns about ownership and lifecycle issues; none of these interfaces
// transfer ownership and no return values are guaranteed to be valid outside
// of the current call stack.
class CONTENT_EXPORT BrowserAccessibilityDelegate {
 public:
  virtual ~BrowserAccessibilityDelegate() {}
  virtual void AccessibilitySetFocus(int acc_obj_id) = 0;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) = 0;
  virtual void AccessibilityShowContextMenu(int acc_obj_id) = 0;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, const gfx::Rect& subfocus) = 0;
  virtual void AccessibilityScrollToPoint(
      int acc_obj_id, const gfx::Point& point) = 0;
  virtual void AccessibilitySetScrollOffset(
      int acc_obj_id, const gfx::Point& offset) = 0;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) = 0;
  virtual void AccessibilitySetValue(
      int acc_obj_id, const base::string16& value) = 0;
  virtual bool AccessibilityViewHasFocus() const = 0;
  virtual gfx::Rect AccessibilityGetViewBounds() const = 0;
  virtual gfx::Point AccessibilityOriginInScreen(
      const gfx::Rect& bounds) const = 0;
  virtual void AccessibilityHitTest(
      const gfx::Point& point) = 0;
  virtual void AccessibilitySetAccessibilityFocus(int acc_obj_id) = 0;
  virtual void AccessibilityFatalError() = 0;
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() = 0;
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() = 0;
  virtual BrowserAccessibilityManager* AccessibilityGetChildFrame(
      int accessibility_node_id) = 0;
  virtual void AccessibilityGetAllChildFrames(
      std::vector<BrowserAccessibilityManager*>* child_frames) = 0;
  virtual BrowserAccessibility* AccessibilityGetParentFrame() = 0;
};

class CONTENT_EXPORT BrowserAccessibilityFactory {
 public:
  virtual ~BrowserAccessibilityFactory() {}

  // Create an instance of BrowserAccessibility and return a new
  // reference to it.
  virtual BrowserAccessibility* Create();
};

// This is all of the information about the current find in page result,
// so we can activate it if requested.
struct BrowserAccessibilityFindInPageInfo {
  BrowserAccessibilityFindInPageInfo();

  // This data about find in text results is updated as the user types.
  int request_id;
  int match_index;
  int start_id;
  int start_offset;
  int end_id;
  int end_offset;

  // The active request id indicates that the user committed to a find query,
  // e.g. by pressing enter or pressing the next or previous buttons. If
  // |active_request_id| == |request_id|, we fire an accessibility event
  // to move screen reader focus to that event.
  int active_request_id;
};

// Manages a tree of BrowserAccessibility objects.
class CONTENT_EXPORT BrowserAccessibilityManager : public ui::AXTreeDelegate {
 public:
  // Creates the platform-specific BrowserAccessibilityManager, but
  // with no parent window pointer. Only useful for unit tests.
  static BrowserAccessibilityManager* Create(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  ~BrowserAccessibilityManager() override;

  void Initialize(const ui::AXTreeUpdate& initial_tree);

  static ui::AXTreeUpdate GetEmptyDocument();

  virtual void NotifyAccessibilityEvent(
      ui::AXEvent event_type, BrowserAccessibility* node) { }

  // Return a pointer to the root of the tree, does not make a new reference.
  BrowserAccessibility* GetRoot();

  // Returns a pointer to the BrowserAccessibility object for a given AXNode.
  BrowserAccessibility* GetFromAXNode(ui::AXNode* node);

  // Return a pointer to the object corresponding to the given id,
  // does not make a new reference.
  BrowserAccessibility* GetFromID(int32 id);

  // Called to notify the accessibility manager that its associated native
  // view got focused.
  virtual void OnWindowFocused();

  // Called to notify the accessibility manager that its associated native
  // view lost focus.
  virtual void OnWindowBlurred();

  // Notify the accessibility manager about page navigation.
  void UserIsNavigatingAway();
  virtual void UserIsReloading();
  void NavigationSucceeded();
  void NavigationFailed();

  // Called to notify the accessibility manager that a mouse down event
  // occurred in the tab.
  void GotMouseDown();

  // Update the focused node to |node|, which may be null.
  // If |notify| is true, send a message to the renderer to set focus
  // to this node.
  void SetFocus(ui::AXNode* node, bool notify);
  void SetFocus(BrowserAccessibility* node, bool notify);

  // Tell the renderer to do the default action for this node.
  void DoDefaultAction(const BrowserAccessibility& node);

  // Tell the renderer to scroll to make |node| visible.
  // In addition, if it's not possible to make the entire object visible,
  // scroll so that the |subfocus| rect is visible at least. The subfocus
  // rect is in local coordinates of the object itself.
  void ScrollToMakeVisible(
      const BrowserAccessibility& node, gfx::Rect subfocus);

  // Tell the renderer to scroll such that |node| is at |point|,
  // where |point| is in global coordinates of the WebContents.
  void ScrollToPoint(
      const BrowserAccessibility& node, gfx::Point point);

  // If |node| is itself a scrollable container, set its scroll
  // offset to |offset|.
  void SetScrollOffset(const BrowserAccessibility& node, gfx::Point offset);

  // Tell the renderer to set the value of an editable text node.
  void SetValue(
      const BrowserAccessibility& node, const base::string16& value);

  // Tell the renderer to set the text selection on a node.
  void SetTextSelection(
      const BrowserAccessibility& node, int start_offset, int end_offset);

  // Retrieve the bounds of the parent View in screen coordinates.
  gfx::Rect GetViewBounds();

  // Fire an event telling native assistive technology to move focus to the
  // given find in page result.
  void ActivateFindInPageResult(int request_id, int match_index);

  // Called when the renderer process has notified us of about tree changes.
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params);

  // Called when the renderer process updates the location of accessibility
  // objects.
  void OnLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);

  // Called when a new find in page result is received. We hold on to this
  // information and don't activate it until the user requests it.
  void OnFindInPageResult(
      int request_id, int match_index, int start_id, int start_offset,
      int end_id, int end_offset);

  // This is called when the user has committed to a find in page query,
  // e.g. by pressing enter or tapping on the next / previous result buttons.
  // If a match has already been received for this request id,
  // activate the result now by firing an accessibility event. If a match
  // has not been received, we hold onto this request id and update it
  // when OnFindInPageResult is called.
  void ActivateFindInPageResult(int request_id);

#if defined(OS_WIN)
  BrowserAccessibilityManagerWin* ToBrowserAccessibilityManagerWin();
#endif

#if defined(OS_ANDROID)
  BrowserAccessibilityManagerAndroid* ToBrowserAccessibilityManagerAndroid();
#endif

  // Return the object that has focus, if it's a descandant of the
  // given root (inclusive). Does not make a new reference.
  virtual BrowserAccessibility* GetFocus(BrowserAccessibility* root);

  // Return the descentant of the given root that has focus, or that object's
  // active descendant if it has one.
  BrowserAccessibility* GetActiveDescendantFocus(BrowserAccessibility* root);

  // True by default, but some platforms want to treat the root
  // scroll offsets separately.
  virtual bool UseRootScrollOffsetsWhenComputingBounds();

  // Walk the tree.
  BrowserAccessibility* NextInTreeOrder(BrowserAccessibility* node);
  BrowserAccessibility* PreviousInTreeOrder(BrowserAccessibility* node);

  // AXTreeDelegate implementation.
  void OnNodeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnSubtreeWillBeDeleted(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeCreated(ui::AXTree* tree, ui::AXNode* node) override;
  void OnNodeChanged(ui::AXTree* tree, ui::AXNode* node) override;
  void OnAtomicUpdateFinished(
      ui::AXTree* tree,
      bool root_changed,
      const std::vector<ui::AXTreeDelegate::Change>& changes) override;

  BrowserAccessibilityDelegate* delegate() const { return delegate_; }
  void set_delegate(BrowserAccessibilityDelegate* delegate) {
    delegate_ = delegate;
  }

  // If this BrowserAccessibilityManager is a child frame or guest frame,
  // return the BrowserAccessibilityDelegate from the highest ancestor frame
  // in the frame tree.
  BrowserAccessibilityDelegate* GetDelegateFromRootManager();

  // Get a snapshot of the current tree as an AXTreeUpdate.
  ui::AXTreeUpdate SnapshotAXTreeForTesting();

 protected:
  BrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

  BrowserAccessibilityManager(
      const ui::AXTreeUpdate& initial_tree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

 private:
  // The following states keep track of whether or not the
  // on-screen keyboard is allowed to be shown.
  enum OnScreenKeyboardState {
    // Never show the on-screen keyboard because this tab is hidden.
    OSK_DISALLOWED_BECAUSE_TAB_HIDDEN,

    // This tab was just shown, so don't pop-up the on-screen keyboard if a
    // text field gets focus that wasn't the result of an explicit touch.
    OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED,

    // A touch event has occurred within the window, but focus has not
    // explicitly changed. Allow the on-screen keyboard to be shown if the
    // touch event was within the bounds of the currently focused object.
    // Otherwise we'll just wait to see if focus changes.
    OSK_ALLOWED_WITHIN_FOCUSED_OBJECT,

    // Focus has changed within a tab that's already visible. Allow the
    // on-screen keyboard to show anytime that a touch event leads to an
    // editable text control getting focus.
    OSK_ALLOWED
  };

 protected:
  // The object that can perform actions on our behalf.
  BrowserAccessibilityDelegate* delegate_;

  // Factory to create BrowserAccessibility objects (for dependency injection).
  scoped_ptr<BrowserAccessibilityFactory> factory_;

  // The underlying tree of accessibility objects.
  scoped_ptr<ui::AXSerializableTree> tree_;

  // The node that currently has focus.
  ui::AXNode* focus_;

  // A mapping from a node id to its wrapper of type BrowserAccessibility.
  base::hash_map<int32, BrowserAccessibility*> id_wrapper_map_;

  // True if the user has initiated a navigation to another page.
  bool user_is_navigating_away_;

  // The on-screen keyboard state.
  OnScreenKeyboardState osk_state_;

  BrowserAccessibilityFindInPageInfo find_in_page_info_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
