// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_POSIX_H_
#define IPC_IPC_CHANNEL_POSIX_H_

#include "ipc/ipc_channel.h"

#include <sys/socket.h>  // for CMSG macros

#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "ipc/ipc_channel_reader.h"
#include "ipc/ipc_message_attachment_set.h"

namespace IPC {

class IPC_EXPORT ChannelPosix : public Channel,
                                public internal::ChannelReader,
                                public base::MessageLoopForIO::Watcher {
 public:
  // |broker| must outlive the newly created object.
  ChannelPosix(const IPC::ChannelHandle& channel_handle,
               Mode mode,
               Listener* listener,
               AttachmentBroker* broker);
  ~ChannelPosix() override;

  // Channel implementation
  bool Connect() override;
  void Close() override;
  bool Send(Message* message) override;
  AttachmentBroker* GetAttachmentBroker() override;
  base::ProcessId GetPeerPID() const override;
  base::ProcessId GetSelfPID() const override;
  int GetClientFileDescriptor() const override;
  base::ScopedFD TakeClientFileDescriptor() override;

  // Returns true if the channel supports listening for connections.
  bool AcceptsConnections() const;

  // Returns true if the channel supports listening for connections and is
  // currently connected.
  bool HasAcceptedConnection() const;

  // Closes any currently connected socket, and returns to a listening state
  // for more connections.
  void ResetToAcceptingConnectionState();

  // Returns true if the peer process' effective user id can be determined, in
  // which case the supplied peer_euid is updated with it.
  bool GetPeerEuid(uid_t* peer_euid) const;

  void CloseClientFileDescriptor();

  static bool IsNamedServerInitialized(const std::string& channel_id);
#if defined(OS_LINUX)
  static void SetGlobalPid(int pid);
#endif  // OS_LINUX

 private:
  bool CreatePipe(const IPC::ChannelHandle& channel_handle);

  bool ProcessOutgoingMessages();

  bool AcceptConnection();
  void ClosePipeOnError();
  int GetHelloMessageProcId() const;
  void QueueHelloMessage();
  void CloseFileDescriptors(Message* msg);
  void QueueCloseFDMessage(int fd, int hops);

  // ChannelReader implementation.
  ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) override;
  bool WillDispatchInputMessage(Message* msg) override;
  bool DidEmptyInputBuffers() override;
  void HandleInternalMessage(const Message& msg) override;

  // Finds the set of file descriptors in the given message.  On success,
  // appends the descriptors to the input_fds_ member and returns true
  //
  // Returns false if the message was truncated. In this case, any handles that
  // were sent will be closed.
  bool ExtractFileDescriptorsFromMsghdr(msghdr* msg);

  // Closes all handles in the input_fds_ list and clears the list. This is
  // used to clean up handles in error conditions to avoid leaking the handles.
  void ClearInputFDs();

  // MessageLoopForIO::Watcher implementation.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  Mode mode_;

  base::ProcessId peer_pid_;

  // After accepting one client connection on our server socket we want to
  // stop listening.
  base::MessageLoopForIO::FileDescriptorWatcher
  server_listen_connection_watcher_;
  base::MessageLoopForIO::FileDescriptorWatcher read_watcher_;
  base::MessageLoopForIO::FileDescriptorWatcher write_watcher_;

  // Indicates whether we're currently blocked waiting for a write to complete.
  bool is_blocked_on_write_;
  bool waiting_connect_;

  // If sending a message blocks then we use this variable
  // to keep track of where we are.
  size_t message_send_bytes_written_;

  // File descriptor we're listening on for new connections if we listen
  // for connections.
  base::ScopedFD server_listen_pipe_;

  // The pipe used for communication.
  base::ScopedFD pipe_;

  // For a server, the client end of our socketpair() -- the other end of our
  // pipe_ that is passed to the client.
  base::ScopedFD client_pipe_;
  mutable base::Lock client_pipe_lock_;  // Lock that protects |client_pipe_|.

  // The "name" of our pipe.  On Windows this is the global identifier for
  // the pipe.  On POSIX it's used as a key in a local map of file descriptors.
  std::string pipe_name_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // We assume a worst case: kReadBufferSize bytes of messages, where each
  // message has no payload and a full complement of descriptors.
  static const size_t kMaxReadFDs =
      (Channel::kReadBufferSize / sizeof(IPC::Message::Header)) *
      MessageAttachmentSet::kMaxDescriptorsPerMessage;

  // Buffer size for file descriptors used for recvmsg. On Mac the CMSG macros
  // are not constant so we have to pick a "large enough" padding for headers.
#if defined(OS_MACOSX)
  static const size_t kMaxReadFDBuffer = 1024 + sizeof(int) * kMaxReadFDs;
#else
  static const size_t kMaxReadFDBuffer = CMSG_SPACE(sizeof(int) * kMaxReadFDs);
#endif
  static_assert(kMaxReadFDBuffer <= 8192,
                "kMaxReadFDBuffer too big for a stack buffer");

  // File descriptors extracted from messages coming off of the channel. The
  // handles may span messages and come off different channels from the message
  // data (in the case of READWRITE), and are processed in FIFO here.
  // NOTE: The implementation assumes underlying storage here is contiguous, so
  // don't change to something like std::deque<> without changing the
  // implementation!
  std::vector<int> input_fds_;


  void ResetSafely(base::ScopedFD* fd);
  bool in_dtor_;

#if defined(OS_MACOSX)
  // On OSX, sent FDs must not be closed until we get an ack.
  // Keep track of sent FDs here to make sure the remote is not
  // trying to bamboozle us.
  std::set<int> fds_to_close_;
#endif

  // True if we are responsible for unlinking the unix domain socket file.
  bool must_unlink_;

#if defined(OS_LINUX)
  // If non-zero, overrides the process ID sent in the hello message.
  static int global_pid_;
#endif  // OS_LINUX

  // |broker_| must outlive this instance.
  AttachmentBroker* broker_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChannelPosix);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_POSIX_H_
