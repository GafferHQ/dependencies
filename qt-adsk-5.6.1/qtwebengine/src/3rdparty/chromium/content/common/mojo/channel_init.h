// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_CHANNEL_INIT_H_
#define CONTENT_COMMON_MOJO_CHANNEL_INIT_H_

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ipc/mojo/scoped_ipc_support.h"
#include "third_party/mojo/src/mojo/edk/embedder/channel_info_forward.h"
#include "third_party/mojo/src/mojo/public/cpp/system/message_pipe.h"

namespace base {
class TaskRunner;
}

namespace content {

// ChannelInit handles creation and destruction of the Mojo channel. It is not
// thread-safe, but may be used on any single thread with a MessageLoop.
class CONTENT_EXPORT ChannelInit {
 public:
  ChannelInit();
  ~ChannelInit();

  // Initializes the channel. This takes ownership of |file|. Returns the
  // primordial MessagePipe for the channel.
  mojo::ScopedMessagePipeHandle Init(
      base::PlatformFile file,
      scoped_refptr<base::TaskRunner> io_thread_task_runner);

  // Notifies the channel that we (hence it) will soon be destroyed.
  void WillDestroySoon();

  // Shuts down the channel. Must be called from the IO thread.
  void ShutdownOnIOThread();

 private:
  // Invoked on the thread on which this object lives once the channel has been
  // established. This is a static method that takes a weak pointer to self,
  // since we want to destroy the channel if we were destroyed first.
  static void OnCreatedChannel(
      base::WeakPtr<ChannelInit> self,
      scoped_ptr<IPC::ScopedIPCSupport> ipc_support,
      mojo::embedder::ChannelInfo* channel);

  // If non-null the channel has been established.
  mojo::embedder::ChannelInfo* channel_info_;

  scoped_ptr<IPC::ScopedIPCSupport> ipc_support_;
  base::WeakPtrFactory<ChannelInit> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelInit);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_CHANNEL_INIT_H_
