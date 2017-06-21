// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_SERVICE_WIN_H_
#define DEVICE_HID_HID_SERVICE_WIN_H_

#include <windows.h>
#include <hidclass.h>

extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/win/scoped_handle.h"
#include "device/core/device_monitor_win.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"

namespace base {
namespace win {
class MessageWindow;
}
}

namespace device {

class HidServiceWin : public HidService, public DeviceMonitorWin::Observer {
 public:
  HidServiceWin(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

  void Connect(const HidDeviceId& device_id,
               const ConnectCallback& callback) override;

 private:
  ~HidServiceWin() override;

  static void EnumerateOnFileThread(
      base::WeakPtr<HidServiceWin> service,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  static void CollectInfoFromButtonCaps(PHIDP_PREPARSED_DATA preparsed_data,
                                        HIDP_REPORT_TYPE report_type,
                                        USHORT button_caps_length,
                                        HidCollectionInfo* collection_info);
  static void CollectInfoFromValueCaps(PHIDP_PREPARSED_DATA preparsed_data,
                                       HIDP_REPORT_TYPE report_type,
                                       USHORT value_caps_length,
                                       HidCollectionInfo* collection_info);
  static void AddDeviceOnFileThread(
      base::WeakPtr<HidServiceWin> service,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const std::string& device_path);

  // DeviceMonitorWin::Observer implementation:
  void OnDeviceAdded(const GUID& class_guid,
                     const std::string& device_path) override;
  void OnDeviceRemoved(const GUID& class_guid,
                       const std::string& device_path) override;

  // Tries to open the device read-write and falls back to read-only.
  static base::win::ScopedHandle OpenDevice(const std::string& device_path);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_;
  base::WeakPtrFactory<HidServiceWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceWin);
};

}  // namespace device

#endif  // DEVICE_HID_HID_SERVICE_WIN_H_
