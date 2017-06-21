// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_FRAME_SWAP_MESSAGE_QUEUE_H_
#define CONTENT_RENDERER_GPU_FRAME_SWAP_MESSAGE_QUEUE_H_

#include <map>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "cc/output/swap_promise.h"
#include "content/common/content_export.h"
#include "content/renderer/message_delivery_policy.h"

namespace IPC {
class Message;
};

namespace content {

class FrameSwapMessageSubQueue;

// Queue used to keep track of which IPC::Messages should be sent along with a
// particular compositor frame swap.
class CONTENT_EXPORT FrameSwapMessageQueue
    : public base::RefCountedThreadSafe<FrameSwapMessageQueue> {
 public:
  class CONTENT_EXPORT SendMessageScope {
   public:
    virtual ~SendMessageScope() {}
  };

  FrameSwapMessageQueue();

  // Queues message to be returned on a matching DrainMessages call.
  //
  // |policy| determines how messages are matched with DrainMessages calls.
  // |source_frame_number| frame number to queue |msg| for.
  // |msg| - message to queue. The method takes ownership of |msg|.
  // |is_first| - output parameter. Set to true if this was the first message
  //              enqueued for the given source_frame_number.
  void QueueMessageForFrame(MessageDeliveryPolicy policy,
                            int source_frame_number,
                            scoped_ptr<IPC::Message> msg,
                            bool* is_first);

  // Returns true if there are no messages in the queue.
  bool Empty() const;

  // Should be called when a successful activation occurs. The messages for
  // that activation can be obtained by calling DrainMessages.
  //
  // |source_frame_number| frame number for which the activate occurred.
  void DidActivate(int source_frame_number);

  // Should be called when a successful swap occurs. The messages for that
  // swap can be obtained by calling DrainMessages.
  //
  // |source_frame_number| frame number for which the swap occurred.
  void DidSwap(int source_frame_number);

  // Should be called when we know a swap will not occur.
  //
  // |source_frame_number| frame number for which the swap will not occur.
  // |reason| reason for the which the swap will not occur.
  // |messages| depending on |reason| it may make sense to deliver certain
  //            messages asynchronously. This vector will contain those
  //            messages.
  void DidNotSwap(int source_frame_number,
                  cc::SwapPromise::DidNotSwapReason reason,
                  ScopedVector<IPC::Message>* messages);

  // A SendMessageScope object must be held by the caller when this method is
  // called.
  //
  // |messages| vector to store messages, it's not cleared, only appended to.
  //            The method will append messages queued for frame numbers lower
  //            or equal to |source_frame_number|
  void DrainMessages(ScopedVector<IPC::Message>* messages);

  // SendMessageScope is used to make sure that messages sent from different
  // threads (impl/main) are scheduled in the right order on the IO threads.
  //
  // Returns an object that must be kept in scope till an IPC message containing
  // |messages| is sent.
  scoped_ptr<SendMessageScope> AcquireSendMessageScope();

  static void TransferMessages(ScopedVector<IPC::Message>& source,
                               std::vector<IPC::Message>* dest);

 private:
  friend class base::RefCountedThreadSafe<FrameSwapMessageQueue>;

  FrameSwapMessageSubQueue* GetSubQueue(MessageDeliveryPolicy policy);

  ~FrameSwapMessageQueue();

  mutable base::Lock lock_;
  scoped_ptr<FrameSwapMessageSubQueue> visual_state_queue_;
  scoped_ptr<FrameSwapMessageSubQueue> swap_queue_;
  ScopedVector<IPC::Message> next_drain_messages_;

  DISALLOW_COPY_AND_ASSIGN(FrameSwapMessageQueue);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_FRAME_SWAP_MESSAGE_QUEUE_H_
