// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_DEVICE_IMPL_H_
#define DEVICE_USB_DEVICE_IMPL_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/devices_app/usb/public/interfaces/device.mojom.h"
#include "device/usb/usb_device_handle.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/callback.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace net {
class IOBuffer;
}

namespace device {
namespace usb {

// Implementation of the public Device interface. Instances of this class are
// constructed by DeviceManagerImpl and are strongly bound to their MessagePipe
// lifetime.
class DeviceImpl : public Device {
 public:
  DeviceImpl(scoped_refptr<UsbDeviceHandle> device_handle,
             mojo::InterfaceRequest<Device> request);
  ~DeviceImpl() override;

 private:
  using MojoTransferInCallback =
      mojo::Callback<void(TransferStatus, mojo::Array<uint8_t>)>;

  using MojoTransferOutCallback = mojo::Callback<void(TransferStatus)>;

  // Closes the device if it's open. This will always set |handle_| to null.
  void CloseHandle();

  // Handles a UsbDeviceHandle::ClaimInterface response.
  void OnClaimInterface(uint8_t interface_number,
                        const ClaimInterfaceCallback& callback,
                        bool result);

  // Handles completion of an inbound transfer on the UsbDeviceHandle.
  void OnTransferIn(const MojoTransferInCallback& callback,
                    UsbTransferStatus status,
                    scoped_refptr<net::IOBuffer> data,
                    size_t size);

  // Handles completion of an outbound transfer on the UsbDeviceHandle.
  void OnTransferOut(const MojoTransferOutCallback& callback,
                     UsbTransferStatus status,
                     scoped_refptr<net::IOBuffer> data,
                     size_t size);

  // Handles completion of an inbound isochronous transfer on the
  // UsbDeviceHandle.
  void OnIsochronousTransferIn(const IsochronousTransferInCallback& callback,
                               uint32_t packet_length,
                               UsbTransferStatus status,
                               scoped_refptr<net::IOBuffer> data,
                               size_t size);

  // Handles completion of an outbound isochronous transfer on the
  // UsbDeviceHandle.
  void OnIsochronousTransferOut(const MojoTransferOutCallback& callback,
                                UsbTransferStatus status,
                                scoped_refptr<net::IOBuffer> data,
                                size_t size);

  // Device implementation:
  void Close(const CloseCallback& callback) override;
  void GetDeviceInfo(const GetDeviceInfoCallback& callback) override;
  void SetConfiguration(uint8_t value,
                        const SetConfigurationCallback& callback) override;
  void ClaimInterface(uint8_t interface_number,
                      const ClaimInterfaceCallback& callback) override;
  void ReleaseInterface(uint8_t interface_number,
                        const ReleaseInterfaceCallback& callback) override;
  void SetInterfaceAlternateSetting(
      uint8_t interface_number,
      uint8_t alternate_setting,
      const SetInterfaceAlternateSettingCallback& callback) override;
  void Reset(const ResetCallback& callback) override;
  void ControlTransferIn(ControlTransferParamsPtr params,
                         uint32_t length,
                         uint32_t timeout,
                         const ControlTransferInCallback& callback) override;
  void ControlTransferOut(ControlTransferParamsPtr params,
                          mojo::Array<uint8_t> data,
                          uint32_t timeout,
                          const ControlTransferOutCallback& callback) override;
  void BulkTransferIn(uint8_t endpoint_number,
                      uint32_t length,
                      uint32_t timeout,
                      const BulkTransferInCallback& callback) override;
  void BulkTransferOut(uint8_t endpoint_number,
                       mojo::Array<uint8_t> data,
                       uint32_t timeout,
                       const BulkTransferOutCallback& callback) override;
  void InterruptTransferIn(
      uint8_t endpoint_number,
      uint32_t length,
      uint32_t timeout,
      const InterruptTransferInCallback& callback) override;
  void InterruptTransferOut(
      uint8_t endpoint_number,
      mojo::Array<uint8_t> data,
      uint32_t timeout,
      const InterruptTransferOutCallback& callback) override;
  void IsochronousTransferIn(
      uint8_t endpoint_number,
      uint32_t num_packets,
      uint32_t packet_length,
      uint32_t timeout,
      const IsochronousTransferInCallback& callback) override;
  void IsochronousTransferOut(
      uint8_t endpoint_number,
      mojo::Array<mojo::Array<uint8_t>> packets,
      uint32_t timeout,
      const IsochronousTransferOutCallback& callback) override;

  mojo::StrongBinding<Device> binding_;

  // The opened device handle. May be null if the device has been closed.
  scoped_refptr<UsbDeviceHandle> device_handle_;

  base::WeakPtrFactory<DeviceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceImpl);
};

}  // namespace usb
}  // namespace device

#endif  // DEVICE_USB_DEVICE_IMPL_H_
