// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/memory.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

namespace embedder {
class PlatformSharedBufferMapping;
}

namespace system {

class Channel;
class Core;
class Dispatcher;
class DispatcherTransport;
class HandleTable;
class LocalMessagePipeEndpoint;
class ProxyMessagePipeEndpoint;
class TransportData;
class Awakable;

using DispatcherVector = std::vector<scoped_refptr<Dispatcher>>;

namespace test {

// Test helper. We need to declare it here so we can friend it.
MOJO_SYSTEM_IMPL_EXPORT DispatcherTransport
DispatcherTryStartTransport(Dispatcher* dispatcher);

}  // namespace test

// A |Dispatcher| implements Mojo primitives that are "attached" to a particular
// handle. This includes most (all?) primitives except for |MojoWait...()|. This
// object is thread-safe, with its state being protected by a single lock
// |lock_|, which is also made available to implementation subclasses (via the
// |lock()| method).
class MOJO_SYSTEM_IMPL_EXPORT Dispatcher
    : public base::RefCountedThreadSafe<Dispatcher> {
 public:
  enum class Type {
    UNKNOWN = 0,
    MESSAGE_PIPE,
    DATA_PIPE_PRODUCER,
    DATA_PIPE_CONSUMER,
    SHARED_BUFFER,

    // "Private" types (not exposed via the public interface):
    PLATFORM_HANDLE = -1
  };
  virtual Type GetType() const = 0;

  // These methods implement the various primitives named |Mojo...()|. These
  // take |lock_| and handle races with |Close()|. Then they call out to
  // subclasses' |...ImplNoLock()| methods (still under |lock_|), which actually
  // implement the primitives.
  // NOTE(vtl): This puts a big lock around each dispatcher (i.e., handle), and
  // prevents the various |...ImplNoLock()|s from releasing the lock as soon as
  // possible. If this becomes an issue, we can rethink this.
  MojoResult Close();

  // |transports| may be non-null if and only if there are handles to be
  // written; not that |this| must not be in |transports|. On success, all the
  // dispatchers in |transports| must have been moved to a closed state; on
  // failure, they should remain in their original state.
  MojoResult WriteMessage(UserPointer<const void> bytes,
                          uint32_t num_bytes,
                          std::vector<DispatcherTransport>* transports,
                          MojoWriteMessageFlags flags);
  // |dispatchers| must be non-null but empty, if |num_dispatchers| is non-null
  // and nonzero. On success, it will be set to the dispatchers to be received
  // (and assigned handles) as part of the message.
  MojoResult ReadMessage(UserPointer<void> bytes,
                         UserPointer<uint32_t> num_bytes,
                         DispatcherVector* dispatchers,
                         uint32_t* num_dispatchers,
                         MojoReadMessageFlags flags);
  MojoResult WriteData(UserPointer<const void> elements,
                       UserPointer<uint32_t> elements_num_bytes,
                       MojoWriteDataFlags flags);
  MojoResult BeginWriteData(UserPointer<void*> buffer,
                            UserPointer<uint32_t> buffer_num_bytes,
                            MojoWriteDataFlags flags);
  MojoResult EndWriteData(uint32_t num_bytes_written);
  MojoResult ReadData(UserPointer<void> elements,
                      UserPointer<uint32_t> num_bytes,
                      MojoReadDataFlags flags);
  MojoResult BeginReadData(UserPointer<const void*> buffer,
                           UserPointer<uint32_t> buffer_num_bytes,
                           MojoReadDataFlags flags);
  MojoResult EndReadData(uint32_t num_bytes_read);
  // |options| may be null. |new_dispatcher| must not be null, but
  // |*new_dispatcher| should be null (and will contain the dispatcher for the
  // new handle on success).
  MojoResult DuplicateBufferHandle(
      UserPointer<const MojoDuplicateBufferHandleOptions> options,
      scoped_refptr<Dispatcher>* new_dispatcher);
  MojoResult MapBuffer(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      scoped_ptr<embedder::PlatformSharedBufferMapping>* mapping);

  // Gets the current handle signals state. (The default implementation simply
  // returns a default-constructed |HandleSignalsState|, i.e., no signals
  // satisfied or satisfiable.) Note: The state is subject to change from other
  // threads.
  HandleSignalsState GetHandleSignalsState() const;

  // Adds an awakable to this dispatcher, which will be woken up when this
  // object changes state to satisfy |signals| with context |context|. It will
  // also be woken up when it becomes impossible for the object to ever satisfy
  // |signals| with a suitable error status.
  //
  // If |signals_state| is non-null, on *failure* |*signals_state| will be set
  // to the current handle signals state (on success, it is left untouched).
  //
  // Returns:
  //  - |MOJO_RESULT_OK| if the awakable was added;
  //  - |MOJO_RESULT_ALREADY_EXISTS| if |signals| is already satisfied;
  //  - |MOJO_RESULT_INVALID_ARGUMENT| if the dispatcher has been closed; and
  //  - |MOJO_RESULT_FAILED_PRECONDITION| if it is not (or no longer) possible
  //    that |signals| will ever be satisfied.
  MojoResult AddAwakable(Awakable* awakable,
                         MojoHandleSignals signals,
                         uint32_t context,
                         HandleSignalsState* signals_state);
  // Removes an awakable from this dispatcher. (It is valid to call this
  // multiple times for the same |awakable| on the same object, so long as
  // |AddAwakable()| was called at most once.) If |signals_state| is non-null,
  // |*signals_state| will be set to the current handle signals state.
  void RemoveAwakable(Awakable* awakable, HandleSignalsState* signals_state);

  // A dispatcher must be put into a special state in order to be sent across a
  // message pipe. Outside of tests, only |HandleTableAccess| is allowed to do
  // this, since there are requirements on the handle table (see below).
  //
  // In this special state, only a restricted set of operations is allowed.
  // These are the ones available as |DispatcherTransport| methods. Other
  // |Dispatcher| methods must not be called until |DispatcherTransport::End()|
  // has been called.
  class HandleTableAccess {
   private:
    friend class Core;
    friend class HandleTable;
    // Tests also need this, to avoid needing |Core|.
    friend DispatcherTransport test::DispatcherTryStartTransport(Dispatcher*);

    // This must be called under the handle table lock and only if the handle
    // table entry is not marked busy. The caller must maintain a reference to
    // |dispatcher| until |DispatcherTransport::End()| is called.
    static DispatcherTransport TryStartTransport(Dispatcher* dispatcher);
  };

  // A |TransportData| may serialize dispatchers that are given to it (and which
  // were previously attached to the |MessageInTransit| that is creating it) to
  // a given |Channel| and then (probably in a different process) deserialize.
  // Note that the |MessageInTransit| "owns" (i.e., has the only ref to) these
  // dispatchers, so there are no locking issues. (There's no lock ordering
  // issue, and in fact no need to take dispatcher locks at all.)
  // TODO(vtl): Consider making another wrapper similar to |DispatcherTransport|
  // (but with an owning, unique reference), and having
  // |CreateEquivalentDispatcherAndCloseImplNoLock()| return that wrapper (and
  // |MessageInTransit|, etc. only holding on to such wrappers).
  class TransportDataAccess {
   private:
    friend class TransportData;

    // Serialization API. These functions may only be called on such
    // dispatchers. (|channel| is the |Channel| to which the dispatcher is to be
    // serialized.) See the |Dispatcher| methods of the same names for more
    // details.
    static void StartSerialize(Dispatcher* dispatcher,
                               Channel* channel,
                               size_t* max_size,
                               size_t* max_platform_handles);
    static bool EndSerializeAndClose(
        Dispatcher* dispatcher,
        Channel* channel,
        void* destination,
        size_t* actual_size,
        embedder::PlatformHandleVector* platform_handles);

    // Deserialization API.
    // Note: This "clears" (i.e., reset to the invalid handle) any platform
    // handles that it takes ownership of.
    static scoped_refptr<Dispatcher> Deserialize(
        Channel* channel,
        int32_t type,
        const void* source,
        size_t size,
        embedder::PlatformHandleVector* platform_handles);
  };

 protected:
  friend class base::RefCountedThreadSafe<Dispatcher>;

  Dispatcher();
  virtual ~Dispatcher();

  // These are to be overridden by subclasses (if necessary). They are called
  // exactly once -- first |CancelAllAwakablesNoLock()|, then
  // |CloseImplNoLock()|,
  // when the dispatcher is being closed. They are called under |lock_|.
  virtual void CancelAllAwakablesNoLock();
  virtual void CloseImplNoLock();
  virtual scoped_refptr<Dispatcher>
  CreateEquivalentDispatcherAndCloseImplNoLock() = 0;

  // These are to be overridden by subclasses (if necessary). They are never
  // called after the dispatcher has been closed. They are called under |lock_|.
  // See the descriptions of the methods without the "ImplNoLock" for more
  // information.
  virtual MojoResult WriteMessageImplNoLock(
      UserPointer<const void> bytes,
      uint32_t num_bytes,
      std::vector<DispatcherTransport>* transports,
      MojoWriteMessageFlags flags);
  virtual MojoResult ReadMessageImplNoLock(UserPointer<void> bytes,
                                           UserPointer<uint32_t> num_bytes,
                                           DispatcherVector* dispatchers,
                                           uint32_t* num_dispatchers,
                                           MojoReadMessageFlags flags);
  virtual MojoResult WriteDataImplNoLock(UserPointer<const void> elements,
                                         UserPointer<uint32_t> num_bytes,
                                         MojoWriteDataFlags flags);
  virtual MojoResult BeginWriteDataImplNoLock(
      UserPointer<void*> buffer,
      UserPointer<uint32_t> buffer_num_bytes,
      MojoWriteDataFlags flags);
  virtual MojoResult EndWriteDataImplNoLock(uint32_t num_bytes_written);
  virtual MojoResult ReadDataImplNoLock(UserPointer<void> elements,
                                        UserPointer<uint32_t> num_bytes,
                                        MojoReadDataFlags flags);
  virtual MojoResult BeginReadDataImplNoLock(
      UserPointer<const void*> buffer,
      UserPointer<uint32_t> buffer_num_bytes,
      MojoReadDataFlags flags);
  virtual MojoResult EndReadDataImplNoLock(uint32_t num_bytes_read);
  virtual MojoResult DuplicateBufferHandleImplNoLock(
      UserPointer<const MojoDuplicateBufferHandleOptions> options,
      scoped_refptr<Dispatcher>* new_dispatcher);
  virtual MojoResult MapBufferImplNoLock(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      scoped_ptr<embedder::PlatformSharedBufferMapping>* mapping);
  virtual HandleSignalsState GetHandleSignalsStateImplNoLock() const;
  virtual MojoResult AddAwakableImplNoLock(Awakable* awakable,
                                           MojoHandleSignals signals,
                                           uint32_t context,
                                           HandleSignalsState* signals_state);
  virtual void RemoveAwakableImplNoLock(Awakable* awakable,
                                        HandleSignalsState* signals_state);

  // These implement the API used to serialize dispatchers to a |Channel|
  // (described below). They will only be called on a dispatcher that's attached
  // to and "owned" by a |MessageInTransit|. See the non-"impl" versions for
  // more information.
  //
  // Note: |StartSerializeImplNoLock()| is actually called with |lock_| NOT
  // held, since the dispatcher should only be accessible to the calling thread.
  // On Debug builds, |EndSerializeAndCloseImplNoLock()| is called with |lock_|
  // held, to satisfy any |lock_.AssertAcquired()| (e.g., in |CloseImplNoLock()|
  // -- and anything it calls); disentangling those assertions is
  // difficult/fragile, and would weaken our general checking of invariants.
  //
  // TODO(vtl): Consider making these pure virtual once most things support
  // being passed over a message pipe.
  virtual void StartSerializeImplNoLock(Channel* channel,
                                        size_t* max_size,
                                        size_t* max_platform_handles);
  virtual bool EndSerializeAndCloseImplNoLock(
      Channel* channel,
      void* destination,
      size_t* actual_size,
      embedder::PlatformHandleVector* platform_handles);

  // This should be overridden to return true if/when there's an ongoing
  // operation (e.g., two-phase read/writes on data pipes) that should prevent a
  // handle from being sent over a message pipe (with status "busy").
  virtual bool IsBusyNoLock() const;

  // Available to subclasses. (Note: Returns a non-const reference, just like
  // |base::AutoLock|'s constructor takes a non-const reference.)
  base::Lock& lock() const { return lock_; }

 private:
  friend class DispatcherTransport;

  // Closes the dispatcher. This must be done under lock, and unlike |Close()|,
  // the dispatcher must not be closed already. (This is the "equivalent" of
  // |CreateEquivalentDispatcherAndCloseNoLock()|, for situations where the
  // dispatcher must be disposed of instead of "transferred".)
  void CloseNoLock();

  // Creates an equivalent dispatcher -- representing the same resource as this
  // dispatcher -- and close (i.e., disable) this dispatcher. I.e., this
  // dispatcher will look as though it was closed, but the resource it
  // represents will be assigned to the new dispatcher. This must be called
  // under the dispatcher's lock.
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseNoLock();

  // API to serialize dispatchers to a |Channel|, exposed to only
  // |TransportData| (via |TransportData|). They may only be called on a
  // dispatcher attached to a |MessageInTransit| (and in particular not in
  // |CoreImpl|'s handle table).
  //
  // Starts the serialization. Returns (via the two "out" parameters) the
  // maximum amount of space that may be needed to serialize this dispatcher to
  // the given |Channel| (no more than
  // |TransportData::kMaxSerializedDispatcherSize|) and the maximum number of
  // |PlatformHandle|s that may need to be attached (no more than
  // |TransportData::kMaxSerializedDispatcherPlatformHandles|). If this
  // dispatcher cannot be serialized to the given |Channel|, |*max_size| and
  // |*max_platform_handles| should be set to zero. A call to this method will
  // ALWAYS be followed by a call to |EndSerializeAndClose()| (even if this
  // dispatcher cannot be serialized to the given |Channel|).
  void StartSerialize(Channel* channel,
                      size_t* max_size,
                      size_t* max_platform_handles);
  // Completes the serialization of this dispatcher to the given |Channel| and
  // closes it. (This call will always follow an earlier call to
  // |StartSerialize()|, with the same |Channel|.) This does so by writing to
  // |destination| and appending any |PlatformHandle|s needed to
  // |platform_handles| (which may be null if no platform handles were indicated
  // to be required to |StartSerialize()|). This may write no more than the
  // amount indicated by |StartSerialize()|. (WARNING: Beware of races, e.g., if
  // something can be mutated between the two calls!) Returns true on success,
  // in which case |*actual_size| is set to the amount it actually wrote to
  // |destination|. On failure, |*actual_size| should not be modified; however,
  // the dispatcher will still be closed.
  bool EndSerializeAndClose(Channel* channel,
                            void* destination,
                            size_t* actual_size,
                            embedder::PlatformHandleVector* platform_handles);

  // This protects the following members as well as any state added by
  // subclasses.
  mutable base::Lock lock_;
  bool is_closed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

// Wrapper around a |Dispatcher| pointer, while it's being processed to be
// passed in a message pipe. See the comment about
// |Dispatcher::HandleTableAccess| for more details.
//
// Note: This class is deliberately "thin" -- no more expensive than a
// |Dispatcher*|.
class MOJO_SYSTEM_IMPL_EXPORT DispatcherTransport {
 public:
  DispatcherTransport() : dispatcher_(nullptr) {}

  void End();

  Dispatcher::Type GetType() const { return dispatcher_->GetType(); }
  bool IsBusy() const { return dispatcher_->IsBusyNoLock(); }
  void Close() { dispatcher_->CloseNoLock(); }
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndClose() {
    return dispatcher_->CreateEquivalentDispatcherAndCloseNoLock();
  }

  bool is_valid() const { return !!dispatcher_; }

 protected:
  Dispatcher* dispatcher() { return dispatcher_; }

 private:
  friend class Dispatcher::HandleTableAccess;

  explicit DispatcherTransport(Dispatcher* dispatcher)
      : dispatcher_(dispatcher) {}

  Dispatcher* dispatcher_;

  // Copy and assign allowed.
};

// So logging macros and |DCHECK_EQ()|, etc. work.
MOJO_SYSTEM_IMPL_EXPORT inline std::ostream& operator<<(std::ostream& out,
                                                        Dispatcher::Type type) {
  return out << static_cast<int>(type);
}

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DISPATCHER_H_
