// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_BROKERABLE_ATTACHMENT_H_
#define IPC_BROKERABLE_ATTACHMENT_H_

#include <stdint.h>

#include "base/macros.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_message_attachment.h"

namespace IPC {

// This subclass of MessageAttachment requires an AttachmentBroker to be
// attached to a Chrome IPC message.
class IPC_EXPORT BrokerableAttachment : public MessageAttachment {
 public:
  static const size_t kNonceSize = 16;
  // An id uniquely identifies an attachment sent via a broker.
  struct IPC_EXPORT AttachmentId {
    uint8_t nonce[kNonceSize];
  };

  enum BrokerableType {
    WIN_HANDLE,
  };

  // The identifier is unique across all Chrome processes.
  AttachmentId GetIdentifier() const;

  // Returns TYPE_BROKERABLE_ATTACHMENT
  Type GetType() const override;

  virtual BrokerableType GetBrokerableType() const = 0;

 protected:
  BrokerableAttachment();
  ~BrokerableAttachment() override;

 private:
  // This member uniquely identifies a BrokerableAttachment across all Chrome
  // processes.
  const AttachmentId id_;
  DISALLOW_COPY_AND_ASSIGN(BrokerableAttachment);
};

}  // namespace IPC

#endif  // IPC_BROKERABLE_ATTACHMENT_H_
