// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/battery_status_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "device/battery/battery_monitor_impl.h"
#include "device/battery/battery_status_manager.h"

namespace device {

BatteryStatusService::BatteryStatusService()
    : main_thread_task_runner_(base::MessageLoop::current()->task_runner()),
      update_callback_(base::Bind(&BatteryStatusService::NotifyConsumers,
                                  base::Unretained(this))),
      status_updated_(false),
      is_shutdown_(false) {
  callback_list_.set_removal_callback(
      base::Bind(&BatteryStatusService::ConsumersChanged,
                 base::Unretained(this)));
}

BatteryStatusService::~BatteryStatusService() {
}

BatteryStatusService* BatteryStatusService::GetInstance() {
  return Singleton<BatteryStatusService,
                   LeakySingletonTraits<BatteryStatusService> >::get();
}

scoped_ptr<BatteryStatusService::BatteryUpdateSubscription>
BatteryStatusService::AddCallback(const BatteryUpdateCallback& callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_shutdown_);

  if (!battery_fetcher_)
    battery_fetcher_ = BatteryStatusManager::Create(update_callback_);

  if (callback_list_.empty()) {
    bool success = battery_fetcher_->StartListeningBatteryChange();
    // On failure pass the default values back.
    if (!success)
      callback.Run(BatteryStatus());
  }

  if (status_updated_) {
    // Send recent status to the new callback if already available.
    callback.Run(status_);
  }

  return callback_list_.Add(callback);
}

void BatteryStatusService::ConsumersChanged() {
  if (is_shutdown_)
    return;

  if (callback_list_.empty()) {
    battery_fetcher_->StopListeningBatteryChange();
    status_updated_ = false;
  }
}

void BatteryStatusService::NotifyConsumers(const BatteryStatus& status) {
  DCHECK(!is_shutdown_);

  main_thread_task_runner_->PostTask(FROM_HERE, base::Bind(
      &BatteryStatusService::NotifyConsumersOnMainThread,
      base::Unretained(this),
      status));
}

void BatteryStatusService::NotifyConsumersOnMainThread(
    const BatteryStatus& status) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  if (callback_list_.empty())
    return;

  status_ = status;
  status_updated_ = true;
  callback_list_.Notify(status_);
}

void BatteryStatusService::Shutdown() {
  if (!callback_list_.empty())
    battery_fetcher_->StopListeningBatteryChange();
  battery_fetcher_.reset();
  is_shutdown_ = true;
}

const BatteryStatusService::BatteryUpdateCallback&
BatteryStatusService::GetUpdateCallbackForTesting() const {
  return update_callback_;
}

void BatteryStatusService::SetBatteryManagerForTesting(
    scoped_ptr<BatteryStatusManager> test_battery_manager) {
  battery_fetcher_ = test_battery_manager.Pass();
  status_ = BatteryStatus();
  status_updated_ = false;
}

}  // namespace device
