// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "device/devices_app/devices_app.h"
#include "device/devices_app/usb/public/interfaces/device_manager.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"

namespace device {
namespace {

class DevicesAppTest : public mojo::test::ApplicationTestBase {
 public:
  DevicesAppTest() {}
  ~DevicesAppTest() override {}

  void SetUp() override {
    ApplicationTestBase::SetUp();
    mojo::URLRequestPtr request = mojo::URLRequest::New();
    request->url = "mojo:devices";
    application_impl()->ConnectToService(request.Pass(), &usb_device_manager_);
  }

  usb::DeviceManager* usb_device_manager() { return usb_device_manager_.get(); }

 private:
  usb::DeviceManagerPtr usb_device_manager_;

  DISALLOW_COPY_AND_ASSIGN(DevicesAppTest);
};

void OnGetDevices(const base::Closure& continuation,
                  mojo::Array<usb::DeviceInfoPtr> devices) {
  continuation.Run();
}

}  // namespace

// Simple test to verify that we can connect to the USB DeviceManager and get
// a response.
TEST_F(DevicesAppTest, GetUSBDevices) {
  base::RunLoop loop;
  usb::EnumerationOptionsPtr options = usb::EnumerationOptions::New();
  options->filters = mojo::Array<usb::DeviceFilterPtr>(1);
  options->filters[0] = usb::DeviceFilter::New();
  usb_device_manager()->GetDevices(
      options.Pass(), base::Bind(&OnGetDevices, loop.QuitClosure()));
  loop.Run();
}

}  // namespace device
