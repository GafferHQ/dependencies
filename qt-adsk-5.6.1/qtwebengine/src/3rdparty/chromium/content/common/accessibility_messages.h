// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for accessibility.
// Multiply-included message file, hence no include guard.

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

// Singly-included section for custom types.
#ifndef CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
#define CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_

typedef std::map<int32, int> FrameIDMap;

#endif  // CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START AccessibilityMsgStart

IPC_STRUCT_BEGIN(AccessibilityHostMsg_EventParams)
  // The tree update.
  IPC_STRUCT_MEMBER(ui::AXTreeUpdate, update)

  // Mapping from node id to routing id of its child frame - either the
  // routing id of a RenderFrame or a RenderFrameProxy for an out-of-process
  // iframe.
  IPC_STRUCT_MEMBER(FrameIDMap, node_to_frame_routing_id_map)

  // Mapping from node id to the browser plugin instance id of a child
  // browser plugin.
  IPC_STRUCT_MEMBER(FrameIDMap, node_to_browser_plugin_instance_id_map)

  // Type of event.
  IPC_STRUCT_MEMBER(ui::AXEvent, event_type)

  // ID of the node that the event applies to.
  IPC_STRUCT_MEMBER(int, id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_LocationChangeParams)
  // ID of the object whose location is changing.
  IPC_STRUCT_MEMBER(int, id)

  // The object's new location, in frame-relative coordinates (same
  // as the coordinates in AccessibilityNodeData).
  IPC_STRUCT_MEMBER(gfx::Rect, new_location)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_FindInPageResultParams)
  // The find in page request id.
  IPC_STRUCT_MEMBER(int, request_id)

  // The index of the result match.
  IPC_STRUCT_MEMBER(int, match_index)

  // The id of the accessibility object for the start of the match range.
  IPC_STRUCT_MEMBER(int, start_id)

  // The character offset into the text of the start object.
  IPC_STRUCT_MEMBER(int, start_offset)

  // The id of the accessibility object for the end of the match range.
  IPC_STRUCT_MEMBER(int, end_id)

  // The character offset into the text of the end object.
  IPC_STRUCT_MEMBER(int, end_offset)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Relay a request from assistive technology to set focus to a given node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_SetFocus,
                    int /* object id */)

// Relay a request from assistive technology to perform the default action
// on a given node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_DoDefaultAction,
                    int /* object id */)

// Relay a request from assistive technology to make a given object
// visible by scrolling as many scrollable containers as possible.
// In addition, if it's not possible to make the entire object visible,
// scroll so that the |subfocus| rect is visible at least. The subfocus
// rect is in local coordinates of the object itself.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_ScrollToMakeVisible,
                    int /* object id */,
                    gfx::Rect /* subfocus */)

// Relay a request from assistive technology to show the context menu for a
// given object.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_ShowContextMenu, int /* object id */)

// Relay a request from assistive technology to move a given object
// to a specific location, in the WebContents area coordinate space, i.e.
// (0, 0) is the top-left corner of the WebContents.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_ScrollToPoint,
                    int /* object id */,
                    gfx::Point /* new location */)

// Relay a request from assistive technology to set the scroll offset
// of an accessibility object that's a scroll container, to a specific
// offset.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_SetScrollOffset,
                    int /* object id */,
                    gfx::Point /* new offset */)

// Relay a request from assistive technology to set the cursor or
// selection within an editable text element.
IPC_MESSAGE_ROUTED3(AccessibilityMsg_SetTextSelection,
                    int /* object id */,
                    int /* New start offset */,
                    int /* New end offset */)

// Relay a request from assistive technology to set the value of an
// editable text element.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_SetValue,
                    int /* object id */,
                    base::string16 /* Value */)

// Determine the accessibility object under a given point and reply with
// a AccessibilityHostMsg_HitTestResult with the same id.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_HitTest,
                    gfx::Point /* location to test */)

// Relay a request from assistive technology to set accessibility focus
// to a given node. On platforms where this is used (currently Android),
// inline text boxes are only computed for the node with accessibility focus,
// rather than for the whole tree.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_SetAccessibilityFocus,
                    int /* object id */)

// Tells the render view that a AccessibilityHostMsg_Events
// message was processed and it can send addition events.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_Events_ACK)

// Tell the renderer to reset and send a new accessibility tree from
// scratch because the browser is out of sync. It passes a sequential
// reset token. This should be rare, and if we need reset the same renderer
// too many times we just kill it. After sending a reset, the browser ignores
// incoming accessibility IPCs until it receives one with the matching reset
// token. Conversely, it ignores IPCs with a reset token if it was not
// expecting a reset.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_Reset,
                    int /* reset token */)

// Kill the renderer because we got a fatal error in the accessibility tree
// and we've already reset too many times.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_FatalError)

// Request a one-time snapshot of the accessibility tree without
// enabling accessibility if it wasn't already enabled. The passed id
// will be returned in the AccessibilityHostMsg_SnapshotResponse message.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_SnapshotTree,
                    int /* callback id */)

// Messages sent from the renderer to the browser.

// Sent to notify the browser about renderer accessibility events.
// The browser responds with a AccessibilityMsg_Events_ACK.
// The second parameter, reset_token, is set if this IPC was sent in response
// to a reset request from the browser. When the browser requests a reset,
// it ignores incoming IPCs until it sees one with the correct reset token.
// Any other time, it ignores IPCs with a reset token.
IPC_MESSAGE_ROUTED2(
    AccessibilityHostMsg_Events,
    std::vector<AccessibilityHostMsg_EventParams> /* events */,
    int /* reset_token */)

// Sent to update the browser of the location of accessibility objects.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_LocationChanges,
    std::vector<AccessibilityHostMsg_LocationChangeParams>)

// Sent to update the browser of the location of accessibility objects.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_FindInPageResult,
    AccessibilityHostMsg_FindInPageResultParams)

// Sent in response to AccessibilityMsg_SnapshotTree. The callback id that was
// passed to the request will be returned in |callback_id|, along with
// a standalone snapshot of the accessibility tree.
IPC_MESSAGE_ROUTED2(AccessibilityHostMsg_SnapshotResponse,
                    int /* callback_id */,
                    ui::AXTreeUpdate)
