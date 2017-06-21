// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SYNC_MESSAGE_FILTER_H_
#define IPC_IPC_SYNC_MESSAGE_FILTER_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/message_filter.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace IPC {

// This MessageFilter allows sending synchronous IPC messages from a thread
// other than the listener thread associated with the SyncChannel.  It does not
// support fancy features that SyncChannel does, such as handling recursion or
// receiving messages while waiting for a response.  Note that this object can
// be used to send simultaneous synchronous messages from different threads.
class IPC_EXPORT SyncMessageFilter : public MessageFilter, public Sender {
 public:
  explicit SyncMessageFilter(base::WaitableEvent* shutdown_event);

  // MessageSender implementation.
  bool Send(Message* message) override;

  // MessageFilter implementation.
  void OnFilterAdded(Sender* sender) override;
  void OnChannelError() override;
  void OnChannelClosing() override;
  bool OnMessageReceived(const Message& message) override;

 protected:
  ~SyncMessageFilter() override;

 private:
  void SendOnIOThread(Message* message);
  // Signal all the pending sends as done, used in an error condition.
  void SignalAllEvents();

  // The channel to which this filter was added.
  Sender* sender_;

  // The process's main thread.
  scoped_refptr<base::SingleThreadTaskRunner> listener_task_runner_;

  // The message loop where the Channel lives.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  typedef std::set<PendingSyncMsg*> PendingSyncMessages;
  PendingSyncMessages pending_sync_messages_;

  // Locks data members above.
  base::Lock lock_;

  base::WaitableEvent* shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(SyncMessageFilter);
};

}  // namespace IPC

#endif  // IPC_IPC_SYNC_MESSAGE_FILTER_H_
