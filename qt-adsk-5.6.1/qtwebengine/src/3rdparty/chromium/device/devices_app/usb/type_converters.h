// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_DEVICES_APP_USB_TYPE_CONVERTERS_H_
#define DEVICE_DEVICES_APP_USB_TYPE_CONVERTERS_H_

#include <vector>

#include "device/devices_app/usb/public/interfaces/device.mojom.h"
#include "device/devices_app/usb/public/interfaces/device_manager.mojom.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_filter.h"
#include "device/usb/usb_device_handle.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"

// Type converters to convert objects between internal device/usb data types and
// public Mojo interface data types.

namespace device {
class UsbDevice;
}

namespace mojo {

template <>
struct TypeConverter<device::UsbDeviceFilter, device::usb::DeviceFilterPtr> {
  static device::UsbDeviceFilter Convert(
      const device::usb::DeviceFilterPtr& mojo_filter);
};

template <>
struct TypeConverter<device::usb::TransferDirection,
                     device::UsbEndpointDirection> {
  static device::usb::TransferDirection Convert(
      const device::UsbEndpointDirection& direction);
};

template <>
struct TypeConverter<device::usb::TransferStatus, device::UsbTransferStatus> {
  static device::usb::TransferStatus Convert(
      const device::UsbTransferStatus& status);
};

template <>
struct TypeConverter<device::UsbDeviceHandle::TransferRequestType,
                     device::usb::ControlTransferType> {
  static device::UsbDeviceHandle::TransferRequestType Convert(
      const device::usb::ControlTransferType& type);
};

template <>
struct TypeConverter<device::UsbDeviceHandle::TransferRecipient,
                     device::usb::ControlTransferRecipient> {
  static device::UsbDeviceHandle::TransferRecipient Convert(
      const device::usb::ControlTransferRecipient& recipient);
};

template <>
struct TypeConverter<device::usb::EndpointType, device::UsbTransferType> {
  static device::usb::EndpointType Convert(const device::UsbTransferType& type);
};

template <>
struct TypeConverter<device::usb::EndpointInfoPtr,
                     device::UsbEndpointDescriptor> {
  static device::usb::EndpointInfoPtr Convert(
      const device::UsbEndpointDescriptor& endpoint);
};

template <>
struct TypeConverter<device::usb::AlternateInterfaceInfoPtr,
                     device::UsbInterfaceDescriptor> {
  static device::usb::AlternateInterfaceInfoPtr Convert(
      const device::UsbInterfaceDescriptor& interface);
};

// Note that this is an explicit vector-to-array conversion, as
// UsbInterfaceDescriptor collections are flattened to contain all alternate
// settings, whereas InterfaceInfos contain their own sets of alternates with
// a different structure type.
template <>
struct TypeConverter<mojo::Array<device::usb::InterfaceInfoPtr>,
                     std::vector<device::UsbInterfaceDescriptor>> {
  static mojo::Array<device::usb::InterfaceInfoPtr> Convert(
      const std::vector<device::UsbInterfaceDescriptor>& interfaces);
};

template <>
struct TypeConverter<device::usb::ConfigurationInfoPtr,
                     device::UsbConfigDescriptor> {
  static device::usb::ConfigurationInfoPtr Convert(
      const device::UsbConfigDescriptor& config);
};

template <>
struct TypeConverter<device::usb::DeviceInfoPtr, device::UsbDevice> {
  static device::usb::DeviceInfoPtr Convert(const device::UsbDevice& device);
};

}  // namespace mojo

#endif  // DEVICE_DEVICES_APP_USB_TYPE_CONVERTERS_H_
