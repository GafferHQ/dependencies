// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "net/log/net_log.h"

namespace base {
class BinaryValue;
}

namespace device {

class BluetoothGattConnection;
class BluetoothGattService;
class BluetoothSocket;
class BluetoothUUID;

// BluetoothDevice represents a remote Bluetooth device, both its properties and
// capabilities as discovered by a local adapter and actions that may be
// performed on the remove device such as pairing, connection and disconnection.
//
// The class is instantiated and managed by the BluetoothAdapter class
// and pointers should only be obtained from that class and not cached,
// instead use the GetAddress() method as a unique key for a device.
//
// Since the lifecycle of BluetoothDevice instances is managed by
// BluetoothAdapter, that class rather than this provides observer methods
// for devices coming and going, as well as properties being updated.
class DEVICE_BLUETOOTH_EXPORT BluetoothDevice {
 public:
  // Possible values that may be returned by GetVendorIDSource(),
  // indicating different organisations that allocate the identifiers returned
  // by GetVendorID().
  enum VendorIDSource {
    VENDOR_ID_UNKNOWN,
    VENDOR_ID_BLUETOOTH,
    VENDOR_ID_USB,
    VENDOR_ID_MAX_VALUE = VENDOR_ID_USB
  };

  // Possible values that may be returned by GetDeviceType(), representing
  // different types of bluetooth device that we support or are aware of
  // decoded from the bluetooth class information.
  enum DeviceType {
    DEVICE_UNKNOWN,
    DEVICE_COMPUTER,
    DEVICE_PHONE,
    DEVICE_MODEM,
    DEVICE_AUDIO,
    DEVICE_CAR_AUDIO,
    DEVICE_VIDEO,
    DEVICE_PERIPHERAL,
    DEVICE_JOYSTICK,
    DEVICE_GAMEPAD,
    DEVICE_KEYBOARD,
    DEVICE_MOUSE,
    DEVICE_TABLET,
    DEVICE_KEYBOARD_MOUSE_COMBO
  };

  // The value returned if the RSSI or transmit power cannot be read.
  static const int kUnknownPower = 127;

  struct DEVICE_BLUETOOTH_EXPORT ConnectionInfo {
    int rssi;
    int transmit_power;
    int max_transmit_power;

    ConnectionInfo();
    ConnectionInfo(int rssi, int transmit_power, int max_transmit_power);
    ~ConnectionInfo();
  };

  // Possible errors passed back to an error callback function in case of a
  // failed call to Connect().
  enum ConnectErrorCode {
    ERROR_UNKNOWN,
    ERROR_INPROGRESS,
    ERROR_FAILED,
    ERROR_AUTH_FAILED,
    ERROR_AUTH_CANCELED,
    ERROR_AUTH_REJECTED,
    ERROR_AUTH_TIMEOUT,
    ERROR_UNSUPPORTED_DEVICE
  };

  typedef std::vector<BluetoothUUID> UUIDList;

  // Interface for negotiating pairing of bluetooth devices.
  class PairingDelegate {
   public:
    virtual ~PairingDelegate() {}

    // This method will be called when the Bluetooth daemon requires a
    // PIN Code for authentication of the device |device|, the delegate should
    // obtain the code from the user and call SetPinCode() on the device to
    // provide it, or RejectPairing() or CancelPairing() to reject or cancel
    // the request.
    //
    // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
    // for which there is no automatic pairing or special handling.
    virtual void RequestPinCode(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires a
    // Passkey for authentication of the device |device|, the delegate should
    // obtain the passkey from the user (a numeric in the range 0-999999) and
    // call SetPasskey() on the device to provide it, or RejectPairing() or
    // CancelPairing() to reject or cancel the request.
    //
    // Passkeys are generally required for Bluetooth 2.1 and later devices
    // which cannot provide input or display on their own, and don't accept
    // passkey-less pairing.
    virtual void RequestPasskey(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the PIN code |pincode| into the device |device| so that it
    // may be authenticated.
    //
    // This is used for Bluetooth 2.0 and earlier keyboard devices, the
    // |pincode| will always be a six-digit numeric in the range 000000-999999
    // for compatibility with later specifications.
    virtual void DisplayPinCode(BluetoothDevice* device,
                                const std::string& pincode) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the Passkey |passkey| into the device |device| so that it
    // may be authenticated.
    //
    // This is used for Bluetooth 2.1 and later devices that support input
    // but not display, such as keyboards. The Passkey is a numeric in the
    // range 0-999999 and should be always presented zero-padded to six
    // digits.
    virtual void DisplayPasskey(BluetoothDevice* device,
                                uint32 passkey) = 0;

    // This method will be called when the Bluetooth daemon gets a notification
    // of a key entered on the device |device| while pairing with the device
    // using a PIN code or a Passkey.
    //
    // This method will be called only after DisplayPinCode() or
    // DisplayPasskey() method is called, but is not warranted to be called
    // on every pairing process that requires a PIN code or a Passkey because
    // some device may not support this feature.
    //
    // The |entered| value describes the number of keys entered so far,
    // including the last [enter] key. A first call to KeysEntered() with
    // |entered| as 0 will be sent when the device supports this feature.
    virtual void KeysEntered(BluetoothDevice* device,
                             uint32 entered) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user confirm that the Passkey |passkey| is displayed on the screen
    // of the device |device| so that it may be authenticated. The delegate
    // should display to the user and ask for confirmation, then call
    // ConfirmPairing() on the device to confirm, RejectPairing() on the device
    // to reject or CancelPairing() on the device to cancel authentication
    // for any other reason.
    //
    // This is used for Bluetooth 2.1 and later devices that support display,
    // such as other computers or phones. The Passkey is a numeric in the
    // range 0-999999 and should be always present zero-padded to six
    // digits.
    virtual void ConfirmPasskey(BluetoothDevice* device,
                                uint32 passkey) = 0;

    // This method will be called when the Bluetooth daemon requires that a
    // pairing request, usually only incoming, using the just-works model is
    // authorized. The delegate should decide whether the user should confirm
    // or not, then call ConfirmPairing() on the device to confirm the pairing
    // (whether by user action or by default), RejectPairing() on the device to
    // reject or CancelPairing() on the device to cancel authorization for
    // any other reason.
    virtual void AuthorizePairing(BluetoothDevice* device) = 0;
  };

  virtual ~BluetoothDevice();

  // Returns the Bluetooth class of the device, used by GetDeviceType()
  // and metrics logging,
  virtual uint32 GetBluetoothClass() const = 0;

  // Returns the identifier of the bluetooth device.
  virtual std::string GetIdentifier() const;

  // Returns the Bluetooth of address the device. This should be used as
  // a unique key to identify the device and copied where needed.
  virtual std::string GetAddress() const = 0;

  // Returns the allocation source of the identifier returned by GetVendorID(),
  // where available, or VENDOR_ID_UNKNOWN where not.
  virtual VendorIDSource GetVendorIDSource() const = 0;

  // Returns the Vendor ID of the device, where available.
  virtual uint16 GetVendorID() const = 0;

  // Returns the Product ID of the device, where available.
  virtual uint16 GetProductID() const = 0;

  // Returns the Device ID of the device, typically the release or version
  // number in BCD format, where available.
  virtual uint16 GetDeviceID() const = 0;

  // Returns the name of the device suitable for displaying, this may
  // be a synthesized string containing the address and localized type name
  // if the device has no obtained name.
  virtual base::string16 GetName() const;

  // Returns the type of the device, limited to those we support or are
  // aware of, by decoding the bluetooth class information. The returned
  // values are unique, and do not overlap, so DEVICE_KEYBOARD is not also
  // DEVICE_PERIPHERAL.
  DeviceType GetDeviceType() const;

  // Indicates whether the device is known to support pairing based on its
  // device class and address.
  bool IsPairable() const;

  // Indicates whether the device is paired with the adapter.
  virtual bool IsPaired() const = 0;

  // Indicates whether the device is currently connected to the adapter.
  // Note that if IsConnected() is true, does not imply that the device is
  // connected to any application or service. If the device is not paired, it
  // could be still connected to the adapter for other reason, for example, to
  // request the adapter's SDP records. The same holds for paired devices, since
  // they could be connected to the adapter but not to an application.
  virtual bool IsConnected() const = 0;

  // Indicates whether the paired device accepts connections initiated from the
  // adapter. This value is undefined for unpaired devices.
  virtual bool IsConnectable() const = 0;

  // Indicates whether there is a call to Connect() ongoing. For this attribute,
  // we consider a call is ongoing if none of the callbacks passed to Connect()
  // were called after the corresponding call to Connect().
  virtual bool IsConnecting() const = 0;

  // Indicates whether the device can be trusted, based on device properties,
  // such as vendor and product id.
  bool IsTrustable() const;

  // Returns the set of UUIDs that this device supports. For classic Bluetooth
  // devices this data is collected from both the EIR data and SDP tables,
  // for Low Energy devices this data is collected from AD and GATT primary
  // services, for dual mode devices this may be collected from both./
  virtual UUIDList GetUUIDs() const = 0;

  // The received signal strength, in dBm. This field is avaliable and valid
  // only during discovery. If not during discovery, or RSSI wasn't reported,
  // this method will return |kUnknownPower|.
  virtual int16 GetInquiryRSSI() const = 0;

  // The transmitted power level. This field is avaliable only for LE devices
  // that include this field in AD. It is avaliable and valid only during
  // discovery. If not during discovery, or TxPower wasn't reported, this
  // method will return |kUnknownPower|.
  virtual int16 GetInquiryTxPower() const = 0;

  // The ErrorCallback is used for methods that can fail in which case it
  // is called, in the success case the callback is simply not called.
  typedef base::Callback<void()> ErrorCallback;

  // The ConnectErrorCallback is used for methods that can fail with an error,
  // passed back as an error code argument to this callback.
  // In the success case this callback is not called.
  typedef base::Callback<void(enum ConnectErrorCode)> ConnectErrorCallback;

  typedef base::Callback<void(const ConnectionInfo&)> ConnectionInfoCallback;

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  virtual bool ExpectingPinCode() const = 0;

  // Indicates whether the device is currently pairing and expecting a
  // Passkey to be returned.
  virtual bool ExpectingPasskey() const = 0;

  // Indicates whether the device is currently pairing and expecting
  // confirmation of a displayed passkey.
  virtual bool ExpectingConfirmation() const = 0;

  // Returns the RSSI and TX power of the active connection to the device:
  //
  // The RSSI indicates the power present in the received radio signal, measured
  // in dBm, to a resolution of 1dBm. Larger (typically, less negative) values
  // indicate a stronger signal.
  //
  // The transmit power indicates the strength of the signal broadcast from the
  // host's Bluetooth antenna when communicating with the device, measured in
  // dBm, to a resolution of 1dBm. Larger (typically, less negative) values
  // indicate a stronger signal.
  //
  // If the device isn't connected, then the ConnectionInfo struct passed into
  // the callback will be populated with |kUnknownPower|.
  virtual void GetConnectionInfo(const ConnectionInfoCallback& callback) = 0;

  // Initiates a connection to the device, pairing first if necessary.
  //
  // Method calls will be made on the supplied object |pairing_delegate|
  // to indicate what display, and in response should make method calls
  // back to the device object. Not all devices require user responses
  // during pairing, so it is normal for |pairing_delegate| to receive no
  // calls. To explicitly force a low-security connection without bonding,
  // pass NULL, though this is ignored if the device is already paired.
  //
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  // After calling Connect, CancelPairing should be called to cancel the pairing
  // process and release the pairing delegate if user cancels the pairing and
  // closes the pairing UI.
  virtual void Connect(PairingDelegate* pairing_delegate,
                       const base::Closure& callback,
                       const ConnectErrorCallback& error_callback) = 0;

  // Sends the PIN code |pincode| to the remote device during pairing.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  virtual void SetPinCode(const std::string& pincode) = 0;

  // Sends the Passkey |passkey| to the remote device during pairing.
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing, and are a numeric in the range 0-999999.
  virtual void SetPasskey(uint32 passkey) = 0;

  // Confirms to the remote device during pairing that a passkey provided by
  // the ConfirmPasskey() delegate call is displayed on both devices.
  virtual void ConfirmPairing() = 0;

  // Rejects a pairing or connection request from a remote device.
  virtual void RejectPairing() = 0;

  // Cancels a pairing or connection attempt to a remote device, releasing
  // the pairing delegate.
  virtual void CancelPairing() = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it. Link keys and other pairing
  // information are not discarded, and the device object is not deleted.
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  virtual void Disconnect(const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it, and then discards link keys
  // and other pairing information. The device object remains valid until
  // returning from the calling function, after which it should be assumed to
  // have been deleted. If the request fails, |error_callback| will be called.
  // There is no callback for success because this object is often deleted
  // before that callback would be called.
  virtual void Forget(const ErrorCallback& error_callback) = 0;

  // Attempts to initiate an outgoing L2CAP or RFCOMM connection to the
  // advertised service on this device matching |uuid|, performing an SDP lookup
  // if necessary to determine the correct protocol and channel for the
  // connection. |callback| will be called on a successful connection with a
  // BluetoothSocket instance that is to be owned by the receiver.
  // |error_callback| will be called on failure with a message indicating the
  // cause.
  typedef base::Callback<void(scoped_refptr<BluetoothSocket>)>
      ConnectToServiceCallback;
  typedef base::Callback<void(const std::string& message)>
      ConnectToServiceErrorCallback;
  virtual void ConnectToService(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) = 0;

  // Attempts to initiate an insecure outgoing L2CAP or RFCOMM connection to the
  // advertised service on this device matching |uuid|, performing an SDP lookup
  // if necessary to determine the correct protocol and channel for the
  // connection. Unlike ConnectToService, the outgoing connection will request
  // no bonding rather than general bonding. |callback| will be called on a
  // successful connection with a BluetoothSocket instance that is to be owned
  // by the receiver. |error_callback| will be called on failure with a message
  // indicating the cause.
  virtual void ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) = 0;

  // Opens a new GATT connection to this device. On success, a new
  // BluetoothGattConnection will be handed to the caller via |callback|. On
  // error, |error_callback| will be called. The connection will be kept alive,
  // as long as there is at least one active GATT connection. In the case that
  // the underlying connection gets terminated, either due to a call to
  // BluetoothDevice::Disconnect or other unexpected circumstances, the
  // returned BluetoothGattConnection will be automatically marked as inactive.
  // To monitor the state of the connection, observe the
  // BluetoothAdapter::Observer::DeviceChanged method.
  typedef base::Callback<void(scoped_ptr<BluetoothGattConnection>)>
      GattConnectionCallback;
  virtual void CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) = 0;

  // Returns the list of discovered GATT services.
  virtual std::vector<BluetoothGattService*> GetGattServices() const;

  // Returns the GATT service with device-specific identifier |identifier|.
  // Returns NULL, if no such service exists.
  virtual BluetoothGattService* GetGattService(
      const std::string& identifier) const;

  // Returns service data of a service given its UUID.
  virtual base::BinaryValue* GetServiceData(BluetoothUUID serviceUUID) const;

  // Returns the list UUIDs of services that have service data.
  virtual UUIDList GetServiceDataUUIDs() const;

  // Returns the |address| in the canonical format: XX:XX:XX:XX:XX:XX, where
  // each 'X' is a hex digit.  If the input |address| is invalid, returns an
  // empty string.
  static std::string CanonicalizeAddress(const std::string& address);

 protected:
  BluetoothDevice();

  // Returns the internal name of the Bluetooth device, used by GetName().
  virtual std::string GetDeviceName() const = 0;

  // Clears the list of service data.
  void ClearServiceData();

  // Set the data of a given service designated by its UUID.
  void SetServiceData(BluetoothUUID serviceUUID, const char* buffer,
                      size_t size);

  // Mapping from the platform-specific GATT service identifiers to
  // BluetoothGattService objects.
  typedef std::map<std::string, BluetoothGattService*> GattServiceMap;
  GattServiceMap gatt_services_;

  // Mapping from service UUID represented as a std::string of a bluetooth
  // service to
  // the specific data. The data is stored as BinaryValue.
  scoped_ptr<base::DictionaryValue> services_data_;

 private:
  // Returns a localized string containing the device's bluetooth address and
  // a device type for display when |name_| is empty.
  base::string16 GetAddressWithLocalizedDeviceTypeName() const;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
