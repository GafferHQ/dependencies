// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_CORE_H_
#define MOJO_EDK_SYSTEM_CORE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/system/handle_table.h"
#include "mojo/edk/system/mapping_table.h"
#include "mojo/edk/system/memory.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

namespace embedder {
class PlatformSupport;
}

namespace system {

class Dispatcher;
struct HandleSignalsState;

// |Core| is an object that implements the Mojo system calls. All public methods
// are thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT Core {
 public:
  // ---------------------------------------------------------------------------

  // These methods are only to be used by via the embedder API (and internally):

  // |*platform_support| must outlive this object.
  explicit Core(embedder::PlatformSupport* platform_support);
  virtual ~Core();

  // Adds |dispatcher| to the handle table, returning the handle for it. Returns
  // |MOJO_HANDLE_INVALID| on failure, namely if the handle table is full.
  MojoHandle AddDispatcher(const scoped_refptr<Dispatcher>& dispatcher);

  // Looks up the dispatcher for the given handle. Returns null if the handle is
  // invalid.
  scoped_refptr<Dispatcher> GetDispatcher(MojoHandle handle);

  // Like |GetDispatcher()|, but also removes the handle from the handle table.
  // On success, gets the dispatcher for a given handle (which should not be
  // |MOJO_HANDLE_INVALID|) and removes it. (On failure, returns an appropriate
  // result (and leaves |dispatcher| alone), namely
  // |MOJO_RESULT_INVALID_ARGUMENT| if there's no dispatcher for the given
  // handle or |MOJO_RESULT_BUSY| if the handle is marked as busy.)
  MojoResult GetAndRemoveDispatcher(MojoHandle handle,
                                    scoped_refptr<Dispatcher>* dispatcher);

  // Watches on the given handle for the given signals, calling |callback| when
  // a signal is satisfied or when all signals become unsatisfiable. |callback|
  // must satisfy stringent requirements -- see |Awakable::Awake()| in
  // awakable.h. In particular, it must not call any Mojo system functions.
  MojoResult AsyncWait(MojoHandle handle,
                       MojoHandleSignals signals,
                       const base::Callback<void(MojoResult)>& callback);

  embedder::PlatformSupport* platform_support() const {
    return platform_support_;
  }

  // ---------------------------------------------------------------------------

  // The following methods are essentially implementations of the Mojo Core
  // functions of the Mojo API, with the C interface translated to C++ by
  // "mojo/edk/embedder/entrypoints.cc". The best way to understand the contract
  // of these methods is to look at the header files defining the corresponding
  // API functions, referenced below.

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/functions.h":
  MojoTimeTicks GetTimeTicksNow();
  MojoResult Close(MojoHandle handle);
  MojoResult Wait(MojoHandle handle,
                  MojoHandleSignals signals,
                  MojoDeadline deadline,
                  UserPointer<MojoHandleSignalsState> signals_state);
  MojoResult WaitMany(UserPointer<const MojoHandle> handles,
                      UserPointer<const MojoHandleSignals> signals,
                      uint32_t num_handles,
                      MojoDeadline deadline,
                      UserPointer<uint32_t> result_index,
                      UserPointer<MojoHandleSignalsState> signals_states);

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/message_pipe.h":
  MojoResult CreateMessagePipe(
      UserPointer<const MojoCreateMessagePipeOptions> options,
      UserPointer<MojoHandle> message_pipe_handle0,
      UserPointer<MojoHandle> message_pipe_handle1);
  MojoResult WriteMessage(MojoHandle message_pipe_handle,
                          UserPointer<const void> bytes,
                          uint32_t num_bytes,
                          UserPointer<const MojoHandle> handles,
                          uint32_t num_handles,
                          MojoWriteMessageFlags flags);
  MojoResult ReadMessage(MojoHandle message_pipe_handle,
                         UserPointer<void> bytes,
                         UserPointer<uint32_t> num_bytes,
                         UserPointer<MojoHandle> handles,
                         UserPointer<uint32_t> num_handles,
                         MojoReadMessageFlags flags);

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/data_pipe.h":
  MojoResult CreateDataPipe(
      UserPointer<const MojoCreateDataPipeOptions> options,
      UserPointer<MojoHandle> data_pipe_producer_handle,
      UserPointer<MojoHandle> data_pipe_consumer_handle);
  MojoResult WriteData(MojoHandle data_pipe_producer_handle,
                       UserPointer<const void> elements,
                       UserPointer<uint32_t> num_bytes,
                       MojoWriteDataFlags flags);
  MojoResult BeginWriteData(MojoHandle data_pipe_producer_handle,
                            UserPointer<void*> buffer,
                            UserPointer<uint32_t> buffer_num_bytes,
                            MojoWriteDataFlags flags);
  MojoResult EndWriteData(MojoHandle data_pipe_producer_handle,
                          uint32_t num_bytes_written);
  MojoResult ReadData(MojoHandle data_pipe_consumer_handle,
                      UserPointer<void> elements,
                      UserPointer<uint32_t> num_bytes,
                      MojoReadDataFlags flags);
  MojoResult BeginReadData(MojoHandle data_pipe_consumer_handle,
                           UserPointer<const void*> buffer,
                           UserPointer<uint32_t> buffer_num_bytes,
                           MojoReadDataFlags flags);
  MojoResult EndReadData(MojoHandle data_pipe_consumer_handle,
                         uint32_t num_bytes_read);

  // These methods correspond to the API functions defined in
  // "mojo/public/c/system/buffer.h":
  MojoResult CreateSharedBuffer(
      UserPointer<const MojoCreateSharedBufferOptions> options,
      uint64_t num_bytes,
      UserPointer<MojoHandle> shared_buffer_handle);
  MojoResult DuplicateBufferHandle(
      MojoHandle buffer_handle,
      UserPointer<const MojoDuplicateBufferHandleOptions> options,
      UserPointer<MojoHandle> new_buffer_handle);
  MojoResult MapBuffer(MojoHandle buffer_handle,
                       uint64_t offset,
                       uint64_t num_bytes,
                       UserPointer<void*> buffer,
                       MojoMapBufferFlags flags);
  MojoResult UnmapBuffer(UserPointer<void> buffer);

 private:
  friend bool internal::ShutdownCheckNoLeaks(Core*);

  // Internal implementation of |Wait()| and |WaitMany()|; doesn't do basic
  // validation of arguments. |*result_index| is only set if the result (whether
  // success or failure) applies to a specific handle, so its value should be
  // preinitialized to |static_cast<uint32_t>(-1)|.
  MojoResult WaitManyInternal(const MojoHandle* handles,
                              const MojoHandleSignals* signals,
                              uint32_t num_handles,
                              MojoDeadline deadline,
                              uint32_t* result_index,
                              HandleSignalsState* signals_states);

  embedder::PlatformSupport* const platform_support_;

  // TODO(vtl): |handle_table_lock_| should be a reader-writer lock (if only we
  // had them).
  base::Lock handle_table_lock_;  // Protects |handle_table_|.
  HandleTable handle_table_;

  base::Lock mapping_table_lock_;  // Protects |mapping_table_|.
  MappingTable mapping_table_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Core);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_CORE_H_
