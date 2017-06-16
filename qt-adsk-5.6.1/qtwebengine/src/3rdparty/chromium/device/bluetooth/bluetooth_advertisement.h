// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// BluetoothAdvertisement represents an advertisement which advertises over the
// LE channel during its lifetime.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisement
    : public base::RefCounted<BluetoothAdvertisement> {
 public:
  // Possible types of error raised while registering or unregistering
  // advertisements.
  enum ErrorCode {
    ERROR_UNSUPPORTED_PLATFORM,  // Bluetooth advertisement not supported on
                                 // current platform.
    ERROR_ADVERTISEMENT_ALREADY_EXISTS,  // An advertisement is already
                                         // registered.
    ERROR_ADVERTISEMENT_DOES_NOT_EXIST,  // Unregistering an advertisement which
                                         // is not registered.
    ERROR_ADVERTISEMENT_INVALID_LENGTH,  // Advertisement is not of a valid
                                         // length.
    INVALID_ADVERTISEMENT_ERROR_CODE
  };

  // Type of advertisement.
  enum AdvertisementType {
    // This advertises with the type set to ADV_NONCONN_IND, which indicates
    // to receivers that our device is not connectable.
    ADVERTISEMENT_TYPE_BROADCAST,
    // This advertises with the type set to ADV_IND or ADV_SCAN_IND, which
    // indicates to receivers that our device is connectable.
    ADVERTISEMENT_TYPE_PERIPHERAL
  };

  using UUIDList = std::vector<std::string>;
  using ManufacturerData = std::map<uint16_t, std::vector<uint8_t>>;
  using ServiceData = std::map<std::string, std::vector<uint8_t>>;

  // Structure that holds the data for an advertisement.
  class DEVICE_BLUETOOTH_EXPORT Data {
   public:
    explicit Data(AdvertisementType type);
    ~Data();

    AdvertisementType type() { return type_; }
    scoped_ptr<UUIDList> service_uuids() { return service_uuids_.Pass(); }
    scoped_ptr<ManufacturerData> manufacturer_data() {
      return manufacturer_data_.Pass();
    }
    scoped_ptr<UUIDList> solicit_uuids() { return solicit_uuids_.Pass(); }
    scoped_ptr<ServiceData> service_data() { return service_data_.Pass(); }

    void set_service_uuids(scoped_ptr<UUIDList> service_uuids) {
      service_uuids_ = service_uuids.Pass();
    }
    void set_manufacturer_data(scoped_ptr<ManufacturerData> manufacturer_data) {
      manufacturer_data_ = manufacturer_data.Pass();
    }
    void set_solicit_uuids(scoped_ptr<UUIDList> solicit_uuids) {
      solicit_uuids = solicit_uuids_.Pass();
    }
    void set_service_data(scoped_ptr<ServiceData> service_data) {
      service_data = service_data_.Pass();
    }

    void set_include_tx_power(bool include_tx_power) {
      include_tx_power_ = include_tx_power;
    }

   private:
    Data();

    AdvertisementType type_;
    scoped_ptr<UUIDList> service_uuids_;
    scoped_ptr<ManufacturerData> manufacturer_data_;
    scoped_ptr<UUIDList> solicit_uuids_;
    scoped_ptr<ServiceData> service_data_;
    bool include_tx_power_;

    DISALLOW_COPY_AND_ASSIGN(Data);
  };

  // Interface for observing changes to this advertisement.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when this advertisement is released and is no longer advertising.
    virtual void AdvertisementReleased(
        BluetoothAdvertisement* advertisement) = 0;
  };

  // Adds and removes observers for events for this advertisement.
  void AddObserver(BluetoothAdvertisement::Observer* observer);
  void RemoveObserver(BluetoothAdvertisement::Observer* observer);

  // Unregisters this advertisement. Called on destruction of this object
  // automatically but can be called directly to explicitly unregister this
  // object.
  using SuccessCallback = base::Closure;
  using ErrorCallback = base::Callback<void(ErrorCode)>;
  virtual void Unregister(const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback) = 0;

 protected:
  friend class base::RefCounted<BluetoothAdvertisement>;

  BluetoothAdvertisement();

  // The destructor will unregister this advertisement.
  virtual ~BluetoothAdvertisement();

  // List of observers interested in event notifications from us. Objects in
  // |observers_| are expected to outlive a BluetoothAdvertisement object.
  base::ObserverList<BluetoothAdvertisement::Observer> observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothAdvertisement);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_H_
