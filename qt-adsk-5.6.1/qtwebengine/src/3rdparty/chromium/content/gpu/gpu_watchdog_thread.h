// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_WATCHDOG_THREAD_H_
#define CONTENT_GPU_GPU_WATCHDOG_THREAD_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/common/gpu/gpu_watchdog.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
extern "C" {
#include <X11/Xlib.h>
#include <X11/Xatom.h>
}
#include <sys/poll.h>
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_types.h"

#endif

namespace content {

// A thread that intermitently sends tasks to a group of watched message loops
// and deliberately crashes if one of them does not respond after a timeout.
class GpuWatchdogThread : public base::Thread,
                          public GpuWatchdog,
                          public base::PowerObserver,
                          public base::RefCountedThreadSafe<GpuWatchdogThread> {
 public:
  explicit GpuWatchdogThread(int timeout);

  // Accessible on watched thread but only modified by watchdog thread.
  bool armed() const { return armed_; }
  void PostAcknowledge();

  // Implement GpuWatchdog.
  void CheckArmed() override;

  // Must be called after a PowerMonitor has been created. Can be called from
  // any thread.
  void AddPowerObserver();

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  friend class base::RefCountedThreadSafe<GpuWatchdogThread>;

  // An object of this type intercepts the reception and completion of all tasks
  // on the watched thread and checks whether the watchdog is armed.
  class GpuWatchdogTaskObserver : public base::MessageLoop::TaskObserver {
   public:
    explicit GpuWatchdogTaskObserver(GpuWatchdogThread* watchdog);
    ~GpuWatchdogTaskObserver() override;

    // Implements MessageLoop::TaskObserver.
    void WillProcessTask(const base::PendingTask& pending_task) override;
    void DidProcessTask(const base::PendingTask& pending_task) override;

   private:
    GpuWatchdogThread* watchdog_;
  };

  ~GpuWatchdogThread() override;

  void OnAcknowledge();
  void OnCheck(bool after_suspend);
  void DeliberatelyTerminateToRecoverFromHang();
#if defined(USE_X11)
  void SetupXServer();
  void SetupXChangeProp();
  bool MatchXEventAtom(XEvent* event);
#endif

  void OnAddPowerObserver();

  // Implement PowerObserver.
  void OnSuspend() override;
  void OnResume() override;

#if defined(OS_WIN)
  base::TimeDelta GetWatchedThreadTime();
#endif

  base::MessageLoop* watched_message_loop_;
  base::TimeDelta timeout_;
  volatile bool armed_;
  GpuWatchdogTaskObserver task_observer_;

#if defined(OS_WIN)
  void* watched_thread_handle_;
  base::TimeDelta arm_cpu_time_;
#endif

  // Time after which it's assumed that the computer has been suspended since
  // the task was posted.
  base::Time suspension_timeout_;

  bool suspended_;

#if defined(OS_CHROMEOS)
  FILE* tty_file_;
#endif

#if defined(USE_X11)
  XDisplay* display_;
  gfx::AcceleratedWidget window_;
  XAtom atom_;
#endif

  base::WeakPtrFactory<GpuWatchdogThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuWatchdogThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_WATCHDOG_THREAD_H_
