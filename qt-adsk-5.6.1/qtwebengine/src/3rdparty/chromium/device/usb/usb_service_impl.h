// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_device_impl.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_WIN)
#include "base/scoped_observer.h"
#include "device/core/device_monitor_win.h"
#endif  // OS_WIN

struct libusb_device;
struct libusb_context;

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace device {

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_context* PlatformUsbContext;

class UsbServiceImpl : public UsbService,
#if defined(OS_WIN)
                       public DeviceMonitorWin::Observer,
#endif  // OS_WIN
                       public base::MessageLoop::DestructionObserver {
 public:
  static UsbService* Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

 private:
  explicit UsbServiceImpl(
      PlatformUsbContext context,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  ~UsbServiceImpl() override;

  // device::UsbService implementation
  scoped_refptr<UsbDevice> GetDevice(const std::string& guid) override;
  void GetDevices(const GetDevicesCallback& callback) override;

#if defined(OS_WIN)
  // device::DeviceMonitorWin::Observer implementation
  void OnDeviceAdded(const GUID& class_guid,
                     const std::string& device_path) override;
  void OnDeviceRemoved(const GUID& class_guid,
                       const std::string& device_path) override;
#endif  // OS_WIN

  // base::MessageLoop::DestructionObserver implementation
  void WillDestroyCurrentMessageLoop() override;

  // Enumerate USB devices from OS and update devices_ map.
  void RefreshDevices();
  void OnDeviceList(libusb_device** platform_devices, size_t device_count);
  void RefreshDevicesComplete();

  // Creates a new UsbDevice based on the given libusb device.
  void EnumerateDevice(PlatformUsbDevice platform_device,
                       const base::Closure& refresh_complete);

  void AddDevice(const base::Closure& refresh_complete,
                 scoped_refptr<UsbDeviceImpl> device);
  void RemoveDevice(scoped_refptr<UsbDeviceImpl> device);

  // Handle hotplug events from libusb.
  static int LIBUSB_CALL HotplugCallback(libusb_context* context,
                                         PlatformUsbDevice device,
                                         libusb_hotplug_event event,
                                         void* user_data);
  // These functions release a reference to the provided platform device.
  void OnPlatformDeviceAdded(PlatformUsbDevice platform_device);
  void OnPlatformDeviceRemoved(PlatformUsbDevice platform_device);

  scoped_refptr<UsbContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // When available the device list will be updated when new devices are
  // connected instead of only when a full enumeration is requested.
  // TODO(reillyg): Support this on all platforms. crbug.com/411715
  bool hotplug_enabled_ = false;
  libusb_hotplug_callback_handle hotplug_handle_;

  // Enumeration callbacks are queued until an enumeration completes.
  bool enumeration_ready_ = false;
  bool enumeration_in_progress_ = false;
  std::queue<std::string> pending_path_enumerations_;
  std::vector<GetDevicesCallback> pending_enumeration_callbacks_;

  // The map from unique IDs to UsbDevices.
  typedef std::map<std::string, scoped_refptr<UsbDeviceImpl>> DeviceMap;
  DeviceMap devices_;

  // The map from PlatformUsbDevices to UsbDevices.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDeviceImpl>>
      PlatformDeviceMap;
  PlatformDeviceMap platform_devices_;

  // Tracks PlatformUsbDevices that might be removed while they are being
  // enumerated.
  std::set<PlatformUsbDevice> devices_being_enumerated_;

#if defined(OS_WIN)
  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_;
#endif  // OS_WIN

  base::WeakPtrFactory<UsbServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbServiceImpl);
};

}  // namespace device
