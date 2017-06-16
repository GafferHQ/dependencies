// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_PROCESS_H_
#define CONTENT_CHILD_CHILD_PROCESS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {
class ChildThreadImpl;

// Base class for child processes of the browser process (i.e. renderer and
// plugin host). This is a singleton object for each child process.
//
// During process shutdown the following sequence of actions happens in
// order.
//
// 1. ChildProcess::~ChildProcess() is called.
//   2. Shutdown event is fired. Background threads should stop.
//   3. ChildThreadImpl::Shutdown() is called. ChildThread is also deleted.
//   4. IO thread is stopped.
// 5. Main message loop exits.
// 6. Child process is now fully stopped.
//
// Note: IO thread outlives the ChildThreadImpl object.
class CONTENT_EXPORT ChildProcess {
 public:
  // Child processes should have an object that derives from this class.
  // Normally you would immediately call set_main_thread after construction.
  ChildProcess();
  virtual ~ChildProcess();

  // May be NULL if the main thread hasn't been set explicitly.
  ChildThreadImpl* main_thread();

  // Sets the object associated with the main thread of this process.
  // Takes ownership of the pointer.
  void set_main_thread(ChildThreadImpl* thread);

  base::MessageLoop* io_message_loop() { return io_thread_.message_loop(); }
  base::SingleThreadTaskRunner* io_task_runner() {
    return io_thread_.task_runner().get();
  }

  // A global event object that is signalled when the main thread's message
  // loop exits.  This gives background threads a way to observe the main
  // thread shutting down.  This can be useful when a background thread is
  // waiting for some information from the browser process.  If the browser
  // process goes away prematurely, the background thread can at least notice
  // the child processes's main thread exiting to determine that it should give
  // up waiting.
  // For example, see the renderer code used to implement GetCookies().
  base::WaitableEvent* GetShutDownEvent();

  // These are used for ref-counting the child process.  The process shuts
  // itself down when the ref count reaches 0.
  // For example, in the renderer process, generally each tab managed by this
  // process will hold a reference to the process, and release when closed.
  void AddRefProcess();
  void ReleaseProcess();

  // Getter for the one ChildProcess object for this process. Can only be called
  // on the main thread.
  static ChildProcess* current();

  static void WaitForDebugger(const std::string& label);
 private:
  int ref_count_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // The thread that handles IO events.
  base::Thread io_thread_;

  // NOTE: make sure that main_thread_ is listed after shutdown_event_, since
  // it depends on it (indirectly through IPC::SyncChannel).  Same for
  // io_thread_.
  scoped_ptr<ChildThreadImpl> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcess);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_PROCESS_H_
