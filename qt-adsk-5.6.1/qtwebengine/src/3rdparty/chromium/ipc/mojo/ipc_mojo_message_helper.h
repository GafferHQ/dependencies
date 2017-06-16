// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_IPC_MOJO_MESSAGE_HELPER_H_
#define IPC_MOJO_IPC_MOJO_MESSAGE_HELPER_H_

#include "ipc/ipc_export.h"
#include "ipc/ipc_message.h"
#include "third_party/mojo/src/mojo/public/cpp/system/message_pipe.h"

namespace IPC {

// Reads and writes |mojo::MessagePipe| from/to |Message|.
class IPC_MOJO_EXPORT MojoMessageHelper {
 public:
  static bool WriteMessagePipeTo(Message* message,
                                 mojo::ScopedMessagePipeHandle handle);
  static bool ReadMessagePipeFrom(const Message* message,
                                  base::PickleIterator* iter,
                                  mojo::ScopedMessagePipeHandle* handle);

 private:
  MojoMessageHelper();
};

}  // namespace IPC

#endif  // IPC_MOJO_IPC_MOJO_MESSAGE_HELPER_H_
