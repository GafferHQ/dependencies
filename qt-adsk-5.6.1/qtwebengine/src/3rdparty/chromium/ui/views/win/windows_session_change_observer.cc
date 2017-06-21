// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/windows_session_change_observer.h"

#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/task_runner.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"
#include "ui/views/views_delegate.h"

namespace views {

class WindowsSessionChangeObserver::WtsRegistrationNotificationManager {
 public:
  static WtsRegistrationNotificationManager* GetInstance() {
    return Singleton<WtsRegistrationNotificationManager>::get();
  }

  WtsRegistrationNotificationManager() {
    AddSingletonHwndObserver();
  }

  ~WtsRegistrationNotificationManager() {
    RemoveSingletonHwndObserver();
  }

  void AddObserver(WindowsSessionChangeObserver* observer) {
    DCHECK(singleton_hwnd_observer_);
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(WindowsSessionChangeObserver* observer) {
    observer_list_.RemoveObserver(observer);
  }

 private:
  friend struct DefaultSingletonTraits<WtsRegistrationNotificationManager>;

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
      case WM_WTSSESSION_CHANGE:
        FOR_EACH_OBSERVER(WindowsSessionChangeObserver,
                          observer_list_,
                          OnSessionChange(wparam));
        break;
      case WM_DESTROY:
        RemoveSingletonHwndObserver();
        break;
    }
  }

  void AddSingletonHwndObserver() {
    DCHECK(!singleton_hwnd_observer_);
    singleton_hwnd_observer_.reset(new gfx::SingletonHwndObserver(
        base::Bind(&WtsRegistrationNotificationManager::OnWndProc,
                   base::Unretained(this))));
    scoped_refptr<base::TaskRunner> task_runner;
    if (ViewsDelegate::GetInstance()) {
      task_runner = ViewsDelegate::GetInstance()->GetBlockingPoolTaskRunner();
    }

    base::Closure wts_register =
        base::Bind(base::IgnoreResult(&WTSRegisterSessionNotification),
                   gfx::SingletonHwnd::GetInstance()->hwnd(),
                   NOTIFY_FOR_THIS_SESSION);
    if (task_runner) {
      task_runner->PostTask(FROM_HERE, wts_register);
    } else {
      wts_register.Run();
    }
  }

  void RemoveSingletonHwndObserver() {
    if (!singleton_hwnd_observer_)
      return;

    singleton_hwnd_observer_.reset(nullptr);
    // There is no race condition between this code and the worker thread.
    // RemoveSingletonHwndObserver is only called from two places:
    //   1) Destruction due to Singleton Destruction.
    //   2) WM_DESTROY fired by SingletonHwnd.
    // Under both cases we are in shutdown, which means no other worker threads
    // can be running.
    WTSUnRegisterSessionNotification(gfx::SingletonHwnd::GetInstance()->hwnd());
    FOR_EACH_OBSERVER(WindowsSessionChangeObserver,
                      observer_list_,
                      ClearCallback());
  }

  base::ObserverList<WindowsSessionChangeObserver, true> observer_list_;
  scoped_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  DISALLOW_COPY_AND_ASSIGN(WtsRegistrationNotificationManager);
};

WindowsSessionChangeObserver::WindowsSessionChangeObserver(
    const WtsCallback& callback)
    : callback_(callback) {
  DCHECK(!callback_.is_null());
  WtsRegistrationNotificationManager::GetInstance()->AddObserver(this);
}

WindowsSessionChangeObserver::~WindowsSessionChangeObserver() {
  ClearCallback();
}

void WindowsSessionChangeObserver::OnSessionChange(WPARAM wparam) {
  callback_.Run(wparam);
}

void WindowsSessionChangeObserver::ClearCallback() {
  if (!callback_.is_null()) {
    callback_.Reset();
    WtsRegistrationNotificationManager::GetInstance()->RemoveObserver(this);
  }
}

}  // namespace views
