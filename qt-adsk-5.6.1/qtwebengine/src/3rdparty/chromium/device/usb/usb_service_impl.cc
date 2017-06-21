// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service_impl.h"

#include <list>
#include <set>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_WIN)
#include <setupapi.h>
#include <usbiodef.h>

#include "base/strings/string_util.h"
#endif  // OS_WIN

#if defined(USE_UDEV)
#include "device/udev_linux/scoped_udev.h"
#endif  // USE_UDEV

namespace device {

namespace {

#if defined(OS_WIN)

// Wrapper around a HDEVINFO that automatically destroys it.
class ScopedDeviceInfoList {
 public:
  explicit ScopedDeviceInfoList(HDEVINFO handle) : handle_(handle) {}

  ~ScopedDeviceInfoList() {
    if (valid()) {
      SetupDiDestroyDeviceInfoList(handle_);
    }
  }

  bool valid() { return handle_ != INVALID_HANDLE_VALUE; }

  HDEVINFO get() { return handle_; }

 private:
  HDEVINFO handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDeviceInfoList);
};

// Wrapper around an SP_DEVINFO_DATA that initializes it properly and
// automatically deletes it.
class ScopedDeviceInfo {
 public:
  ScopedDeviceInfo() {
    memset(&dev_info_data_, 0, sizeof(dev_info_data_));
    dev_info_data_.cbSize = sizeof(dev_info_data_);
  }

  ~ScopedDeviceInfo() {
    if (dev_info_set_ != INVALID_HANDLE_VALUE) {
      SetupDiDeleteDeviceInfo(dev_info_set_, &dev_info_data_);
    }
  }

  // Once the SP_DEVINFO_DATA has been populated it must be freed using the
  // HDEVINFO it was created from.
  void set_valid(HDEVINFO dev_info_set) {
    DCHECK(dev_info_set_ == INVALID_HANDLE_VALUE);
    DCHECK(dev_info_set != INVALID_HANDLE_VALUE);
    dev_info_set_ = dev_info_set;
  }

  PSP_DEVINFO_DATA get() { return &dev_info_data_; }

 private:
  HDEVINFO dev_info_set_ = INVALID_HANDLE_VALUE;
  SP_DEVINFO_DATA dev_info_data_;
};

bool IsWinUsbInterface(const std::string& device_path) {
  ScopedDeviceInfoList dev_info_list(SetupDiCreateDeviceInfoList(NULL, NULL));
  if (!dev_info_list.valid()) {
    USB_PLOG(ERROR) << "Failed to create a device information set";
    return false;
  }

  // This will add the device to |dev_info_list| so we can query driver info.
  if (!SetupDiOpenDeviceInterfaceA(dev_info_list.get(), device_path.c_str(), 0,
                                   NULL)) {
    USB_PLOG(ERROR) << "Failed to get device interface data for "
                    << device_path;
    return false;
  }

  ScopedDeviceInfo dev_info;
  if (!SetupDiEnumDeviceInfo(dev_info_list.get(), 0, dev_info.get())) {
    USB_PLOG(ERROR) << "Failed to get device info for " << device_path;
    return false;
  }
  dev_info.set_valid(dev_info_list.get());

  DWORD reg_data_type;
  BYTE buffer[256];
  if (!SetupDiGetDeviceRegistryPropertyA(dev_info_list.get(), dev_info.get(),
                                         SPDRP_SERVICE, &reg_data_type,
                                         &buffer[0], sizeof buffer, NULL)) {
    USB_PLOG(ERROR) << "Failed to get device service property";
    return false;
  }
  if (reg_data_type != REG_SZ) {
    USB_LOG(ERROR) << "Unexpected data type for driver service: "
                   << reg_data_type;
    return false;
  }

  USB_LOG(DEBUG) << "Driver for " << device_path << " is " << buffer << ".";
  if (base::strncasecmp("WinUSB", (const char*)&buffer[0], sizeof "WinUSB") ==
      0) {
    return true;
  }
  return false;
}

#endif  // OS_WIN

void GetDeviceListOnBlockingThread(
    const std::string& new_device_path,
    scoped_refptr<UsbContext> usb_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Callback<void(libusb_device**, size_t)> callback) {
#if defined(OS_WIN)
  if (!new_device_path.empty()) {
    if (!IsWinUsbInterface(new_device_path)) {
      // Wait to call libusb_get_device_list until libusb will be able to find
      // a WinUSB interface for the device.
      task_runner->PostTask(FROM_HERE, base::Bind(callback, nullptr, 0));
      return;
    }
  }
#endif  // defined(OS_WIN)

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(usb_context->context(), &platform_devices);
  if (device_count < 0) {
    USB_LOG(ERROR) << "Failed to get device list: "
                   << ConvertPlatformUsbErrorToString(device_count);
    task_runner->PostTask(FROM_HERE, base::Bind(callback, nullptr, 0));
    return;
  }

  task_runner->PostTask(FROM_HERE,
                        base::Bind(callback, platform_devices, device_count));
}

#if defined(USE_UDEV)

void EnumerateUdevDevice(scoped_refptr<UsbDeviceImpl> device,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         const base::Closure& success_closure,
                         const base::Closure& failure_closure) {
  ScopedUdevPtr udev(udev_new());
  ScopedUdevEnumeratePtr udev_enumerate(udev_enumerate_new(udev.get()));

  udev_enumerate_add_match_subsystem(udev_enumerate.get(), "usb");
  if (udev_enumerate_scan_devices(udev_enumerate.get()) != 0) {
    task_runner->PostTask(FROM_HERE, failure_closure);
    return;
  }

  std::string bus_number =
      base::IntToString(libusb_get_bus_number(device->platform_device()));
  std::string device_address =
      base::IntToString(libusb_get_device_address(device->platform_device()));
  udev_list_entry* devices =
      udev_enumerate_get_list_entry(udev_enumerate.get());
  for (udev_list_entry* i = devices; i != NULL;
       i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr udev_device(
        udev_device_new_from_syspath(udev.get(), udev_list_entry_get_name(i)));
    if (udev_device) {
      const char* value =
          udev_device_get_sysattr_value(udev_device.get(), "busnum");
      if (!value || bus_number != value) {
        continue;
      }
      value = udev_device_get_sysattr_value(udev_device.get(), "devnum");
      if (!value || device_address != value) {
        continue;
      }

      value = udev_device_get_sysattr_value(udev_device.get(), "manufacturer");
      if (value) {
        device->set_manufacturer_string(base::UTF8ToUTF16(value));
      }
      value = udev_device_get_sysattr_value(udev_device.get(), "product");
      if (value) {
        device->set_product_string(base::UTF8ToUTF16(value));
      }
      value = udev_device_get_sysattr_value(udev_device.get(), "serial");
      if (value) {
        device->set_serial_number(base::UTF8ToUTF16(value));
      }

      value = udev_device_get_devnode(udev_device.get());
      if (value) {
        device->set_device_path(value);
        task_runner->PostTask(FROM_HERE, success_closure);
        return;
      }

      break;
    }
  }

  task_runner->PostTask(FROM_HERE, failure_closure);
}

#else

void OnReadStringDescriptor(
    const base::Callback<void(const base::string16&)>& callback,
    UsbTransferStatus status,
    scoped_refptr<net::IOBuffer> buffer,
    size_t length) {
  base::string16 string;
  if (status == USB_TRANSFER_COMPLETED &&
      ParseUsbStringDescriptor(
          std::vector<uint8>(buffer->data(), buffer->data() + length),
          &string)) {
    callback.Run(string);
  } else {
    callback.Run(base::string16());
  }
}

void ReadStringDescriptor(
    scoped_refptr<UsbDeviceHandle> device_handle,
    uint8 index,
    uint16 language_id,
    const base::Callback<void(const base::string16&)>& callback) {
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(256);
  device_handle->ControlTransfer(
      USB_DIRECTION_INBOUND, UsbDeviceHandle::STANDARD, UsbDeviceHandle::DEVICE,
      6 /* GET_DESCRIPTOR */, 3 /* STRING */ << 8 | index, language_id, buffer,
      256, 60, base::Bind(&OnReadStringDescriptor, callback));
}

void CloseHandleAndRunContinuation(scoped_refptr<UsbDeviceHandle> device_handle,
                                   const base::Closure& continuation) {
  device_handle->Close();
  continuation.Run();
}

void SaveStringAndRunContinuation(
    const base::Callback<void(const base::string16&)>& save_callback,
    const base::Closure& continuation,
    const base::string16& value) {
  if (!value.empty()) {
    save_callback.Run(value);
  }
  continuation.Run();
}

void OnReadLanguageIds(scoped_refptr<UsbDeviceHandle> device_handle,
                       uint8 manufacturer,
                       uint8 product,
                       uint8 serial_number,
                       const base::Closure& success_closure,
                       const base::string16& languages) {
  // Default to English unless the device provides a language and then just pick
  // the first one.
  uint16 language_id = 0x0409;
  if (!languages.empty()) {
    language_id = languages[0];
  }

  scoped_refptr<UsbDeviceImpl> device =
      static_cast<UsbDeviceImpl*>(device_handle->GetDevice().get());
  base::Closure continuation =
      base::BarrierClosure(3, base::Bind(&CloseHandleAndRunContinuation,
                                         device_handle, success_closure));

  if (manufacturer == 0) {
    continuation.Run();
  } else {
    ReadStringDescriptor(
        device_handle, manufacturer, language_id,
        base::Bind(&SaveStringAndRunContinuation,
                   base::Bind(&UsbDeviceImpl::set_manufacturer_string, device),
                   continuation));
  }

  if (product == 0) {
    continuation.Run();
  } else {
    ReadStringDescriptor(
        device_handle, product, language_id,
        base::Bind(&SaveStringAndRunContinuation,
                   base::Bind(&UsbDeviceImpl::set_product_string, device),
                   continuation));
  }

  if (serial_number == 0) {
    continuation.Run();
  } else {
    ReadStringDescriptor(
        device_handle, serial_number, language_id,
        base::Bind(&SaveStringAndRunContinuation,
                   base::Bind(&UsbDeviceImpl::set_serial_number, device),
                   continuation));
  }
}

void ReadDeviceLanguage(uint8 manufacturer,
                        uint8 product,
                        uint8 serial_number,
                        const base::Closure& success_closure,
                        const base::Closure& failure_closure,
                        scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle) {
    ReadStringDescriptor(
        device_handle, 0, 0,
        base::Bind(&OnReadLanguageIds, device_handle, manufacturer, product,
                   serial_number, success_closure));
  } else {
    failure_closure.Run();
  }
}

#endif  // USE_UDEV

}  // namespace

// static
UsbService* UsbServiceImpl::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  PlatformUsbContext context = NULL;
  const int rv = libusb_init(&context);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(ERROR) << "Failed to initialize libusb: "
                   << ConvertPlatformUsbErrorToString(rv);
    return nullptr;
  }
  if (!context) {
    return nullptr;
  }

  return new UsbServiceImpl(context, blocking_task_runner);
}

UsbServiceImpl::UsbServiceImpl(
    PlatformUsbContext context,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : context_(new UsbContext(context)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner),
#if defined(OS_WIN)
      device_observer_(this),
#endif
      weak_factory_(this) {
  base::MessageLoop::current()->AddDestructionObserver(this);

  int rv = libusb_hotplug_register_callback(
      context_->context(),
      static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      static_cast<libusb_hotplug_flag>(0), LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      &UsbServiceImpl::HotplugCallback, this, &hotplug_handle_);
  if (rv == LIBUSB_SUCCESS) {
    hotplug_enabled_ = true;
  }

  RefreshDevices();
#if defined(OS_WIN)
  DeviceMonitorWin* device_monitor = DeviceMonitorWin::GetForAllInterfaces();
  if (device_monitor) {
    device_observer_.Add(device_monitor);
  }
#endif  // OS_WIN
}

UsbServiceImpl::~UsbServiceImpl() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);

  if (hotplug_enabled_) {
    libusb_hotplug_deregister_callback(context_->context(), hotplug_handle_);
  }
  for (const auto& map_entry : devices_) {
    map_entry.second->OnDisconnect();
  }
}

scoped_refptr<UsbDevice> UsbServiceImpl::GetDevice(const std::string& guid) {
  DCHECK(CalledOnValidThread());
  DeviceMap::iterator it = devices_.find(guid);
  if (it != devices_.end()) {
    return it->second;
  }
  return NULL;
}

void UsbServiceImpl::GetDevices(const GetDevicesCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (hotplug_enabled_ && !enumeration_in_progress_) {
    // The device list is updated live when hotplug events are supported.
    std::vector<scoped_refptr<UsbDevice>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }
    callback.Run(devices);
  } else {
    pending_enumeration_callbacks_.push_back(callback);
    RefreshDevices();
  }
}

#if defined(OS_WIN)

void UsbServiceImpl::OnDeviceAdded(const GUID& class_guid,
                                   const std::string& device_path) {
  // Only the root node of a composite USB device has the class GUID
  // GUID_DEVINTERFACE_USB_DEVICE but we want to wait until WinUSB is loaded.
  // This first pass filter will catch anything that's sitting on the USB bus
  // (including devices on 3rd party USB controllers) to avoid the more
  // expensive driver check that needs to be done on the FILE thread.
  if (device_path.find("usb") != std::string::npos) {
    pending_path_enumerations_.push(device_path);
    RefreshDevices();
  }
}

void UsbServiceImpl::OnDeviceRemoved(const GUID& class_guid,
                                     const std::string& device_path) {
  // The root USB device node is removed last.
  if (class_guid == GUID_DEVINTERFACE_USB_DEVICE) {
    RefreshDevices();
  }
}

#endif  // OS_WIN

void UsbServiceImpl::WillDestroyCurrentMessageLoop() {
  DCHECK(CalledOnValidThread());
  delete this;
}

void UsbServiceImpl::RefreshDevices() {
  DCHECK(CalledOnValidThread());

  if (enumeration_in_progress_) {
    return;
  }

  enumeration_in_progress_ = true;
  DCHECK(devices_being_enumerated_.empty());

  std::string device_path;
  if (!pending_path_enumerations_.empty()) {
    device_path = pending_path_enumerations_.front();
    pending_path_enumerations_.pop();
  }

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GetDeviceListOnBlockingThread, device_path, context_,
                 task_runner_, base::Bind(&UsbServiceImpl::OnDeviceList,
                                          weak_factory_.GetWeakPtr())));
}

void UsbServiceImpl::OnDeviceList(libusb_device** platform_devices,
                                  size_t device_count) {
  DCHECK(CalledOnValidThread());
  if (!platform_devices) {
    RefreshDevicesComplete();
    return;
  }

  base::Closure refresh_complete =
      base::BarrierClosure(static_cast<int>(device_count),
                           base::Bind(&UsbServiceImpl::RefreshDevicesComplete,
                                      weak_factory_.GetWeakPtr()));
  std::list<PlatformUsbDevice> new_devices;

  // Look for new and existing devices.
  for (size_t i = 0; i < device_count; ++i) {
    PlatformUsbDevice platform_device = platform_devices[i];
    auto it = platform_devices_.find(platform_device);

    if (it == platform_devices_.end()) {
      libusb_ref_device(platform_device);
      new_devices.push_back(platform_device);
    } else {
      it->second->set_visited(true);
      refresh_complete.Run();
    }
  }

  // Remove devices not seen in this enumeration.
  for (PlatformDeviceMap::iterator it = platform_devices_.begin();
       it != platform_devices_.end();
       /* incremented internally */) {
    PlatformDeviceMap::iterator current = it++;
    const scoped_refptr<UsbDeviceImpl>& device = current->second;
    if (device->was_visited()) {
      device->set_visited(false);
    } else {
      RemoveDevice(device);
    }
  }

  for (PlatformUsbDevice platform_device : new_devices) {
    EnumerateDevice(platform_device, refresh_complete);
  }

  libusb_free_device_list(platform_devices, true);
}

void UsbServiceImpl::RefreshDevicesComplete() {
  DCHECK(CalledOnValidThread());
  DCHECK(enumeration_in_progress_);

  enumeration_ready_ = true;
  enumeration_in_progress_ = false;
  devices_being_enumerated_.clear();

  if (!pending_enumeration_callbacks_.empty()) {
    std::vector<scoped_refptr<UsbDevice>> devices;
    for (const auto& map_entry : devices_) {
      devices.push_back(map_entry.second);
    }

    std::vector<GetDevicesCallback> callbacks;
    callbacks.swap(pending_enumeration_callbacks_);
    for (const GetDevicesCallback& callback : callbacks) {
      callback.Run(devices);
    }
  }

  if (!pending_path_enumerations_.empty()) {
    RefreshDevices();
  }
}

void UsbServiceImpl::EnumerateDevice(PlatformUsbDevice platform_device,
                                     const base::Closure& refresh_complete) {
  devices_being_enumerated_.insert(platform_device);

  libusb_device_descriptor descriptor;
  int rv = libusb_get_device_descriptor(platform_device, &descriptor);
  if (rv == LIBUSB_SUCCESS) {
    scoped_refptr<UsbDeviceImpl> device(
        new UsbDeviceImpl(context_, platform_device, descriptor.idVendor,
                          descriptor.idProduct, blocking_task_runner_));

    base::Closure add_device =
        base::Bind(&UsbServiceImpl::AddDevice, weak_factory_.GetWeakPtr(),
                   refresh_complete, device);

#if defined(USE_UDEV)
    blocking_task_runner_->PostTask(
        FROM_HERE, base::Bind(&EnumerateUdevDevice, device, task_runner_,
                              add_device, refresh_complete));
#else
    if (descriptor.iManufacturer == 0 && descriptor.iProduct == 0 &&
        descriptor.iSerialNumber == 0) {
      // Don't bother disturbing the device if it has no string descriptors to
      // offer.
      add_device.Run();
    } else {
      device->Open(base::Bind(&ReadDeviceLanguage, descriptor.iManufacturer,
                              descriptor.iProduct, descriptor.iSerialNumber,
                              add_device, refresh_complete));
    }
#endif
  } else {
    USB_LOG(EVENT) << "Failed to get device descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    refresh_complete.Run();
  }
}

void UsbServiceImpl::AddDevice(const base::Closure& refresh_complete,
                               scoped_refptr<UsbDeviceImpl> device) {
  auto it = devices_being_enumerated_.find(device->platform_device());
  if (it == devices_being_enumerated_.end()) {
    // Device was removed while being enumerated.
    refresh_complete.Run();
    return;
  }

  platform_devices_[device->platform_device()] = device;
  DCHECK(!ContainsKey(devices_, device->guid()));
  devices_[device->guid()] = device;

  USB_LOG(USER) << "USB device added: vendor=" << device->vendor_id() << " \""
                << device->manufacturer_string()
                << "\", product=" << device->product_id() << " \""
                << device->product_string() << "\", serial=\""
                << device->serial_number() << "\", guid=" << device->guid();

  if (enumeration_ready_) {
    NotifyDeviceAdded(device);
  }

  refresh_complete.Run();
}

void UsbServiceImpl::RemoveDevice(scoped_refptr<UsbDeviceImpl> device) {
  platform_devices_.erase(device->platform_device());
  devices_.erase(device->guid());

  USB_LOG(USER) << "USB device removed: guid=" << device->guid();

  NotifyDeviceRemoved(device);
  device->OnDisconnect();
}

// static
int LIBUSB_CALL UsbServiceImpl::HotplugCallback(libusb_context* context,
                                                PlatformUsbDevice device,
                                                libusb_hotplug_event event,
                                                void* user_data) {
  // It is safe to access the UsbServiceImpl* here because libusb takes a lock
  // around registering, deregistering and calling hotplug callback functions
  // and so guarantees that this function will not be called by the event
  // processing thread after it has been deregistered.
  UsbServiceImpl* self = reinterpret_cast<UsbServiceImpl*>(user_data);
  switch (event) {
    case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
      libusb_ref_device(device);  // Released in OnPlatformDeviceAdded.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnPlatformDeviceAdded(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnPlatformDeviceAdded,
                                  base::Unretained(self), device));
      }
      break;
    case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
      libusb_ref_device(device);  // Released in OnPlatformDeviceRemoved.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnPlatformDeviceRemoved(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnPlatformDeviceRemoved,
                                  base::Unretained(self), device));
      }
      break;
    default:
      NOTREACHED();
  }

  return 0;
}

void UsbServiceImpl::OnPlatformDeviceAdded(PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(platform_devices_, platform_device));
  EnumerateDevice(platform_device, base::Bind(&base::DoNothing));
  libusb_unref_device(platform_device);
}

void UsbServiceImpl::OnPlatformDeviceRemoved(
    PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  PlatformDeviceMap::iterator it = platform_devices_.find(platform_device);
  if (it != platform_devices_.end()) {
    RemoveDevice(it->second);
  } else {
    DCHECK(ContainsKey(devices_being_enumerated_, platform_device));
    devices_being_enumerated_.erase(platform_device);
  }
  libusb_unref_device(platform_device);
}

}  // namespace device
