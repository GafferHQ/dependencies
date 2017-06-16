// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/mojo/client_channel.mojom.h"
#include "ipc/mojo/ipc_mojo_bootstrap.h"
#include "ipc/mojo/ipc_mojo_handle_attachment.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace IPC {

namespace {

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(scoped_refptr<base::TaskRunner> io_runner,
                     ChannelHandle channel_handle,
                     Channel::Mode mode,
                     AttachmentBroker* broker)
      : io_runner_(io_runner),
        channel_handle_(channel_handle),
        mode_(mode),
        broker_(broker) {}

  std::string GetName() const override {
    return channel_handle_.name;
  }

  scoped_ptr<Channel> BuildChannel(Listener* listener) override {
    return ChannelMojo::Create(io_runner_, channel_handle_, mode_, listener,
                               broker_);
  }

 private:
  scoped_refptr<base::TaskRunner> io_runner_;
  ChannelHandle channel_handle_;
  Channel::Mode mode_;
  AttachmentBroker* broker_;
};

//------------------------------------------------------------------------------

class ClientChannelMojo : public ChannelMojo, public ClientChannel {
 public:
  ClientChannelMojo(scoped_refptr<base::TaskRunner> io_runner,
                    const ChannelHandle& handle,
                    Listener* listener,
                    AttachmentBroker* broker);
  ~ClientChannelMojo() override;
  // MojoBootstrap::Delegate implementation
  void OnPipeAvailable(mojo::embedder::ScopedPlatformHandle handle) override;

  // ClientChannel implementation
  void Init(
      mojo::ScopedMessagePipeHandle pipe,
      int32_t peer_pid,
      const mojo::Callback<void(int32_t)>& callback) override;

 private:
  void BindPipe(mojo::ScopedMessagePipeHandle handle);
  void OnConnectionError();

  mojo::Binding<ClientChannel> binding_;
  base::WeakPtrFactory<ClientChannelMojo> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientChannelMojo);
};

ClientChannelMojo::ClientChannelMojo(scoped_refptr<base::TaskRunner> io_runner,
                                     const ChannelHandle& handle,
                                     Listener* listener,
                                     AttachmentBroker* broker)
    : ChannelMojo(io_runner, handle, Channel::MODE_CLIENT, listener, broker),
      binding_(this),
      weak_factory_(this) {
}

ClientChannelMojo::~ClientChannelMojo() {
}

void ClientChannelMojo::OnPipeAvailable(
    mojo::embedder::ScopedPlatformHandle handle) {
  CreateMessagingPipe(handle.Pass(), base::Bind(&ClientChannelMojo::BindPipe,
                                                weak_factory_.GetWeakPtr()));
}

void ClientChannelMojo::Init(
    mojo::ScopedMessagePipeHandle pipe,
    int32_t peer_pid,
    const mojo::Callback<void(int32_t)>& callback) {
  InitMessageReader(pipe.Pass(), static_cast<base::ProcessId>(peer_pid));
  callback.Run(GetSelfPID());
}

void ClientChannelMojo::BindPipe(mojo::ScopedMessagePipeHandle handle) {
  binding_.Bind(handle.Pass());
}

void ClientChannelMojo::OnConnectionError() {
  listener()->OnChannelError();
}

//------------------------------------------------------------------------------

class ServerChannelMojo : public ChannelMojo {
 public:
  ServerChannelMojo(scoped_refptr<base::TaskRunner> io_runner,
                    const ChannelHandle& handle,
                    Listener* listener,
                    AttachmentBroker* broker);
  ~ServerChannelMojo() override;

  // MojoBootstrap::Delegate implementation
  void OnPipeAvailable(mojo::embedder::ScopedPlatformHandle handle) override;
  // Channel override
  void Close() override;

 private:
  void InitClientChannel(mojo::ScopedMessagePipeHandle peer_handle,
                         mojo::ScopedMessagePipeHandle handle);
  void OnConnectionError();

  // ClientChannelClient implementation
  void ClientChannelWasInitialized(int32_t peer_pid);

  mojo::InterfacePtr<ClientChannel> client_channel_;
  mojo::ScopedMessagePipeHandle message_pipe_;
  base::WeakPtrFactory<ServerChannelMojo> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServerChannelMojo);
};

ServerChannelMojo::ServerChannelMojo(scoped_refptr<base::TaskRunner> io_runner,
                                     const ChannelHandle& handle,
                                     Listener* listener,
                                     AttachmentBroker* broker)
    : ChannelMojo(io_runner, handle, Channel::MODE_SERVER, listener, broker),
      weak_factory_(this) {
}

ServerChannelMojo::~ServerChannelMojo() {
  Close();
}

void ServerChannelMojo::OnPipeAvailable(
    mojo::embedder::ScopedPlatformHandle handle) {
  mojo::ScopedMessagePipeHandle peer;
  MojoResult create_result =
      mojo::CreateMessagePipe(nullptr, &message_pipe_, &peer);
  if (create_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "mojo::CreateMessagePipe failed: " << create_result;
    listener()->OnChannelError();
    return;
  }
  CreateMessagingPipe(
      handle.Pass(),
      base::Bind(&ServerChannelMojo::InitClientChannel,
                 weak_factory_.GetWeakPtr(), base::Passed(&peer)));
}

void ServerChannelMojo::Close() {
  client_channel_.reset();
  message_pipe_.reset();
  ChannelMojo::Close();
}

void ServerChannelMojo::InitClientChannel(
    mojo::ScopedMessagePipeHandle peer_handle,
    mojo::ScopedMessagePipeHandle handle) {
  client_channel_.Bind(
      mojo::InterfacePtrInfo<ClientChannel>(handle.Pass(), 0u));
  client_channel_.set_connection_error_handler(base::Bind(
      &ServerChannelMojo::OnConnectionError, base::Unretained(this)));
  client_channel_->Init(
      peer_handle.Pass(), static_cast<int32_t>(GetSelfPID()),
      base::Bind(&ServerChannelMojo::ClientChannelWasInitialized,
                 base::Unretained(this)));
}

void ServerChannelMojo::OnConnectionError() {
  listener()->OnChannelError();
}

void ServerChannelMojo::ClientChannelWasInitialized(int32_t peer_pid) {
  InitMessageReader(message_pipe_.Pass(), peer_pid);
}

#if defined(OS_POSIX) && !defined(OS_NACL)

base::ScopedFD TakeOrDupFile(internal::PlatformFileAttachment* attachment) {
  return attachment->Owns() ? base::ScopedFD(attachment->TakePlatformFile())
                            : base::ScopedFD(dup(attachment->file()));
}

#endif

}  // namespace

//------------------------------------------------------------------------------

ChannelMojo::ChannelInfoDeleter::ChannelInfoDeleter(
    scoped_refptr<base::TaskRunner> io_runner)
    : io_runner(io_runner) {
}

ChannelMojo::ChannelInfoDeleter::~ChannelInfoDeleter() {
}

void ChannelMojo::ChannelInfoDeleter::operator()(
    mojo::embedder::ChannelInfo* ptr) const {
  if (base::ThreadTaskRunnerHandle::Get() == io_runner) {
    mojo::embedder::DestroyChannelOnIOThread(ptr);
  } else {
    io_runner->PostTask(
        FROM_HERE, base::Bind(&mojo::embedder::DestroyChannelOnIOThread, ptr));
  }
}

//------------------------------------------------------------------------------

// static
bool ChannelMojo::ShouldBeUsed() {
  // TODO(rockot): Investigate performance bottlenecks and hopefully reenable
  // this at some point. http://crbug.com/500019
  return false;
}

// static
scoped_ptr<ChannelMojo> ChannelMojo::Create(
    scoped_refptr<base::TaskRunner> io_runner,
    const ChannelHandle& channel_handle,
    Mode mode,
    Listener* listener,
    AttachmentBroker* broker) {
  switch (mode) {
    case Channel::MODE_CLIENT:
      return make_scoped_ptr(
          new ClientChannelMojo(io_runner, channel_handle, listener, broker));
    case Channel::MODE_SERVER:
      return make_scoped_ptr(
          new ServerChannelMojo(io_runner, channel_handle, listener, broker));
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateServerFactory(
    scoped_refptr<base::TaskRunner> io_runner,
    const ChannelHandle& channel_handle,
    AttachmentBroker* broker) {
  return make_scoped_ptr(new MojoChannelFactory(io_runner, channel_handle,
                                                Channel::MODE_SERVER, broker));
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateClientFactory(
    scoped_refptr<base::TaskRunner> io_runner,
    const ChannelHandle& channel_handle,
    AttachmentBroker* broker) {
  return make_scoped_ptr(new MojoChannelFactory(io_runner, channel_handle,
                                                Channel::MODE_CLIENT, broker));
}

ChannelMojo::ChannelMojo(scoped_refptr<base::TaskRunner> io_runner,
                         const ChannelHandle& handle,
                         Mode mode,
                         Listener* listener,
                         AttachmentBroker* broker)
    : listener_(listener),
      peer_pid_(base::kNullProcessId),
      io_runner_(io_runner),
      channel_info_(nullptr, ChannelInfoDeleter(nullptr)),
      waiting_connect_(true),
      weak_factory_(this) {
  // Create MojoBootstrap after all members are set as it touches
  // ChannelMojo from a different thread.
  bootstrap_ = MojoBootstrap::Create(handle, mode, this, broker);
  if (io_runner == base::MessageLoop::current()->task_runner()) {
    InitOnIOThread();
  } else {
    io_runner->PostTask(FROM_HERE, base::Bind(&ChannelMojo::InitOnIOThread,
                                              base::Unretained(this)));
  }
}

ChannelMojo::~ChannelMojo() {
  Close();
}

void ChannelMojo::InitOnIOThread() {
  ipc_support_.reset(
      new ScopedIPCSupport(base::MessageLoop::current()->task_runner()));
}

void ChannelMojo::CreateMessagingPipe(
    mojo::embedder::ScopedPlatformHandle handle,
    const CreateMessagingPipeCallback& callback) {
  auto return_callback = base::Bind(&ChannelMojo::OnMessagingPipeCreated,
                                    weak_factory_.GetWeakPtr(), callback);
  if (base::ThreadTaskRunnerHandle::Get() == io_runner_) {
    CreateMessagingPipeOnIOThread(
        handle.Pass(), base::ThreadTaskRunnerHandle::Get(), return_callback);
  } else {
    io_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ChannelMojo::CreateMessagingPipeOnIOThread,
                   base::Passed(&handle), base::ThreadTaskRunnerHandle::Get(),
                   return_callback));
  }
}

// static
void ChannelMojo::CreateMessagingPipeOnIOThread(
    mojo::embedder::ScopedPlatformHandle handle,
    scoped_refptr<base::TaskRunner> callback_runner,
    const CreateMessagingPipeOnIOThreadCallback& callback) {
  mojo::embedder::ChannelInfo* channel_info;
  mojo::ScopedMessagePipeHandle pipe =
      mojo::embedder::CreateChannelOnIOThread(handle.Pass(), &channel_info);
  if (base::ThreadTaskRunnerHandle::Get() == callback_runner) {
    callback.Run(pipe.Pass(), channel_info);
  } else {
    callback_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(&pipe), channel_info));
  }
}

void ChannelMojo::OnMessagingPipeCreated(
    const CreateMessagingPipeCallback& callback,
    mojo::ScopedMessagePipeHandle handle,
    mojo::embedder::ChannelInfo* channel_info) {
  DCHECK(!channel_info_.get());
  channel_info_ = scoped_ptr<mojo::embedder::ChannelInfo, ChannelInfoDeleter>(
      channel_info, ChannelInfoDeleter(io_runner_));
  callback.Run(handle.Pass());
}

bool ChannelMojo::Connect() {
  DCHECK(!message_reader_);
  return bootstrap_->Connect();
}

void ChannelMojo::Close() {
  scoped_ptr<internal::MessagePipeReader, ReaderDeleter> to_be_deleted;

  {
    // |message_reader_| has to be cleared inside the lock,
    // but the instance has to be deleted outside.
    base::AutoLock l(lock_);
    to_be_deleted = message_reader_.Pass();
    // We might Close() before we Connect().
    waiting_connect_ = false;
  }

  channel_info_.reset();
  ipc_support_.reset();
  to_be_deleted.reset();
}

void ChannelMojo::OnBootstrapError() {
  listener_->OnChannelError();
}

namespace {

// ClosingDeleter calls |CloseWithErrorIfPending| before deleting the
// |MessagePipeReader|.
struct ClosingDeleter {
  typedef base::DefaultDeleter<internal::MessagePipeReader> DefaultType;

  void operator()(internal::MessagePipeReader* ptr) const {
    ptr->CloseWithErrorIfPending();
    delete ptr;
  }
};

} // namespace

void ChannelMojo::InitMessageReader(mojo::ScopedMessagePipeHandle pipe,
                                    int32_t peer_pid) {
  scoped_ptr<internal::MessagePipeReader, ClosingDeleter> reader(
      new internal::MessagePipeReader(pipe.Pass(), this));

  {
    base::AutoLock l(lock_);
    for (size_t i = 0; i < pending_messages_.size(); ++i) {
      bool sent = reader->Send(make_scoped_ptr(pending_messages_[i]));
      pending_messages_[i] = nullptr;
      if (!sent) {
        // OnChannelError() is notified through ClosingDeleter.
        pending_messages_.clear();
        LOG(ERROR)  << "Failed to flush pending messages";
        return;
      }
    }

    // We set |message_reader_| here and won't get any |pending_messages_|
    // hereafter. Although we might have some if there is an error, we don't
    // care. They cannot be sent anyway.
    message_reader_.reset(reader.release());
    pending_messages_.clear();
    waiting_connect_ = false;
  }

  set_peer_pid(peer_pid);
  listener_->OnChannelConnected(static_cast<int32_t>(GetPeerPID()));
  if (message_reader_)
    message_reader_->ReadMessagesThenWait();
}

void ChannelMojo::OnPipeClosed(internal::MessagePipeReader* reader) {
  Close();
}

void ChannelMojo::OnPipeError(internal::MessagePipeReader* reader) {
  listener_->OnChannelError();
}


// Warning: Keep the implementation thread-safe.
bool ChannelMojo::Send(Message* message) {
  base::AutoLock l(lock_);
  if (!message_reader_) {
    pending_messages_.push_back(message);
    // Counts as OK before the connection is established, but it's an
    // error otherwise.
    return waiting_connect_;
  }

  return message_reader_->Send(make_scoped_ptr(message));
}

bool ChannelMojo::IsSendThreadSafe() const {
  return false;
}

base::ProcessId ChannelMojo::GetPeerPID() const {
  return peer_pid_;
}

base::ProcessId ChannelMojo::GetSelfPID() const {
  return bootstrap_->GetSelfPID();
}

void ChannelMojo::OnMessageReceived(Message& message) {
  TRACE_EVENT2("ipc,toplevel", "ChannelMojo::OnMessageReceived",
               "class", IPC_MESSAGE_ID_CLASS(message.type()),
               "line", IPC_MESSAGE_ID_LINE(message.type()));
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

#if defined(OS_POSIX) && !defined(OS_NACL)
int ChannelMojo::GetClientFileDescriptor() const {
  return bootstrap_->GetClientFileDescriptor();
}

base::ScopedFD ChannelMojo::TakeClientFileDescriptor() {
  return bootstrap_->TakeClientFileDescriptor();
}
#endif  // defined(OS_POSIX) && !defined(OS_NACL)

// static
MojoResult ChannelMojo::ReadFromMessageAttachmentSet(
    Message* message,
    std::vector<MojoHandle>* handles) {
  // We dup() the handles in IPC::Message to transmit.
  // IPC::MessageAttachmentSet has intricate lifecycle semantics
  // of FDs, so just to dup()-and-own them is the safest option.
  if (message->HasAttachments()) {
    MessageAttachmentSet* set = message->attachment_set();
    for (unsigned i = 0; i < set->size(); ++i) {
      scoped_refptr<MessageAttachment> attachment = set->GetAttachmentAt(i);
      switch (attachment->GetType()) {
        case MessageAttachment::TYPE_PLATFORM_FILE:
#if defined(OS_POSIX) && !defined(OS_NACL)
        {
          base::ScopedFD file =
              TakeOrDupFile(static_cast<IPC::internal::PlatformFileAttachment*>(
                  attachment.get()));
          if (!file.is_valid()) {
            DPLOG(WARNING) << "Failed to dup FD to transmit.";
            set->CommitAll();
            return MOJO_RESULT_UNKNOWN;
          }

          MojoHandle wrapped_handle;
          MojoResult wrap_result = CreatePlatformHandleWrapper(
              mojo::embedder::ScopedPlatformHandle(
                  mojo::embedder::PlatformHandle(file.release())),
              &wrapped_handle);
          if (MOJO_RESULT_OK != wrap_result) {
            LOG(WARNING) << "Pipe failed to wrap handles. Closing: "
                         << wrap_result;
            set->CommitAll();
            return wrap_result;
          }

          handles->push_back(wrapped_handle);
        }
#else
          NOTREACHED();
#endif  //  defined(OS_POSIX) && !defined(OS_NACL)
        break;
        case MessageAttachment::TYPE_MOJO_HANDLE: {
          mojo::ScopedHandle handle =
              static_cast<IPC::internal::MojoHandleAttachment*>(
                  attachment.get())->TakeHandle();
          handles->push_back(handle.release().value());
        } break;
        case MessageAttachment::TYPE_BROKERABLE_ATTACHMENT:
          // Brokerable attachments are handled by the AttachmentBroker so
          // there's no need to do anything here.
          NOTREACHED();
          break;
      }
    }

    set->CommitAll();
  }

  return MOJO_RESULT_OK;
}

// static
MojoResult ChannelMojo::WriteToMessageAttachmentSet(
    const std::vector<MojoHandle>& handle_buffer,
    Message* message) {
  for (size_t i = 0; i < handle_buffer.size(); ++i) {
    bool ok = message->attachment_set()->AddAttachment(
        new IPC::internal::MojoHandleAttachment(
            mojo::MakeScopedHandle(mojo::Handle(handle_buffer[i]))));
    DCHECK(ok);
    if (!ok) {
      LOG(ERROR) << "Failed to add new Mojo handle.";
      return MOJO_RESULT_UNKNOWN;
    }
  }

  return MOJO_RESULT_OK;
}

}  // namespace IPC
