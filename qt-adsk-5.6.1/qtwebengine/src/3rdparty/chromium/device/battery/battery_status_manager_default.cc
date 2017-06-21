// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/battery/battery_status_manager.h"

#include "base/logging.h"

namespace device {

namespace {

class BatteryStatusManagerDefault : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerDefault(
      const BatteryStatusService::BatteryUpdateCallback& callback) {}
  ~BatteryStatusManagerDefault() override {}

 private:
  // BatteryStatusManager:
  bool StartListeningBatteryChange() override {
    NOTIMPLEMENTED();
    return false;
  }

  void StopListeningBatteryChange() override { NOTIMPLEMENTED(); }

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerDefault);
};

}  // namespace

// static
scoped_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return scoped_ptr<BatteryStatusManager>(
      new BatteryStatusManagerDefault(callback));
}

}  // namespace device
