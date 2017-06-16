// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_MOCK_USB_DEVICE_H_
#define DEVICE_USB_MOCK_USB_DEVICE_H_

#include "device/usb/usb_device.h"

#include <string>

#include "device/usb/usb_device_handle.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice(uint16 vendor_id, uint16 product_id);
  MockUsbDevice(uint16 vendor_id,
                uint16 product_id,
                const std::string& manufacturer_string,
                const std::string& product_string,
                const std::string& serial_number);

  MOCK_METHOD1(Open, void(const OpenCallback&));
  MOCK_METHOD1(Close, bool(scoped_refptr<UsbDeviceHandle>));
  MOCK_METHOD0(GetConfiguration, const device::UsbConfigDescriptor*());

 private:
  ~MockUsbDevice() override;
};

}  // namespace device

#endif  // DEVICE_USB_MOCK_USB_DEVICE_H_
