// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_FILTER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_FILTER_H_

#include <set>

#include "base/memory/scoped_vector.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

// Used to keep a discovery filter that can be used to limit reported devices.
class DEVICE_BLUETOOTH_EXPORT BluetoothDiscoveryFilter {
 public:
  // Possible transports to use for scan filter.
  enum Transport {
    TRANSPORT_CLASSIC = 0x01,
    TRANSPORT_LE = 0x02,
    TRANSPORT_DUAL = (TRANSPORT_CLASSIC | TRANSPORT_LE)
  };
  using TransportMask = uint8_t;

  BluetoothDiscoveryFilter(TransportMask transport);
  ~BluetoothDiscoveryFilter();

  // These getters return true when given field is set in filter, and copy this
  // value to |out_*| parameter. If value is not set, returns false.
  // Thes setters assign given value to proper filter field.
  bool GetRSSI(int16_t* out_rssi) const;
  void SetRSSI(int16_t rssi);
  bool GetPathloss(uint16_t* out_pathloss) const;
  void SetPathloss(uint16_t pathloss);

  // Return and set transport field of this filter.
  TransportMask GetTransport() const;
  void SetTransport(TransportMask transport);

  // Make |out_uuids| represent all uuids assigned to this filter.
  void GetUUIDs(std::set<device::BluetoothUUID>& out_uuids) const;

  // Add UUID to internal UUIDs filter. If UUIDs filter doesn't exist, it will
  // be created.
  void AddUUID(const device::BluetoothUUID& uuid);

  // Copy content of |filter| and assigns it to this filter.
  void CopyFrom(const BluetoothDiscoveryFilter& filter);

  // Check if two filters are equal.
  bool Equals(const BluetoothDiscoveryFilter& filter) const;

  // Returns true if all fields in filter are empty
  bool IsDefault() const;

  // Returns result of merging two filters together. If at least one of the
  // filters is NULL this will return an empty filter
  static scoped_ptr<device::BluetoothDiscoveryFilter> Merge(
      const device::BluetoothDiscoveryFilter* filter_a,
      const device::BluetoothDiscoveryFilter* filter_b);

 private:
  scoped_ptr<int16_t> rssi_;
  scoped_ptr<uint16_t> pathloss_;
  TransportMask transport_;
  ScopedVector<device::BluetoothUUID> uuids_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDiscoveryFilter);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_FILTER_H_
