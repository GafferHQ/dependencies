// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle_impl.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/libusb.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "dbus/file_descriptor.h"
#endif  // defined(OS_CHROMEOS)

namespace device {

namespace {

UsbEndpointDirection GetDirection(
    const libusb_endpoint_descriptor* descriptor) {
  switch (descriptor->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) {
    case LIBUSB_ENDPOINT_IN:
      return USB_DIRECTION_INBOUND;
    case LIBUSB_ENDPOINT_OUT:
      return USB_DIRECTION_OUTBOUND;
    default:
      NOTREACHED();
      return USB_DIRECTION_INBOUND;
  }
}

UsbSynchronizationType GetSynchronizationType(
    const libusb_endpoint_descriptor* descriptor) {
  switch ((descriptor->bmAttributes & LIBUSB_ISO_SYNC_TYPE_MASK) >> 2) {
    case LIBUSB_ISO_SYNC_TYPE_NONE:
      return USB_SYNCHRONIZATION_NONE;
    case LIBUSB_ISO_SYNC_TYPE_ASYNC:
      return USB_SYNCHRONIZATION_ASYNCHRONOUS;
    case LIBUSB_ISO_SYNC_TYPE_ADAPTIVE:
      return USB_SYNCHRONIZATION_ADAPTIVE;
    case LIBUSB_ISO_SYNC_TYPE_SYNC:
      return USB_SYNCHRONIZATION_SYNCHRONOUS;
    default:
      NOTREACHED();
      return USB_SYNCHRONIZATION_NONE;
  }
}

UsbTransferType GetTransferType(const libusb_endpoint_descriptor* descriptor) {
  switch (descriptor->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) {
    case LIBUSB_TRANSFER_TYPE_CONTROL:
      return USB_TRANSFER_CONTROL;
    case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
      return USB_TRANSFER_ISOCHRONOUS;
    case LIBUSB_TRANSFER_TYPE_BULK:
      return USB_TRANSFER_BULK;
    case LIBUSB_TRANSFER_TYPE_INTERRUPT:
      return USB_TRANSFER_INTERRUPT;
    default:
      NOTREACHED();
      return USB_TRANSFER_CONTROL;
  }
}

UsbUsageType GetUsageType(const libusb_endpoint_descriptor* descriptor) {
  switch ((descriptor->bmAttributes & LIBUSB_ISO_USAGE_TYPE_MASK) >> 4) {
    case LIBUSB_ISO_USAGE_TYPE_DATA:
      return USB_USAGE_DATA;
    case LIBUSB_ISO_USAGE_TYPE_FEEDBACK:
      return USB_USAGE_FEEDBACK;
    case LIBUSB_ISO_USAGE_TYPE_IMPLICIT:
      return USB_USAGE_EXPLICIT_FEEDBACK;
    default:
      NOTREACHED();
      return USB_USAGE_DATA;
  }
}

}  // namespace

UsbDeviceImpl::UsbDeviceImpl(
    scoped_refptr<UsbContext> context,
    PlatformUsbDevice platform_device,
    uint16 vendor_id,
    uint16 product_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : UsbDevice(vendor_id,
                product_id,
                base::string16(),
                base::string16(),
                base::string16()),
      platform_device_(platform_device),
      context_(context),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(blocking_task_runner) {
  CHECK(platform_device) << "platform_device cannot be NULL";
  libusb_ref_device(platform_device);
  RefreshConfiguration();
}

UsbDeviceImpl::~UsbDeviceImpl() {
  // The destructor must be safe to call from any thread.
  libusb_unref_device(platform_device_);
}

#if defined(OS_CHROMEOS)

void UsbDeviceImpl::CheckUsbAccess(const ResultCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  client->CheckPathAccess(device_path_, callback);
}

#endif  // defined(OS_CHROMEOS)

void UsbDeviceImpl::Open(const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_CHROMEOS)
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  client->OpenPath(
      device_path_,
      base::Bind(&UsbDeviceImpl::OnOpenRequestComplete, this, callback));
#else
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UsbDeviceImpl::OpenOnBlockingThread, this, callback));
#endif  // defined(OS_CHROMEOS)
}

bool UsbDeviceImpl::Close(scoped_refptr<UsbDeviceHandle> handle) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (HandlesVector::iterator it = handles_.begin(); it != handles_.end();
       ++it) {
    if (it->get() == handle.get()) {
      (*it)->InternalClose();
      handles_.erase(it);
      return true;
    }
  }
  return false;
}

const UsbConfigDescriptor* UsbDeviceImpl::GetConfiguration() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return configuration_.get();
}

void UsbDeviceImpl::OnDisconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Swap the list of handles into a local variable because closing all open
  // handles may release the last reference to this object.
  HandlesVector handles;
  swap(handles, handles_);

  for (const scoped_refptr<UsbDeviceHandleImpl>& handle : handles_) {
    handle->InternalClose();
  }
}

void UsbDeviceImpl::RefreshConfiguration() {
  libusb_config_descriptor* platform_config;
  int rv =
      libusb_get_active_config_descriptor(platform_device_, &platform_config);
  if (rv != LIBUSB_SUCCESS) {
    USB_LOG(EVENT) << "Failed to get config descriptor: "
                   << ConvertPlatformUsbErrorToString(rv);
    return;
  }

  configuration_.reset(new UsbConfigDescriptor());
  configuration_->configuration_value = platform_config->bConfigurationValue;
  configuration_->self_powered = (platform_config->bmAttributes & 0x40) != 0;
  configuration_->remote_wakeup = (platform_config->bmAttributes & 0x20) != 0;
  configuration_->maximum_power = platform_config->MaxPower * 2;

  for (size_t i = 0; i < platform_config->bNumInterfaces; ++i) {
    const struct libusb_interface* platform_interface =
        &platform_config->interface[i];
    for (int j = 0; j < platform_interface->num_altsetting; ++j) {
      const struct libusb_interface_descriptor* platform_alt_setting =
          &platform_interface->altsetting[j];
      UsbInterfaceDescriptor interface;

      interface.interface_number = platform_alt_setting->bInterfaceNumber;
      interface.alternate_setting = platform_alt_setting->bAlternateSetting;
      interface.interface_class = platform_alt_setting->bInterfaceClass;
      interface.interface_subclass = platform_alt_setting->bInterfaceSubClass;
      interface.interface_protocol = platform_alt_setting->bInterfaceProtocol;

      for (size_t k = 0; k < platform_alt_setting->bNumEndpoints; ++k) {
        const struct libusb_endpoint_descriptor* platform_endpoint =
            &platform_alt_setting->endpoint[k];
        UsbEndpointDescriptor endpoint;

        endpoint.address = platform_endpoint->bEndpointAddress;
        endpoint.direction = GetDirection(platform_endpoint);
        endpoint.maximum_packet_size = platform_endpoint->wMaxPacketSize;
        endpoint.synchronization_type =
            GetSynchronizationType(platform_endpoint);
        endpoint.transfer_type = GetTransferType(platform_endpoint);
        endpoint.usage_type = GetUsageType(platform_endpoint);
        endpoint.polling_interval = platform_endpoint->bInterval;
        endpoint.extra_data = std::vector<uint8_t>(
            platform_endpoint->extra,
            platform_endpoint->extra + platform_endpoint->extra_length);

        interface.endpoints.push_back(endpoint);
      }

      interface.extra_data = std::vector<uint8_t>(
          platform_alt_setting->extra,
          platform_alt_setting->extra + platform_alt_setting->extra_length);

      configuration_->interfaces.push_back(interface);
    }
  }

  configuration_->extra_data = std::vector<uint8_t>(
      platform_config->extra,
      platform_config->extra + platform_config->extra_length);

  libusb_free_config_descriptor(platform_config);
}

#if defined(OS_CHROMEOS)

void UsbDeviceImpl::OnOpenRequestComplete(const OpenCallback& callback,
                                          dbus::FileDescriptor fd) {
  blocking_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UsbDeviceImpl::OpenOnBlockingThreadWithFd, this,
                            base::Passed(&fd), callback));
}

void UsbDeviceImpl::OpenOnBlockingThreadWithFd(dbus::FileDescriptor fd,
                                               const OpenCallback& callback) {
  fd.CheckValidity();
  if (!fd.is_valid()) {
    USB_LOG(EVENT) << "Did not get valid device handle from permission broker.";
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }

  PlatformUsbDeviceHandle handle;
  const int rv = libusb_open_fd(platform_device_, fd.TakeValue(), &handle);
  if (LIBUSB_SUCCESS == rv) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceImpl::Opened, this, handle, callback));
  } else {
    USB_LOG(EVENT) << "Failed to open device: "
                   << ConvertPlatformUsbErrorToString(rv);
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }
}

#endif  // defined(OS_CHROMEOS)

void UsbDeviceImpl::OpenOnBlockingThread(const OpenCallback& callback) {
  PlatformUsbDeviceHandle handle;
  const int rv = libusb_open(platform_device_, &handle);
  if (LIBUSB_SUCCESS == rv) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&UsbDeviceImpl::Opened, this, handle, callback));
  } else {
    USB_LOG(EVENT) << "Failed to open device: "
                   << ConvertPlatformUsbErrorToString(rv);
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }
}

void UsbDeviceImpl::Opened(PlatformUsbDeviceHandle platform_handle,
                           const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<UsbDeviceHandleImpl> device_handle = new UsbDeviceHandleImpl(
      context_, this, platform_handle, blocking_task_runner_);
  handles_.push_back(device_handle);
  callback.Run(device_handle);
}

}  // namespace device
