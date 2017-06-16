// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_JPEG_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_JPEG_DECODE_ACCELERATOR_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class GpuChannel;

class GpuJpegDecodeAccelerator
    : public IPC::Sender,
      public base::NonThreadSafe,
      public base::SupportsWeakPtr<GpuJpegDecodeAccelerator> {
 public:
  // |channel| must outlive this object.
  GpuJpegDecodeAccelerator(
      GpuChannel* channel,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~GpuJpegDecodeAccelerator() override;

  void AddClient(int32 route_id, IPC::Message* reply_msg);

  void NotifyDecodeStatus(int32 route_id,
                          int32_t bitstream_buffer_id,
                          media::JpegDecodeAccelerator::Error error);

  // Function to delegate sending to actual sender.
  bool Send(IPC::Message* message) override;

 private:
  class Client;
  class MessageFilter;

  void ClientRemoved();

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the GpuJpegDecodeAccelerator that they own when
  // they are destroyed. So a raw pointer is safe.
  GpuChannel* channel_;

  // The message filter to run JpegDecodeAccelerator::Decode on IO thread.
  scoped_refptr<MessageFilter> filter_;

  // GPU child task runner.
  scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Number of clients added to |filter_|.
  int client_number_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuJpegDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_JPEG_DECODE_ACCELERATOR_H_
