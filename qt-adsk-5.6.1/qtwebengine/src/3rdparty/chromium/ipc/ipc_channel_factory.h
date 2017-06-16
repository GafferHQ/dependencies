// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_FACTORY_H_
#define IPC_IPC_CHANNEL_FACTORY_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel.h"

namespace IPC {

class AttachmentBroker;

// Encapsulates how a Channel is created. A ChannelFactory can be
// passed to the constructor of ChannelProxy or SyncChannel to tell them
// how to create underlying channel.
class IPC_EXPORT ChannelFactory {
 public:
  // Creates a factory for "native" channel built through
  // IPC::Channel::Create().
  static scoped_ptr<ChannelFactory> Create(const ChannelHandle& handle,
                                           Channel::Mode mode,
                                           AttachmentBroker* broker);

  virtual ~ChannelFactory() { }
  virtual std::string GetName() const = 0;
  virtual scoped_ptr<Channel> BuildChannel(Listener* listener) = 0;
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_FACTORY_H_
