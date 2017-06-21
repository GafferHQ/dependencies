// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_audio_sink.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}  // namespace base

namespace device {

class BluetoothAdapterWinTest;
class BluetoothDevice;
class BluetoothSocketThread;

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterWin
    : public BluetoothAdapter,
      public BluetoothTaskManagerWin::Observer {
 public:
  static base::WeakPtr<BluetoothAdapter> CreateAdapter(
      const InitCallback& init_callback);

  // BluetoothAdapter:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool discoverable,
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

  // BluetoothTaskManagerWin::Observer override
  void AdapterStateChanged(
      const BluetoothTaskManagerWin::AdapterState& state) override;
  void DiscoveryStarted(bool success) override;
  void DiscoveryStopped() override;
  void DevicesPolled(const ScopedVector<BluetoothTaskManagerWin::DeviceState>&
                         devices) override;

  const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner() const {
    return ui_task_runner_;
  }
  const scoped_refptr<BluetoothSocketThread>& socket_thread() const {
    return socket_thread_;
  }

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class BluetoothAdapterWinTest;

  enum DiscoveryStatus {
    NOT_DISCOVERING,
    DISCOVERY_STARTING,
    DISCOVERING,
    DISCOVERY_STOPPING
  };

  explicit BluetoothAdapterWin(const InitCallback& init_callback);
  ~BluetoothAdapterWin() override;

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

  void Init();
  void InitForTest(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);

  void MaybePostStartDiscoveryTask();
  void MaybePostStopDiscoveryTask();

  InitCallback init_callback_;
  std::string address_;
  std::string name_;
  bool initialized_;
  bool powered_;
  DiscoveryStatus discovery_status_;
  base::hash_set<std::string> discovered_devices_;

  std::vector<std::pair<base::Closure, ErrorCallback> >
      on_start_discovery_callbacks_;
  std::vector<base::Closure> on_stop_discovery_callbacks_;
  size_t num_discovery_listeners_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothSocketThread> socket_thread_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;

  base::ThreadChecker thread_checker_;

  // NOTE: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterWin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
