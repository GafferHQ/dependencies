// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

using base::android::ScopedJavaLocalRef;

namespace device {

// BluetoothAdapterAndroid, along with the Java class
// org.chromium.device.bluetooth.BluetoothAdapter, implement BluetoothAdapter.
//
// BluetoothAdapterAndroid is reference counted, and owns the lifetime of the
// Java class BluetoothAdapter via j_adapter_.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterAndroid final
    : public BluetoothAdapter {
 public:
  // Create a BluetoothAdapterAndroid instance.
  //
  // |java_bluetooth_adapter_wrapper| is optional. If it is NULL the adapter
  // will return false for |IsPresent()| and not be functional.
  //
  // The BluetoothAdapterAndroid instance will indirectly hold a Java reference
  // to |java_bluetooth_adapter_wrapper|.
  static base::WeakPtr<BluetoothAdapterAndroid> Create(
      jobject java_bluetooth_adapter_wrapper);

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  // BluetoothAdapter:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  bool IsDiscovering() const override;
  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void RegisterAudioSink(
      const BluetoothAudioSink::Options& options,
      const AcquiredCallback& callback,
      const BluetoothAudioSink::ErrorCallback& error_callback) override;
  void RegisterAdvertisement(
      scoped_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const CreateAdvertisementErrorCallback& error_callback) override;

 protected:
  BluetoothAdapterAndroid();
  ~BluetoothAdapterAndroid() override;

  // BluetoothAdapter:
  void AddDiscoverySession(BluetoothDiscoveryFilter* discovery_filter,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) override;
  void RemoveDiscoverySession(BluetoothDiscoveryFilter* discovery_filter,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) override;
  void SetDiscoveryFilter(scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override;
  void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override;

  // Java object org.chromium.device.bluetooth.ChromeBluetoothAdapter.
  base::android::ScopedJavaGlobalRef<jobject> j_adapter_;

  std::string address_;
  std::string name_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterAndroid> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_ANDROID_H_
