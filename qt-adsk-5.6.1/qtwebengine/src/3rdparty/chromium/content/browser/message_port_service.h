// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MESSAGE_PORT_SERVICE_H_
#define CONTENT_BROWSER_MESSAGE_PORT_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/message_port_types.h"
#include "ipc/ipc_message.h"

namespace content {
class MessagePortDelegate;

class CONTENT_EXPORT MessagePortService {
 public:
  typedef std::vector<std::pair<content::MessagePortMessage,
                                std::vector<TransferredMessagePort>>>
      QueuedMessages;

  // Returns the MessagePortService singleton.
  static MessagePortService* GetInstance();

  // These methods correspond to the message port related IPCs.
  void Create(int route_id,
              MessagePortDelegate* delegate,
              int* message_port_id);
  void Destroy(int message_port_id);
  void Entangle(int local_message_port_id, int remote_message_port_id);
  void PostMessage(
      int sender_message_port_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);
  void QueueMessages(int message_port_id);
  void SendQueuedMessages(int message_port_id,
                          const QueuedMessages& queued_messages);
  void ReleaseMessages(int message_port_id);

  // Updates the information needed to reach a message port when it's sent to a
  // (possibly different) process.
  void UpdateMessagePort(int message_port_id,
                         MessagePortDelegate* delegate,
                         int routing_id);

  // Returns the current information by which a message port can be reached.
  // Either |delegate| or |routing_id| can be null, in which case that bit of
  // information is not returned.
  void GetMessagePortInfo(int message_port_id,
                          MessagePortDelegate** delegate,
                          int* routing_id);

  // The message port is being transferred to a new renderer process, but the
  // code doing that isn't able to immediately update the message port with a
  // new filter and routing_id. This queues up all messages sent to this port
  // until later ReleaseMessages is called for this port (this will happen
  // automatically as soon as a WebMessagePortChannelImpl instance is created
  // for this port in the renderer. The browser side code is still responsible
  // for updating the port with a new filter before that happens. If ultimately
  // transfering the port to a new process fails, ClosePort should be called to
  // clean up the port.
  void HoldMessages(int message_port_id);

  // Closes and cleans up the message port.
  void ClosePort(int message_port_id);

  void OnMessagePortDelegateClosing(MessagePortDelegate* filter);

  // Attempts to send the queued messages for a message port.
  void SendQueuedMessagesIfPossible(int message_port_id);

 private:
  friend struct DefaultSingletonTraits<MessagePortService>;

  MessagePortService();
  ~MessagePortService();

  void PostMessageTo(
      int message_port_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);

  // Handles the details of removing a message port id. Before calling this,
  // verify that the message port id exists.
  void Erase(int message_port_id);

  struct MessagePort;
  typedef std::map<int, MessagePort> MessagePorts;
  MessagePorts message_ports_;

  // We need globally unique identifiers for each message port.
  int next_message_port_id_;

  DISALLOW_COPY_AND_ASSIGN(MessagePortService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MESSAGE_PORT_SERVICE_H_
