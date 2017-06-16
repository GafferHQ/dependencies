// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CORE_DEVICE_CLIENT_H_
#define DEVICE_CORE_DEVICE_CLIENT_H_

#include "base/macros.h"

namespace device {

class HidService;
class UsbService;

namespace usb {
class DeviceManager;
}

// Interface used by consumers of //device APIs to get pointers to the service
// singletons appropriate for a given embedding application. For an example see
// //chrome/browser/chrome_device_client.h.
class DeviceClient {
 public:
  // Construction sets the single instance.
  DeviceClient();

  // Destruction clears the single instance.
  virtual ~DeviceClient();

  // Returns the single instance of |this|.
  static DeviceClient* Get();

  // Returns the UsbService instance for this embedder.
  virtual UsbService* GetUsbService();

  // Returns the HidService instance for this embedder.
  virtual HidService* GetHidService();

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceClient);
};

}  // namespace device

#endif  // DEVICE_CORE_DEVICE_CLIENT_H_
