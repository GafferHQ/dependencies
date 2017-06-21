// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_DELEGATE_H_

#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace content {
struct MessagePortMessage;
struct TransferredMessagePort;

// Delegate used by MessagePortService to send messages to message ports to the
// correct renderer. Delegates are responsible for managing their own lifetime,
// and should call MessagePortService::OnMessagePortDelegateClosing if they are
// destroyed while there are still message ports associated with them.
class CONTENT_EXPORT MessagePortDelegate {
 public:
  // Sends a message to the given route. Implementations are responsible for
  // updating MessagePortService with new routes for the sent message ports.
  virtual void SendMessage(
      int route_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports) = 0;

  // Called when MessagePortService tried to send a message to a port, but
  // instead added it to its queue because the port is currently configured to
  // hold all its messages.
  virtual void MessageWasHeld(int route_id) {}

  // Requests messages to the given route to be queued.
  virtual void SendMessagesAreQueued(int route_id) = 0;

 protected:
  virtual ~MessagePortDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_DELEGATE_H_
