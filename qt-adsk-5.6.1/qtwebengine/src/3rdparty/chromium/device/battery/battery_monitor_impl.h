// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
#define DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "device/battery/battery_export.h"
#include "device/battery/battery_monitor.mojom.h"
#include "device/battery/battery_status_service.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace device {

class BatteryMonitorImpl : public BatteryMonitor {
 public:
  DEVICE_BATTERY_EXPORT static void Create(
      mojo::InterfaceRequest<BatteryMonitor> request);

 private:
  typedef mojo::Callback<void(BatteryStatusPtr)> BatteryStatusCallback;

  explicit BatteryMonitorImpl(mojo::InterfaceRequest<BatteryMonitor> request);
  ~BatteryMonitorImpl() override;

  // BatteryMonitor methods:
  void QueryNextStatus(const BatteryStatusCallback& callback) override;

  void RegisterSubscription();
  void DidChange(const BatteryStatus& battery_status);
  void ReportStatus();

  mojo::StrongBinding<BatteryMonitor> binding_;
  scoped_ptr<BatteryStatusService::BatteryUpdateSubscription> subscription_;
  BatteryStatusCallback callback_;
  BatteryStatus status_;
  bool status_to_report_;

  DISALLOW_COPY_AND_ASSIGN(BatteryMonitorImpl);
};

}  // namespace device

#endif  // DEVICE_BATTERY_BATTERY_MONITOR_IMPL_H_
