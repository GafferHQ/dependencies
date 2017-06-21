// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_message_pipe_reader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/mojo/async_handle_waiter.h"
#include "ipc/mojo/ipc_channel_mojo.h"

namespace IPC {
namespace internal {

MessagePipeReader::MessagePipeReader(mojo::ScopedMessagePipeHandle handle,
                                     MessagePipeReader::Delegate* delegate)
    : pipe_(handle.Pass()),
      delegate_(delegate),
      async_waiter_(
          new AsyncHandleWaiter(base::Bind(&MessagePipeReader::PipeIsReady,
                                           base::Unretained(this)))),
      pending_send_error_(MOJO_RESULT_OK) {
}

MessagePipeReader::~MessagePipeReader() {
  // The pipe should be closed before deletion.
  CHECK(!IsValid());
  DCHECK_EQ(pending_send_error_, MOJO_RESULT_OK);
}

void MessagePipeReader::Close() {
  // All pending errors should be signaled before Close().
  DCHECK_EQ(pending_send_error_, MOJO_RESULT_OK);
  async_waiter_.reset();
  pipe_.reset();
  OnPipeClosed();
}

void MessagePipeReader::CloseWithError(MojoResult error) {
  OnPipeError(error);
  Close();
}

void MessagePipeReader::CloseWithErrorIfPending() {
  if (pending_send_error_ == MOJO_RESULT_OK)
    return;
  MojoResult error = pending_send_error_;
  pending_send_error_ = MOJO_RESULT_OK;
  CloseWithError(error);
  return;
}

void MessagePipeReader::CloseWithErrorLater(MojoResult error) {
  pending_send_error_ = error;
}

bool MessagePipeReader::Send(scoped_ptr<Message> message) {
  DCHECK(IsValid());

  message->TraceMessageBegin();
  std::vector<MojoHandle> handles;
  MojoResult result = MOJO_RESULT_OK;
  result = ChannelMojo::ReadFromMessageAttachmentSet(message.get(), &handles);
  if (result == MOJO_RESULT_OK) {
    result = MojoWriteMessage(handle(),
                              message->data(),
                              static_cast<uint32>(message->size()),
                              handles.empty() ? nullptr : &handles[0],
                              static_cast<uint32>(handles.size()),
                              MOJO_WRITE_MESSAGE_FLAG_NONE);
  }

  if (result != MOJO_RESULT_OK) {
    std::for_each(handles.begin(), handles.end(), &MojoClose);
    // We cannot call CloseWithError() here as Send() is protected by
    // ChannelMojo's lock and CloseWithError() could re-enter ChannelMojo. We
    // cannot call CloseWithError() also because Send() can be called from
    // non-UI thread while OnPipeError() expects to be called on IO thread.
    CloseWithErrorLater(result);
    return false;
  }

  return true;
}

void MessagePipeReader::OnMessageReceived() {
  Message message(data_buffer().empty() ? "" : &data_buffer()[0],
                  static_cast<uint32>(data_buffer().size()));

  std::vector<MojoHandle> handle_buffer;
  TakeHandleBuffer(&handle_buffer);
  MojoResult write_result =
      ChannelMojo::WriteToMessageAttachmentSet(handle_buffer, &message);
  if (write_result != MOJO_RESULT_OK) {
    CloseWithError(write_result);
    return;
  }

  message.TraceMessageEnd();
  delegate_->OnMessageReceived(message);
}

void MessagePipeReader::OnPipeClosed() {
  if (!delegate_)
    return;
  delegate_->OnPipeClosed(this);
  delegate_ = nullptr;
}

void MessagePipeReader::OnPipeError(MojoResult error) {
  if (!delegate_)
    return;
  delegate_->OnPipeError(this);
}

MojoResult MessagePipeReader::ReadMessageBytes() {
  DCHECK(handle_buffer_.empty());

  uint32_t num_bytes = static_cast<uint32_t>(data_buffer_.size());
  uint32_t num_handles = 0;
  MojoResult result = MojoReadMessage(pipe_.get().value(),
                                      num_bytes ? &data_buffer_[0] : nullptr,
                                      &num_bytes,
                                      nullptr,
                                      &num_handles,
                                      MOJO_READ_MESSAGE_FLAG_NONE);
  data_buffer_.resize(num_bytes);
  handle_buffer_.resize(num_handles);
  if (result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    // MOJO_RESULT_RESOURCE_EXHAUSTED was asking the caller that
    // it needs more bufer. So we re-read it with resized buffers.
    result = MojoReadMessage(pipe_.get().value(),
                             num_bytes ? &data_buffer_[0] : nullptr,
                             &num_bytes,
                             num_handles ? &handle_buffer_[0] : nullptr,
                             &num_handles,
                             MOJO_READ_MESSAGE_FLAG_NONE);
  }

  DCHECK(0 == num_bytes || data_buffer_.size() == num_bytes);
  DCHECK(0 == num_handles || handle_buffer_.size() == num_handles);
  return result;
}

void MessagePipeReader::ReadAvailableMessages() {
  while (pipe_.is_valid()) {
    MojoResult read_result = ReadMessageBytes();
    if (read_result == MOJO_RESULT_SHOULD_WAIT)
      break;
    if (read_result != MOJO_RESULT_OK) {
      DLOG(WARNING)
          << "Pipe got error from ReadMessage(). Closing: " << read_result;
      OnPipeError(read_result);
      Close();
      break;
    }

    OnMessageReceived();
  }

}

void MessagePipeReader::ReadMessagesThenWait() {
  while (true) {
    ReadAvailableMessages();
    if (!pipe_.is_valid())
      break;
    // |Wait()| is safe to call only after all messages are read.
    // If can fail with |MOJO_RESULT_ALREADY_EXISTS| otherwise.
    // Also, we don't use MOJO_HANDLE_SIGNAL_WRITABLE here, expecting buffer in
    // MessagePipe.
    MojoResult result =
        async_waiter_->Wait(pipe_.get().value(), MOJO_HANDLE_SIGNAL_READABLE);
    // If the result is |MOJO_RESULT_ALREADY_EXISTS|, there could be messages
    // that have been arrived after the last |ReadAvailableMessages()|.
    // We have to consume then and retry in that case.
    if (result != MOJO_RESULT_ALREADY_EXISTS) {
      if (result != MOJO_RESULT_OK) {
        LOG(ERROR) << "Failed to wait on the pipe. Result is " << result;
        OnPipeError(result);
        Close();
      }

      break;
    }
  }
}

void MessagePipeReader::PipeIsReady(MojoResult wait_result) {
  CloseWithErrorIfPending();
  if (!IsValid()) {
    // There was a pending error and it closed the pipe.
    // We cannot do the work anymore.
    return;
  }

  if (wait_result != MOJO_RESULT_OK) {
    if (wait_result != MOJO_RESULT_ABORTED) {
      // FAILED_PRECONDITION happens every time the peer is dead so
      // it isn't worth polluting the log message.
      LOG_IF(WARNING, wait_result != MOJO_RESULT_FAILED_PRECONDITION)
          << "Pipe got error from the waiter. Closing: " << wait_result;
      OnPipeError(wait_result);
    }

    Close();
    return;
  }

  ReadMessagesThenWait();
}

void MessagePipeReader::DelayedDeleter::operator()(
    MessagePipeReader* ptr) const {
  ptr->Close();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&DeleteNow, ptr));
}

}  // namespace internal
}  // namespace IPC
