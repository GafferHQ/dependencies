// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_RAW_CHANNEL_H_
#define MOJO_EDK_SYSTEM_RAW_CHANNEL_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/message_in_transit_queue.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace base {
class MessageLoopForIO;
}

namespace mojo {
namespace system {

// |RawChannel| is an interface and base class for objects that wrap an OS
// "pipe". It presents the following interface to users:
//  - Receives and dispatches messages on an I/O thread (running a
//    |MessageLoopForIO|.
//  - Provides a thread-safe way of writing messages (|WriteMessage()|);
//    writing/queueing messages will not block and is atomic from the point of
//    view of the caller. If necessary, messages are queued (to be written on
//    the aforementioned thread).
//
// OS-specific implementation subclasses are to be instantiated using the
// |Create()| static factory method.
//
// With the exception of |WriteMessage()|, this class is thread-unsafe (and in
// general its methods should only be used on the I/O thread, i.e., the thread
// on which |Init()| is called).
class MOJO_SYSTEM_IMPL_EXPORT RawChannel {
 public:
  // This object may be destroyed on any thread (if |Init()| was called, after
  // |Shutdown()| was called).
  virtual ~RawChannel();

  // The |Delegate| is only accessed on the same thread as the message loop
  // (passed in on creation).
  class MOJO_SYSTEM_IMPL_EXPORT Delegate {
   public:
    enum Error {
      // Failed read due to raw channel shutdown (e.g., on the other side).
      ERROR_READ_SHUTDOWN,
      // Failed read due to raw channel being broken (e.g., if the other side
      // died without shutting down).
      ERROR_READ_BROKEN,
      // Received a bad message.
      ERROR_READ_BAD_MESSAGE,
      // Unknown read error.
      ERROR_READ_UNKNOWN,
      // Generic write error.
      ERROR_WRITE
    };

    // Called when a message is read. This may call the |RawChannel|'s
    // |Shutdown()| and then (if desired) destroy it.
    virtual void OnReadMessage(
        const MessageInTransit::View& message_view,
        embedder::ScopedPlatformHandleVectorPtr platform_handles) = 0;

    // Called when there's a (fatal) error. This may call the |RawChannel|'s
    // |Shutdown()| and then (if desired) destroy it.
    //
    // For each raw channel, there'll be at most one |ERROR_READ_...| and at
    // most one |ERROR_WRITE| notification. After |OnError(ERROR_READ_...)|,
    // |OnReadMessage()| won't be called again.
    virtual void OnError(Error error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Static factory method. |handle| should be a handle to a
  // (platform-appropriate) bidirectional communication channel (e.g., a socket
  // on POSIX, a named pipe on Windows).
  static scoped_ptr<RawChannel> Create(embedder::ScopedPlatformHandle handle);

  // This must be called (on an I/O thread) before this object is used. Does
  // *not* take ownership of |delegate|. Both the I/O thread and |delegate| must
  // remain alive until |Shutdown()| is called (unless this fails); |delegate|
  // will no longer be used after |Shutdown()|.
  void Init(Delegate* delegate);

  // This must be called (on the I/O thread) before this object is destroyed.
  void Shutdown();

  // Writes the given message (or schedules it to be written). |message| must
  // have no |Dispatcher|s still attached (i.e.,
  // |SerializeAndCloseDispatchers()| should have been called). This method is
  // thread-safe and may be called from any thread. Returns true on success.
  bool WriteMessage(scoped_ptr<MessageInTransit> message);

  // Returns true if the write buffer is empty (i.e., all messages written using
  // |WriteMessage()| have actually been sent.
  // TODO(vtl): We should really also notify our delegate when the write buffer
  // becomes empty (or something like that).
  bool IsWriteBufferEmpty();

  // Returns the amount of space needed in the |MessageInTransit|'s
  // |TransportData|'s "platform handle table" per platform handle (to be
  // attached to a message). (This amount may be zero.)
  virtual size_t GetSerializedPlatformHandleSize() const = 0;

 protected:
  // Result of I/O operations.
  enum IOResult {
    IO_SUCCEEDED,
    // Failed due to a (probably) clean shutdown (e.g., of the other end).
    IO_FAILED_SHUTDOWN,
    // Failed due to the connection being broken (e.g., the other end dying).
    IO_FAILED_BROKEN,
    // Failed due to some other (unexpected) reason.
    IO_FAILED_UNKNOWN,
    IO_PENDING
  };

  class MOJO_SYSTEM_IMPL_EXPORT ReadBuffer {
   public:
    ReadBuffer();
    ~ReadBuffer();

    void GetBuffer(char** addr, size_t* size);

   private:
    friend class RawChannel;

    // We store data from |[Schedule]Read()|s in |buffer_|. The start of
    // |buffer_| is always aligned with a message boundary (we will copy memory
    // to ensure this), but |buffer_| may be larger than the actual number of
    // bytes we have.
    std::vector<char> buffer_;
    size_t num_valid_bytes_;

    MOJO_DISALLOW_COPY_AND_ASSIGN(ReadBuffer);
  };

  class MOJO_SYSTEM_IMPL_EXPORT WriteBuffer {
   public:
    struct Buffer {
      const char* addr;
      size_t size;
    };

    explicit WriteBuffer(size_t serialized_platform_handle_size);
    ~WriteBuffer();

    // Returns true if there are (more) platform handles to be sent (from the
    // front of |message_queue_|).
    bool HavePlatformHandlesToSend() const;
    // Gets platform handles to be sent (from the front of |message_queue_|).
    // This should only be called if |HavePlatformHandlesToSend()| returned
    // true. There are two components to this: the actual |PlatformHandle|s
    // (which should be closed once sent) and any additional serialization
    // information (which will be embedded in the message's data; there are
    // |GetSerializedPlatformHandleSize()| bytes per handle). Once all platform
    // handles have been sent, the message data should be written next (see
    // |GetBuffers()|).
    // TODO(vtl): Maybe this method should be const, but
    // |PlatformHandle::CloseIfNecessary()| isn't const (and actually modifies
    // state).
    void GetPlatformHandlesToSend(size_t* num_platform_handles,
                                  embedder::PlatformHandle** platform_handles,
                                  void** serialization_data);

    // Gets buffers to be written. These buffers will always come from the front
    // of |message_queue_|. Once they are completely written, the front
    // |MessageInTransit| should be popped (and destroyed); this is done in
    // |OnWriteCompletedNoLock()|.
    void GetBuffers(std::vector<Buffer>* buffers) const;

   private:
    friend class RawChannel;

    const size_t serialized_platform_handle_size_;

    MessageInTransitQueue message_queue_;
    // Platform handles are sent before the message data, but doing so may
    // require several passes. |platform_handles_offset_| indicates the position
    // in the first message's vector of platform handles to send next.
    size_t platform_handles_offset_;
    // The first message's data may have been partially sent. |data_offset_|
    // indicates the position in the first message's data to start the next
    // write.
    size_t data_offset_;

    MOJO_DISALLOW_COPY_AND_ASSIGN(WriteBuffer);
  };

  RawChannel();

  // |result| must not be |IO_PENDING|. Must be called on the I/O thread WITHOUT
  // |write_lock_| held. This object may be destroyed by this call.
  void OnReadCompleted(IOResult io_result, size_t bytes_read);
  // |result| must not be |IO_PENDING|. Must be called on the I/O thread WITHOUT
  // |write_lock_| held. This object may be destroyed by this call.
  void OnWriteCompleted(IOResult io_result,
                        size_t platform_handles_written,
                        size_t bytes_written);

  base::MessageLoopForIO* message_loop_for_io() { return message_loop_for_io_; }
  base::Lock& write_lock() { return write_lock_; }

  // Should only be called on the I/O thread.
  ReadBuffer* read_buffer() { return read_buffer_.get(); }

  // Only called under |write_lock_|.
  WriteBuffer* write_buffer_no_lock() {
    write_lock_.AssertAcquired();
    return write_buffer_.get();
  }

  // Adds |message| to the write message queue. Implementation subclasses may
  // override this to add any additional "control" messages needed. This is
  // called (on any thread) with |write_lock_| held.
  virtual void EnqueueMessageNoLock(scoped_ptr<MessageInTransit> message);

  // Handles any control messages targeted to the |RawChannel| (or
  // implementation subclass). Implementation subclasses may override this to
  // handle any implementation-specific control messages, but should call
  // |RawChannel::OnReadMessageForRawChannel()| for any remaining messages.
  // Returns true on success and false on error (e.g., invalid control message).
  // This is only called on the I/O thread.
  virtual bool OnReadMessageForRawChannel(
      const MessageInTransit::View& message_view);

  // Reads into |read_buffer()|.
  // This class guarantees that:
  // - the area indicated by |GetBuffer()| will stay valid until read completion
  //   (but please also see the comments for |OnShutdownNoLock()|);
  // - a second read is not started if there is a pending read;
  // - the method is called on the I/O thread WITHOUT |write_lock_| held.
  //
  // The implementing subclass must guarantee that:
  // - |bytes_read| is untouched unless |Read()| returns |IO_SUCCEEDED|;
  // - if the method returns |IO_PENDING|, |OnReadCompleted()| will be called on
  //   the I/O thread to report the result, unless |Shutdown()| is called.
  virtual IOResult Read(size_t* bytes_read) = 0;
  // Similar to |Read()|, except that the implementing subclass must also
  // guarantee that the method doesn't succeed synchronously, i.e., it only
  // returns |IO_FAILED_...| or |IO_PENDING|.
  virtual IOResult ScheduleRead() = 0;

  // Called by |OnReadCompleted()| to get the platform handles associated with
  // the given platform handle table (from a message). This should only be
  // called when |num_platform_handles| is nonzero. Returns null if the
  // |num_platform_handles| handles are not available. Only called on the I/O
  // thread (without |write_lock_| held).
  virtual embedder::ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_platform_handles,
      const void* platform_handle_table) = 0;

  // Writes contents in |write_buffer_no_lock()|.
  // This class guarantees that:
  // - the |PlatformHandle|s given by |GetPlatformHandlesToSend()| and the
  //   buffer(s) given by |GetBuffers()| will remain valid until write
  //   completion (see also the comments for |OnShutdownNoLock()|);
  // - a second write is not started if there is a pending write;
  // - the method is called under |write_lock_|.
  //
  // The implementing subclass must guarantee that:
  // - |platform_handles_written| and |bytes_written| are untouched unless
  //   |WriteNoLock()| returns |IO_SUCCEEDED|;
  // - if the method returns |IO_PENDING|, |OnWriteCompleted()| will be called
  //   on the I/O thread to report the result, unless |Shutdown()| is called.
  virtual IOResult WriteNoLock(size_t* platform_handles_written,
                               size_t* bytes_written) = 0;
  // Similar to |WriteNoLock()|, except that the implementing subclass must also
  // guarantee that the method doesn't succeed synchronously, i.e., it only
  // returns |IO_FAILED_...| or |IO_PENDING|.
  virtual IOResult ScheduleWriteNoLock() = 0;

  // Must be called on the I/O thread WITHOUT |write_lock_| held.
  virtual void OnInit() = 0;
  // On shutdown, passes the ownership of the buffers to subclasses, which may
  // want to preserve them if there are pending read/writes. After this is
  // called, |OnReadCompleted()| must no longer be called. Must be called on the
  // I/O thread under |write_lock_|.
  virtual void OnShutdownNoLock(scoped_ptr<ReadBuffer> read_buffer,
                                scoped_ptr<WriteBuffer> write_buffer) = 0;

 private:
  // Converts an |IO_FAILED_...| for a read to a |Delegate::Error|.
  static Delegate::Error ReadIOResultToError(IOResult io_result);

  // Calls |delegate_->OnError(error)|. Must be called on the I/O thread WITHOUT
  // |write_lock_| held. This object may be destroyed by this call.
  void CallOnError(Delegate::Error error);

  // If |io_result| is |IO_SUCCESS|, updates the write buffer and schedules a
  // write operation to run later if there is more to write. If |io_result| is
  // failure or any other error occurs, cancels pending writes and returns
  // false. Must be called under |write_lock_| and only if |write_stopped_| is
  // false.
  bool OnWriteCompletedNoLock(IOResult io_result,
                              size_t platform_handles_written,
                              size_t bytes_written);

  // Set in |Init()| and never changed (hence usable on any thread without
  // locking):
  base::MessageLoopForIO* message_loop_for_io_;

  // Only used on the I/O thread:
  Delegate* delegate_;
  bool* set_on_shutdown_;
  scoped_ptr<ReadBuffer> read_buffer_;

  base::Lock write_lock_;  // Protects the following members.
  bool write_stopped_;
  scoped_ptr<WriteBuffer> write_buffer_;

  // This is used for posting tasks from write threads to the I/O thread. It
  // must only be accessed under |write_lock_|. The weak pointers it produces
  // are only used/invalidated on the I/O thread.
  base::WeakPtrFactory<RawChannel> weak_ptr_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RawChannel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_RAW_CHANNEL_H_
