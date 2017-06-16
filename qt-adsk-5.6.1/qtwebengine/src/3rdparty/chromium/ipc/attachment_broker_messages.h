// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages used by the attachment broker.
// Multiply-included message file, hence no include guard.

#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_message_macros.h"

#if defined(OS_WIN)
#include "ipc/handle_attachment_win.h"
#endif  // defined(OS_WIN)

// ----------------------------------------------------------------------------
// Serialization of structs.
// ----------------------------------------------------------------------------

#if defined(OS_WIN)
IPC_STRUCT_TRAITS_BEGIN(IPC::internal::HandleAttachmentWin::WireFormat)
IPC_STRUCT_TRAITS_MEMBER(handle)
IPC_STRUCT_TRAITS_MEMBER(destination_process)
IPC_STRUCT_TRAITS_MEMBER(attachment_id)
IPC_STRUCT_TRAITS_END()
#endif  // defined(OS_WIN)

#define IPC_MESSAGE_START AttachmentBrokerMsgStart

// ----------------------------------------------------------------------------
// Messages sent from the broker to a non-broker process.
// ----------------------------------------------------------------------------

#if defined(OS_WIN)
// Sent from a broker to a non-broker to indicate that a windows HANDLE has been
// brokered. Contains all information necessary for the non-broker to translate
// a BrokerAttachment::AttachmentId to a BrokerAttachment.
IPC_MESSAGE_CONTROL1(
    AttachmentBrokerMsg_WinHandleHasBeenDuplicated,
    IPC::internal::HandleAttachmentWin::WireFormat /* wire_format */)
#endif  // defined(OS_WIN)

// ----------------------------------------------------------------------------
// Messages sent from a non-broker process to a broker process.
// ----------------------------------------------------------------------------

#if defined(OS_WIN)
// Sent from a non-broker to a broker to request the duplication of a HANDLE
// into a different process (possibly the broker process, or even the original
// process).
IPC_MESSAGE_CONTROL1(
    AttachmentBrokerMsg_DuplicateWinHandle,
    IPC::internal::HandleAttachmentWin::WireFormat /* wire_format */)
#endif  // defined(OS_WIN)
