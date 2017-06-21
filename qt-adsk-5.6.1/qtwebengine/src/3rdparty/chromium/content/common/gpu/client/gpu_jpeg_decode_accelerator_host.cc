// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_jpeg_decode_accelerator_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_handle.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

namespace content {

// Class to receive AcceleratedJpegDecoderHostMsg_DecodeAck IPC message on IO
// thread. This does very similar what MessageFilter usually does. It is not
// MessageFilter because GpuChannelHost doesn't support AddFilter.
class GpuJpegDecodeAcceleratorHost::Receiver : public IPC::Listener,
                                               public base::NonThreadSafe {
 public:
  Receiver(Client* client,
           const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : client_(client),
        io_task_runner_(io_task_runner),
        weak_factory_for_io_(this) {
    DCHECK(CalledOnValidThread());
  }

  ~Receiver() override { DCHECK(CalledOnValidThread()); }

  void InvalidateWeakPtr(base::WaitableEvent* event) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    weak_factory_for_io_.InvalidateWeakPtrs();
    event->Signal();
  }

  // IPC::Listener implementation.
  void OnChannelError() override {
    DCHECK(io_task_runner_->BelongsToCurrentThread());

    OnDecodeAck(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
  }

  bool OnMessageReceived(const IPC::Message& msg) override {
    DCHECK(io_task_runner_->BelongsToCurrentThread());

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(GpuJpegDecodeAcceleratorHost::Receiver, msg)
    IPC_MESSAGE_HANDLER(AcceleratedJpegDecoderHostMsg_DecodeAck, OnDecodeAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    DCHECK(handled);
    return handled;
  }

  base::WeakPtr<IPC::Listener> AsWeakPtrForIO() {
    return weak_factory_for_io_.GetWeakPtr();
  }

 private:
  void OnDecodeAck(int32_t bitstream_buffer_id, Error error) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());

    if (!client_)
      return;

    if (error == media::JpegDecodeAccelerator::NO_ERRORS) {
      client_->VideoFrameReady(bitstream_buffer_id);
    } else {
      // Only NotifyError once.
      // Client::NotifyError() may trigger deletion of |this| (on another
      // thread), so calling it needs to be the last thing done on this stack!
      media::JpegDecodeAccelerator::Client* client = nullptr;
      std::swap(client, client_);
      client->NotifyError(bitstream_buffer_id, error);
    }
  }

  Client* client_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Weak pointers will be invalidated on IO thread.
  base::WeakPtrFactory<Receiver> weak_factory_for_io_;

  DISALLOW_COPY_AND_ASSIGN(Receiver);
};

GpuJpegDecodeAcceleratorHost::GpuJpegDecodeAcceleratorHost(
    GpuChannelHost* channel,
    int32 route_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : channel_(channel),
      decoder_route_id_(route_id),
      io_task_runner_(io_task_runner) {
  DCHECK(channel_);
  DCHECK_NE(decoder_route_id_, MSG_ROUTING_NONE);
}

GpuJpegDecodeAcceleratorHost::~GpuJpegDecodeAcceleratorHost() {
  DCHECK(CalledOnValidThread());
  Send(new AcceleratedJpegDecoderMsg_Destroy(decoder_route_id_));

  if (receiver_) {
    channel_->RemoveRoute(decoder_route_id_);

    // Invalidate weak ptr of |receiver_|. After that, no more messages will be
    // routed to |receiver_| on IO thread.
    base::WaitableEvent event(false, false);
    io_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&Receiver::InvalidateWeakPtr,
                                         base::Unretained(receiver_.get()),
                                         base::Unretained(&event)));
    event.Wait();
  }
}

bool GpuJpegDecodeAcceleratorHost::Initialize(
    media::JpegDecodeAccelerator::Client* client) {
  DCHECK(CalledOnValidThread());

  bool succeeded = false;
  // This cannot be on IO thread because the msg is synchronous.
  Send(new GpuMsg_CreateJpegDecoder(decoder_route_id_, &succeeded));

  if (!succeeded) {
    DLOG(ERROR) << "Send(GpuMsg_CreateJpegDecoder()) failed";
    return false;
  }

  receiver_.reset(new Receiver(client, io_task_runner_));

  return true;
}

void GpuJpegDecodeAcceleratorHost::Decode(
    const media::BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DCHECK(CalledOnValidThread());

  DCHECK(
      base::SharedMemory::IsHandleValid(video_frame->shared_memory_handle()));

  base::SharedMemoryHandle input_handle =
      channel_->ShareToGpuProcess(bitstream_buffer.handle());
  if (!base::SharedMemory::IsHandleValid(input_handle)) {
    DLOG(ERROR) << "Failed to duplicate handle of BitstreamBuffer";
    return;
  }
  base::SharedMemoryHandle output_handle =
      channel_->ShareToGpuProcess(video_frame->shared_memory_handle());
  if (!base::SharedMemory::IsHandleValid(output_handle)) {
    DLOG(ERROR) << "Failed to duplicate handle of VideoFrame";
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
    if (input_handle.auto_close) {
      // Defer closing task to the ScopedFD.
      base::ScopedFD(input_handle.fd);
    }
#else
    // TODO(kcwu) fix the handle leak after crbug.com/493414 resolved.
#endif
    return;
  }

  size_t output_buffer_size = media::VideoFrame::AllocationSize(
      video_frame->format(), video_frame->coded_size());

  AcceleratedJpegDecoderMsg_Decode_Params decode_params;
  decode_params.coded_size = video_frame->coded_size();
  decode_params.input_buffer_id = bitstream_buffer.id();
  decode_params.input_buffer_handle = input_handle;
  decode_params.input_buffer_size = bitstream_buffer.size();
  decode_params.output_video_frame_handle = output_handle;
  decode_params.output_buffer_size = output_buffer_size;
  Send(new AcceleratedJpegDecoderMsg_Decode(decoder_route_id_, decode_params));
}

void GpuJpegDecodeAcceleratorHost::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());

  if (!channel_->Send(message)) {
    DLOG(ERROR) << "Send(" << message->type() << ") failed";
  }
}

base::WeakPtr<IPC::Listener> GpuJpegDecodeAcceleratorHost::GetReceiver() {
  return receiver_->AsWeakPtrForIO();
}

}  // namespace content
