// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_UUID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_UUID_H_

#include <string>

#include "device/bluetooth/bluetooth_export.h"
#include "ipc/ipc_param_traits.h"

namespace base {
class PickleIterator;
}  // namespace base

namespace device {

// Opaque wrapper around a Bluetooth UUID. Instances of UUID represent the
// 128-bit universally unique identifiers (UUIDs) of profiles and attributes
// used in Bluetooth based communication, such as a peripheral's services,
// characteristics, and characteristic descriptors. An instance are
// constructed using a string representing 16, 32, or 128 bit UUID formats.
class DEVICE_BLUETOOTH_EXPORT BluetoothUUID {
 public:
  // Possible representation formats used during construction.
  enum Format {
    kFormatInvalid,
    kFormat16Bit,
    kFormat32Bit,
    kFormat128Bit
  };

  // Single argument constructor. |uuid| can be a 16, 32, or 128 bit UUID
  // represented as a 4, 8, or 36 character string with the following
  // formats:
  //   xxxx
  //   0xxxxx
  //   xxxxxxxx
  //   0xxxxxxxxx
  //   xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  //
  // 16 and 32 bit UUIDs will be internally converted to a 128 bit UUID using
  // the base UUID defined in the Bluetooth specification, hence custom UUIDs
  // should be provided in the 128-bit format. If |uuid| is in an unsupported
  // format, the result might be invalid. Use IsValid to check for validity
  // after construction.
  explicit BluetoothUUID(const std::string& uuid);

  // Default constructor does nothing. Since BluetoothUUID is copyable, this
  // constructor is useful for initializing member variables and assigning a
  // value to them later. The default constructor will initialize an invalid
  // UUID by definition and the string accessors will return an empty string.
  BluetoothUUID();
  virtual ~BluetoothUUID();

  // Returns true, if the UUID is in a valid canonical format.
  bool IsValid() const;

  // Returns the representation format of the UUID. This reflects the format
  // that was provided during construction.
  Format format() const { return format_; }

  // Returns the value of the UUID as a string. The representation format is
  // based on what was passed in during construction. For the supported sizes,
  // this representation can have the following formats:
  //   - 16 bit:  xxxx
  //   - 32 bit:  xxxxxxxx
  //   - 128 bit: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  // where x is a lowercase hex digit.
  const std::string& value() const { return value_; }

  // Returns the underlying 128-bit value as a string in the following format:
  //   xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  // where x is a lowercase hex digit.
  const std::string& canonical_value() const { return canonical_value_; }

  // Permit sufficient comparison to allow a UUID to be used as a key in a
  // std::map.
  bool operator<(const BluetoothUUID& uuid) const;

  // Equality operators.
  bool operator==(const BluetoothUUID& uuid) const;
  bool operator!=(const BluetoothUUID& uuid) const;

 private:
  // String representation of the UUID that was used during construction. For
  // the supported sizes, this representation can have the following formats:
  //   - 16 bit:  xxxx
  //   - 32 bit:  xxxxxxxx
  //   - 128 bit: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  Format format_;
  std::string value_;

  // The 128-bit string representation of the UUID.
  std::string canonical_value_;
};

// This is required by gtest to print a readable output on test failures.
void DEVICE_BLUETOOTH_EXPORT
PrintTo(const BluetoothUUID& uuid, std::ostream* out);

}  // namespace device

namespace IPC {

class Message;

// Tell the IPC system how to serialize and deserialize a BluetoothUUID.
template <>
struct DEVICE_BLUETOOTH_EXPORT ParamTraits<device::BluetoothUUID> {
  typedef device::BluetoothUUID param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_UUID_H_
