// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {

class SequencedTaskRunner;
class SequencedWorkerPool;

}  // namespace base

namespace device {

// Manages the blocking Bluetooth tasks using |SequencedWorkerPool|. It runs
// bluetooth tasks using |SequencedWorkerPool| and informs its observers of
// bluetooth adapter state changes and any other bluetooth device inquiry
// result.
//
// It delegates the blocking Windows API calls to |bluetooth_task_runner_|'s
// message loop, and receives responses via methods like OnAdapterStateChanged
// posted to UI thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothTaskManagerWin
    : public base::RefCountedThreadSafe<BluetoothTaskManagerWin> {
 public:
  struct DEVICE_BLUETOOTH_EXPORT AdapterState {
    AdapterState();
    ~AdapterState();
    std::string name;
    std::string address;
    bool powered;
  };

  struct DEVICE_BLUETOOTH_EXPORT ServiceRecordState {
    ServiceRecordState();
    ~ServiceRecordState();
    // Properties common to Bluetooth Classic and LE devices.
    std::string name;
    // Properties specific to Bluetooth Classic devices.
    std::vector<uint8> sdp_bytes;
    // Properties specific to Bluetooth LE devices.
    BluetoothUUID gatt_uuid;
  };

  struct DEVICE_BLUETOOTH_EXPORT DeviceState {
    DeviceState();
    ~DeviceState();

    bool is_bluetooth_classic() const { return path.empty(); }

    // Properties common to Bluetooth Classic and LE devices.
    std::string address;  // This uniquely identifies the device.
    std::string name;     // Friendly name
    bool visible;
    bool connected;
    bool authenticated;
    ScopedVector<ServiceRecordState> service_record_states;
    // Properties specific to Bluetooth Classic devices.
    uint32 bluetooth_class;
    // Properties specific to Bluetooth LE devices.
    base::FilePath path;
  };

  class DEVICE_BLUETOOTH_EXPORT Observer {
   public:
     virtual ~Observer() {}

     virtual void AdapterStateChanged(const AdapterState& state) {}
     virtual void DiscoveryStarted(bool success) {}
     virtual void DiscoveryStopped() {}
     // Called when the adapter has just been polled for the list of *all* known
     // devices. This includes devices previously paired, devices paired using
     // the underlying Operating System UI, and devices discovered recently due
     // to an active discovery session. Note that for a given device (address),
     // the associated state can change over time. For example, during a
     // discovery session, the "friendly" name may initially be "unknown" before
     // the actual name is retrieved in subsequent poll events.
     virtual void DevicesPolled(const ScopedVector<DeviceState>& devices) {}
  };

  explicit BluetoothTaskManagerWin(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Initialize();
  void InitializeWithBluetoothTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);
  void Shutdown();

  void PostSetPoweredBluetoothTask(
      bool powered,
      const base::Closure& callback,
      const BluetoothAdapter::ErrorCallback& error_callback);
  void PostStartDiscoveryTask();
  void PostStopDiscoveryTask();

 private:
  friend class base::RefCountedThreadSafe<BluetoothTaskManagerWin>;
  friend class BluetoothTaskManagerWinTest;

  static const int kPollIntervalMs;

  virtual ~BluetoothTaskManagerWin();

  // Logs Win32 errors occurring during polling on the worker thread. The method
  // may discard messages to avoid logging being too verbose.
  void LogPollingError(const char* message, int win32_error);

  // Notify all Observers of updated AdapterState. Should only be called on the
  // UI thread.
  void OnAdapterStateChanged(const AdapterState* state);
  void OnDiscoveryStarted(bool success);
  void OnDiscoveryStopped();
  void OnDevicesPolled(const ScopedVector<DeviceState>* devices);

  // Called on BluetoothTaskRunner.
  void StartPolling();
  void PollAdapter();
  void PostAdapterStateToUi();
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const BluetoothAdapter::ErrorCallback& error_callback);

  // Starts discovery. Once the discovery starts, it issues a discovery inquiry
  // with a short timeout, then issues more inquiries with greater timeout
  // values. The discovery finishes when StopDiscovery() is called or timeout
  // has reached its maximum value.
  void StartDiscovery();
  void StopDiscovery();

  // Issues a device inquiry that runs for |timeout_multiplier| * 1.28 seconds.
  // This posts itself again with |timeout_multiplier| + 1 until
  // |timeout_multiplier| reaches the maximum value or stop discovery call is
  // received.
  void DiscoverDevices(int timeout_multiplier);

  // Fetch already known device information. Similar to |StartDiscovery|, except
  // this function does not issue a discovery inquiry. Instead it gets the
  // device info cached in the adapter.
  void GetKnownDevices();

  // Looks for Bluetooth Classic and Low Energy devices, as well as the services
  // exposed by those devices.
  bool SearchDevices(int timeout_multiplier,
                     bool search_cached_devices_only,
                     ScopedVector<DeviceState>* device_list);

  // Sends a device search API call to the adapter to look for Bluetooth Classic
  // devices.
  bool SearchClassicDevices(int timeout_multiplier,
                            bool search_cached_devices_only,
                            ScopedVector<DeviceState>* device_list);

  // Enumerate Bluetooth Low Energy devices.
  bool SearchLowEnergyDevices(ScopedVector<DeviceState>* device_list);

  // Discover services for the devices in |device_list|.
  bool DiscoverServices(ScopedVector<DeviceState>* device_list,
                        bool search_cached_services_only);

  // Discover Bluetooth Classic services for the given |device_address|.
  bool DiscoverClassicDeviceServices(
      const std::string& device_address,
      const GUID& protocol_uuid,
      bool search_cached_services_only,
      ScopedVector<ServiceRecordState>* service_record_states);

  // Discover Bluetooth Classic services for the given |device_address|.
  // Returns a Win32 error code.
  int DiscoverClassicDeviceServicesWorker(
      const std::string& device_address,
      const GUID& protocol_uuid,
      bool search_cached_services_only,
      ScopedVector<ServiceRecordState>* service_record_states);

  // Discover Bluetooth Low Energy services for the given |device_path|.
  bool DiscoverLowEnergyDeviceServices(
      const base::FilePath& device_path,
      ScopedVector<ServiceRecordState>* service_record_states);

  // UI task runner reference.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner_;

  // List of observers interested in event notifications.
  base::ObserverList<Observer> observers_;

  // Adapter handle owned by bluetooth task runner.
  base::win::ScopedHandle adapter_handle_;

  // indicates whether the adapter is in discovery mode or not.
  bool discovering_;

  // Use for discarding too many log messages.
  base::TimeTicks current_logging_batch_ticks_;
  int current_logging_batch_count_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothTaskManagerWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
