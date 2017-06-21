// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_DEVICES_DEVICES_APP_H_
#define DEVICE_DEVICES_DEVICES_APP_H_

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace base {
class SequencedTaskRunner;
}

namespace mojo {
class ApplicationImpl;
}

namespace device {

namespace usb {
class DeviceManager;
}

class DevicesApp : public mojo::ApplicationDelegate,
                   public mojo::InterfaceFactory<usb::DeviceManager>,
                   public mojo::ErrorHandler {
 public:
  explicit DevicesApp(
      scoped_refptr<base::SequencedTaskRunner> service_task_runner);
  ~DevicesApp() override;

 private:
  class USBServiceInitializer;

  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;
  void Quit() override;

  // mojo::InterfaceFactory<usb::DeviceManager>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<usb::DeviceManager> request) override;

  // mojo::ErrorHandler:
  void OnConnectionError() override;

  // Sets the app for destruction after a period of idle time. If any top-level
  // services (e.g. usb::DeviceManager) are bound before the timeout elapses,
  // it's canceled.
  void StartIdleTimer();

  mojo::ApplicationImpl* app_impl_;
  scoped_ptr<USBServiceInitializer> service_initializer_;
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  size_t active_device_manager_count_;

  // Callback used to shut down the app after a period of inactivity.
  base::CancelableClosure idle_timeout_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevicesApp);
};

}  // naespace device

#endif  // DEVICE_DEVICES_DEVICES_APP_H_
