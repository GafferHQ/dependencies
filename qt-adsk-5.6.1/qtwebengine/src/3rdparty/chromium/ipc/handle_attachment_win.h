// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_ATTACHMENT_WIN_H_
#define IPC_HANDLE_ATTACHMENT_WIN_H_

#include <stdint.h>

#include "base/process/process_handle.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_export.h"

namespace IPC {
namespace internal {

// This class represents a Windows HANDLE attached to a Chrome IPC message.
class IPC_EXPORT HandleAttachmentWin : public BrokerableAttachment {
 public:
  // The wire format for this handle.
  struct IPC_EXPORT WireFormat {
    // The HANDLE that is intended for duplication, or the HANDLE that has been
    // duplicated, depending on context.
    // The type is int32_t instead of HANDLE because HANDLE gets typedefed to
    // void*, whose size varies between 32 and 64-bit processes. Using a
    // int32_t means that 64-bit processes will need to perform both up-casting
    // and down-casting. This is performed using the appropriate Windows apis.
    int32_t handle;
    // The id of the destination process that the handle is duplicated into.
    base::ProcessId destination_process;
    AttachmentId attachment_id;
  };

  HandleAttachmentWin(const HANDLE& handle);

  BrokerableType GetBrokerableType() const override;

  // Returns the wire format of this attachment.
  WireFormat GetWireFormat(const base::ProcessId& destination) const;

 private:
  ~HandleAttachmentWin() override;
  HANDLE handle_;
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_HANDLE_ATTACHMENT_WIN_H_
