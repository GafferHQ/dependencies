// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MESSAGE_PIPE_H_
#define MOJO_EDK_SYSTEM_MESSAGE_PIPE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/channel_endpoint_client.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/memory.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/message_pipe_endpoint.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace system {

class Awakable;
class Channel;
class ChannelEndpoint;
class MessageInTransitQueue;

// |MessagePipe| is the secondary object implementing a message pipe (see the
// explanatory comment in core.cc). It is typically owned by the dispatcher(s)
// corresponding to the local endpoints. This class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT MessagePipe final : public ChannelEndpointClient {
 public:
  // Creates a |MessagePipe| with two new |LocalMessagePipeEndpoint|s.
  static MessagePipe* CreateLocalLocal();

  // Creates a |MessagePipe| with a |LocalMessagePipeEndpoint| on port 0 and a
  // |ProxyMessagePipeEndpoint| on port 1. |*channel_endpoint| is set to the
  // (newly-created) |ChannelEndpoint| for the latter.
  static MessagePipe* CreateLocalProxy(
      scoped_refptr<ChannelEndpoint>* channel_endpoint);

  // Similar to |CreateLocalProxy()|, except that it'll do so from an existing
  // |ChannelEndpoint| (whose |ReplaceClient()| it'll call) and take
  // |message_queue|'s contents as already-received incoming messages. If
  // |channel_endpoint| is null, this will create a "half-open" message pipe.
  static MessagePipe* CreateLocalProxyFromExisting(
      MessageInTransitQueue* message_queue,
      ChannelEndpoint* channel_endpoint);

  // Creates a |MessagePipe| with a |ProxyMessagePipeEndpoint| on port 0 and a
  // |LocalMessagePipeEndpoint| on port 1. |*channel_endpoint| is set to the
  // (newly-created) |ChannelEndpoint| for the former.
  // Note: This is really only needed in tests (outside of tests, this
  // configuration arises from a local message pipe having its port 0
  // "converted" using |ConvertLocalToProxy()|).
  static MessagePipe* CreateProxyLocal(
      scoped_refptr<ChannelEndpoint>* channel_endpoint);

  // Gets the other port number (i.e., 0 -> 1, 1 -> 0).
  static unsigned GetPeerPort(unsigned port);

  // Used by |MessagePipeDispatcher::Deserialize()|. Returns true on success (in
  // which case, |*message_pipe|/|*port| are set appropriately) and false on
  // failure (in which case |*message_pipe| may or may not be set to null).
  static bool Deserialize(Channel* channel,
                          const void* source,
                          size_t size,
                          scoped_refptr<MessagePipe>* message_pipe,
                          unsigned* port);

  // Gets the type of the endpoint (used for assertions, etc.).
  MessagePipeEndpoint::Type GetType(unsigned port);

  // These are called by the dispatcher to implement its methods of
  // corresponding names. In all cases, the port |port| must be open.
  void CancelAllAwakables(unsigned port);
  void Close(unsigned port);
  // Unlike |MessagePipeDispatcher::WriteMessage()|, this does not validate its
  // arguments.
  MojoResult WriteMessage(unsigned port,
                          UserPointer<const void> bytes,
                          uint32_t num_bytes,
                          std::vector<DispatcherTransport>* transports,
                          MojoWriteMessageFlags flags);
  MojoResult ReadMessage(unsigned port,
                         UserPointer<void> bytes,
                         UserPointer<uint32_t> num_bytes,
                         DispatcherVector* dispatchers,
                         uint32_t* num_dispatchers,
                         MojoReadMessageFlags flags);
  HandleSignalsState GetHandleSignalsState(unsigned port) const;
  MojoResult AddAwakable(unsigned port,
                         Awakable* awakable,
                         MojoHandleSignals signals,
                         uint32_t context,
                         HandleSignalsState* signals_state);
  void RemoveAwakable(unsigned port,
                      Awakable* awakable,
                      HandleSignalsState* signals_state);
  void StartSerialize(unsigned port,
                      Channel* channel,
                      size_t* max_size,
                      size_t* max_platform_handles);
  bool EndSerialize(unsigned port,
                    Channel* channel,
                    void* destination,
                    size_t* actual_size,
                    embedder::PlatformHandleVector* platform_handles);

  // |ChannelEndpointClient| methods:
  bool OnReadMessage(unsigned port, MessageInTransit* message) override;
  void OnDetachFromChannel(unsigned port) override;

 private:
  MessagePipe();
  ~MessagePipe() override;

  // This is used internally by |WriteMessage()| and by |OnReadMessage()|.
  // |transports| may be non-null only if it's nonempty and |message| has no
  // dispatchers attached. Must be called with |lock_| held.
  MojoResult EnqueueMessageNoLock(unsigned port,
                                  scoped_ptr<MessageInTransit> message,
                                  std::vector<DispatcherTransport>* transports);

  // Helper for |EnqueueMessageNoLock()|. Must be called with |lock_| held.
  MojoResult AttachTransportsNoLock(
      unsigned port,
      MessageInTransit* message,
      std::vector<DispatcherTransport>* transports);

  base::Lock lock_;  // Protects the following members.
  scoped_ptr<MessagePipeEndpoint> endpoints_[2];

  MOJO_DISALLOW_COPY_AND_ASSIGN(MessagePipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MESSAGE_PIPE_H_
