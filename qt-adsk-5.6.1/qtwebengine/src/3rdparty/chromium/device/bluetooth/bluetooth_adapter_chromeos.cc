// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_manager_client.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter_profile_chromeos.h"
#include "device/bluetooth/bluetooth_advertisement_chromeos.h"
#include "device/bluetooth/bluetooth_audio_sink_chromeos.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_pairing_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_chromeos.h"
#include "device/bluetooth/bluetooth_socket_chromeos.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothAdapter;
using device::BluetoothAudioSink;
using device::BluetoothDevice;
using device::BluetoothDiscoveryFilter;
using device::BluetoothSocket;
using device::BluetoothUUID;

namespace {

// The agent path is relatively meaningless since BlueZ only permits one to
// exist per D-Bus connection, it just has to be unique within Chromium.
const char kAgentPath[] = "/org/chromium/bluetooth_agent";

void OnUnregisterAgentError(const std::string& error_name,
                            const std::string& error_message) {
  // It's okay if the agent didn't exist, it means we never saw an adapter.
  if (error_name == bluetooth_agent_manager::kErrorDoesNotExist)
    return;

  LOG(WARNING) << "Failed to unregister pairing agent: "
               << error_name << ": " << error_message;
}

}  // namespace

namespace device {

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return chromeos::BluetoothAdapterChromeOS::CreateAdapter();
}

}  // namespace device

namespace chromeos {

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapterChromeOS::CreateAdapter() {
  BluetoothAdapterChromeOS* adapter = new BluetoothAdapterChromeOS();
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

void BluetoothAdapterChromeOS::Shutdown() {
  if (dbus_is_shutdown_)
    return;
  DCHECK(DBusThreadManager::IsInitialized())
      << "Call BluetoothAdapterFactory::Shutdown() before "
         "DBusThreadManager::Shutdown().";

  if (IsPresent())
    RemoveAdapter();  // Also deletes devices_.
  DCHECK(devices_.empty());
  // profiles_ is empty because all BluetoothSockets have been notified
  // that this adapter is disappearing.
  DCHECK(profiles_.empty());

  for (auto& it : profile_queues_)
    delete it.second;
  profile_queues_.clear();

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothInputClient()->RemoveObserver(this);

  VLOG(1) << "Unregistering pairing agent";
  DBusThreadManager::Get()->GetBluetoothAgentManagerClient()->UnregisterAgent(
      dbus::ObjectPath(kAgentPath), base::Bind(&base::DoNothing),
      base::Bind(&OnUnregisterAgentError));

  agent_.reset();
  dbus_is_shutdown_ = true;
}

BluetoothAdapterChromeOS::BluetoothAdapterChromeOS()
    : dbus_is_shutdown_(false),
      num_discovery_sessions_(0),
      discovery_request_pending_(false),
      weak_ptr_factory_(this) {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  socket_thread_ = device::BluetoothSocketThread::Get();

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothInputClient()->AddObserver(this);

  // Register the pairing agent.
  dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
  agent_.reset(BluetoothAgentServiceProvider::Create(
      system_bus, dbus::ObjectPath(kAgentPath), this));
  DCHECK(agent_.get());

  std::vector<dbus::ObjectPath> object_paths =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->GetAdapters();

  if (!object_paths.empty()) {
    VLOG(1) << object_paths.size() << " Bluetooth adapter(s) available.";
    SetAdapter(object_paths[0]);
  }
}

BluetoothAdapterChromeOS::~BluetoothAdapterChromeOS() {
  Shutdown();
}

std::string BluetoothAdapterChromeOS::GetAddress() const {
  if (!IsPresent())
    return std::string();

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return BluetoothDevice::CanonicalizeAddress(properties->address.value());
}

std::string BluetoothAdapterChromeOS::GetName() const {
  if (!IsPresent())
    return std::string();

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  return properties->alias.value();
}

void BluetoothAdapterChromeOS::SetName(const std::string& name,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  if (!IsPresent()) {
    error_callback.Run();
    return;
  }

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->alias.Set(
          name,
          base::Bind(&BluetoothAdapterChromeOS::OnPropertyChangeCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothAdapterChromeOS::IsInitialized() const {
  return true;
}

bool BluetoothAdapterChromeOS::IsPresent() const {
  return !dbus_is_shutdown_ && !object_path_.value().empty();
}

bool BluetoothAdapterChromeOS::IsPowered() const {
  if (!IsPresent())
    return false;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->powered.value();
}

void BluetoothAdapterChromeOS::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!IsPresent()) {
    error_callback.Run();
    return;
  }

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->powered.Set(
          powered,
          base::Bind(&BluetoothAdapterChromeOS::OnPropertyChangeCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothAdapterChromeOS::IsDiscoverable() const {
  if (!IsPresent())
    return false;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->discoverable.value();
}

void BluetoothAdapterChromeOS::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!IsPresent()) {
    error_callback.Run();
    return;
  }

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->discoverable.Set(
          discoverable,
          base::Bind(&BluetoothAdapterChromeOS::OnSetDiscoverable,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

bool BluetoothAdapterChromeOS::IsDiscovering() const {
  if (!IsPresent())
    return false;

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  return properties->discovering.value();
}

void BluetoothAdapterChromeOS::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  DCHECK(!dbus_is_shutdown_);
  VLOG(1) << object_path_.value() << ": Creating RFCOMM service: "
          << uuid.canonical_value();
  scoped_refptr<BluetoothSocketChromeOS> socket =
      BluetoothSocketChromeOS::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_);
  socket->Listen(this,
                 BluetoothSocketChromeOS::kRfcomm,
                 uuid,
                 options,
                 base::Bind(callback, socket),
                 error_callback);
}

void BluetoothAdapterChromeOS::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  DCHECK(!dbus_is_shutdown_);
  VLOG(1) << object_path_.value() << ": Creating L2CAP service: "
          << uuid.canonical_value();
  scoped_refptr<BluetoothSocketChromeOS> socket =
      BluetoothSocketChromeOS::CreateBluetoothSocket(
          ui_task_runner_, socket_thread_);
  socket->Listen(this,
                 BluetoothSocketChromeOS::kL2cap,
                 uuid,
                 options,
                 base::Bind(callback, socket),
                 error_callback);
}

void BluetoothAdapterChromeOS::RegisterAudioSink(
    const BluetoothAudioSink::Options& options,
    const device::BluetoothAdapter::AcquiredCallback& callback,
    const BluetoothAudioSink::ErrorCallback& error_callback) {
  VLOG(1) << "Registering audio sink";
  if (!this->IsPresent()) {
    error_callback.Run(BluetoothAudioSink::ERROR_INVALID_ADAPTER);
    return;
  }
  scoped_refptr<BluetoothAudioSinkChromeOS> audio_sink(
      new BluetoothAudioSinkChromeOS(this));
  audio_sink->Register(
      options, base::Bind(&BluetoothAdapterChromeOS::OnRegisterAudioSink,
                          weak_ptr_factory_.GetWeakPtr(), callback,
                          error_callback, audio_sink),
      error_callback);
}

void BluetoothAdapterChromeOS::RegisterAdvertisement(
    scoped_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const CreateAdvertisementErrorCallback& error_callback) {
  scoped_refptr<BluetoothAdvertisementChromeOS> advertisement(
      new BluetoothAdvertisementChromeOS(advertisement_data.Pass(), this));
  advertisement->Register(base::Bind(callback, advertisement), error_callback);
}

void BluetoothAdapterChromeOS::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  // Check if any device is using the pairing delegate.
  // If so, clear the pairing context which will make any responses no-ops.
  for (DevicesMap::iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    BluetoothDeviceChromeOS* device_chromeos =
        static_cast<BluetoothDeviceChromeOS*>(iter->second);

    BluetoothPairingChromeOS* pairing = device_chromeos->GetPairing();
    if (pairing && pairing->GetPairingDelegate() == pairing_delegate)
      device_chromeos->EndPairing();
  }
}

void BluetoothAdapterChromeOS::AdapterAdded(
    const dbus::ObjectPath& object_path) {
  // Set the adapter to the newly added adapter only if no adapter is present.
  if (!IsPresent())
    SetAdapter(object_path);
}

void BluetoothAdapterChromeOS::AdapterRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path == object_path_)
    RemoveAdapter();
}

void BluetoothAdapterChromeOS::AdapterPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;
  DCHECK(IsPresent());

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  if (property_name == properties->powered.name()) {
    PoweredChanged(properties->powered.value());
  } else if (property_name == properties->discoverable.name()) {
    DiscoverableChanged(properties->discoverable.value());
  } else if (property_name == properties->discovering.name()) {
    DiscoveringChanged(properties->discovering.value());
  }
}

void BluetoothAdapterChromeOS::DeviceAdded(
  const dbus::ObjectPath& object_path) {
  DCHECK(DBusThreadManager::Get());
  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path);
  if (!properties || properties->adapter.value() != object_path_)
    return;
  DCHECK(IsPresent());

  BluetoothDeviceChromeOS* device_chromeos =
      new BluetoothDeviceChromeOS(this,
                                  object_path,
                                  ui_task_runner_,
                                  socket_thread_);
  DCHECK(devices_.find(device_chromeos->GetAddress()) == devices_.end());

  devices_[device_chromeos->GetAddress()] = device_chromeos;

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceAdded(this, device_chromeos));
}

void BluetoothAdapterChromeOS::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
  for (DevicesMap::iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    BluetoothDeviceChromeOS* device_chromeos =
        static_cast<BluetoothDeviceChromeOS*>(iter->second);
    if (device_chromeos->object_path() == object_path) {
      devices_.erase(iter);

      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceRemoved(this, device_chromeos));
      delete device_chromeos;
      return;
    }
  }
}

void BluetoothAdapterChromeOS::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceChromeOS* device_chromeos = GetDeviceWithPath(object_path);
  if (!device_chromeos)
    return;

  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path);

  if (property_name == properties->bluetooth_class.name() ||
      property_name == properties->address.name() ||
      property_name == properties->alias.name() ||
      property_name == properties->paired.name() ||
      property_name == properties->trusted.name() ||
      property_name == properties->connected.name() ||
      property_name == properties->uuids.name() ||
      property_name == properties->rssi.name() ||
      property_name == properties->tx_power.name()) {
    NotifyDeviceChanged(device_chromeos);
  }

  // When a device becomes paired, mark it as trusted so that the user does
  // not need to approve every incoming connection
  if (property_name == properties->paired.name() &&
      properties->paired.value() && !properties->trusted.value()) {
    device_chromeos->SetTrusted();
  }

  // UMA connection counting
  if (property_name == properties->connected.name()) {
    // PlayStation joystick tries to reconnect after disconnection from USB.
    // If it is still not trusted, set it, so it becomes available on the
    // list of known devices.
    if (properties->connected.value() && device_chromeos->IsTrustable() &&
        !properties->trusted.value())
      device_chromeos->SetTrusted();

    int count = 0;

    for (DevicesMap::iterator iter = devices_.begin();
         iter != devices_.end(); ++iter) {
      if (iter->second->IsPaired() && iter->second->IsConnected())
        ++count;
    }

    UMA_HISTOGRAM_COUNTS_100("Bluetooth.ConnectedDeviceCount", count);
  }
}

void BluetoothAdapterChromeOS::InputPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  BluetoothDeviceChromeOS* device_chromeos = GetDeviceWithPath(object_path);
  if (!device_chromeos)
    return;

  BluetoothInputClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothInputClient()->
          GetProperties(object_path);

  // Properties structure can be removed, which triggers a change in the
  // BluetoothDevice::IsConnectable() property, as does a change in the
  // actual reconnect_mode property.
  if (!properties || property_name == properties->reconnect_mode.name()) {
    NotifyDeviceChanged(device_chromeos);
  }
}

void BluetoothAdapterChromeOS::Released() {
  VLOG(1) << "Release";
  if (!IsPresent())
    return;
  DCHECK(agent_.get());

  // Called after we unregister the pairing agent, e.g. when changing I/O
  // capabilities. Nothing much to be done right now.
}

void BluetoothAdapterChromeOS::RequestPinCode(
    const dbus::ObjectPath& device_path,
    const PinCodeCallback& callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestPinCode";

  BluetoothPairingChromeOS* pairing = GetPairing(device_path);
  if (!pairing) {
    callback.Run(REJECTED, "");
    return;
  }

  pairing->RequestPinCode(callback);
}

void BluetoothAdapterChromeOS::DisplayPinCode(
    const dbus::ObjectPath& device_path,
    const std::string& pincode) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": DisplayPinCode: " << pincode;

  BluetoothPairingChromeOS* pairing = GetPairing(device_path);
  if (!pairing)
    return;

  pairing->DisplayPinCode(pincode);
}

void BluetoothAdapterChromeOS::RequestPasskey(
    const dbus::ObjectPath& device_path,
    const PasskeyCallback& callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestPasskey";

  BluetoothPairingChromeOS* pairing = GetPairing(device_path);
  if (!pairing) {
    callback.Run(REJECTED, 0);
    return;
  }

  pairing->RequestPasskey(callback);
}

void BluetoothAdapterChromeOS::DisplayPasskey(
    const dbus::ObjectPath& device_path,
    uint32 passkey,
    uint16 entered) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": DisplayPasskey: " << passkey
          << " (" << entered << " entered)";

  BluetoothPairingChromeOS* pairing = GetPairing(device_path);
  if (!pairing)
    return;

  if (entered == 0)
    pairing->DisplayPasskey(passkey);

  pairing->KeysEntered(entered);
}

void BluetoothAdapterChromeOS::RequestConfirmation(
    const dbus::ObjectPath& device_path,
    uint32 passkey,
    const ConfirmationCallback& callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestConfirmation: " << passkey;

  BluetoothPairingChromeOS* pairing = GetPairing(device_path);
  if (!pairing) {
    callback.Run(REJECTED);
    return;
  }

  pairing->RequestConfirmation(passkey, callback);
}

void BluetoothAdapterChromeOS::RequestAuthorization(
    const dbus::ObjectPath& device_path,
    const ConfirmationCallback& callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": RequestAuthorization";

  BluetoothPairingChromeOS* pairing = GetPairing(device_path);
  if (!pairing) {
    callback.Run(REJECTED);
    return;
  }

  pairing->RequestAuthorization(callback);
}

void BluetoothAdapterChromeOS::AuthorizeService(
    const dbus::ObjectPath& device_path,
    const std::string& uuid,
    const ConfirmationCallback& callback) {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << device_path.value() << ": AuthorizeService: " << uuid;

  BluetoothDeviceChromeOS* device_chromeos = GetDeviceWithPath(device_path);
  if (!device_chromeos) {
    callback.Run(CANCELLED);
    return;
  }

  // We always set paired devices to Trusted, so the only reason that this
  // method call would ever be called is in the case of a race condition where
  // our "Set('Trusted', true)" method call is still pending in the Bluetooth
  // daemon because it's busy handling the incoming connection.
  if (device_chromeos->IsPaired()) {
    callback.Run(SUCCESS);
    return;
  }

  // TODO(keybuk): reject service authorizations when not paired, determine
  // whether this is acceptable long-term.
  LOG(WARNING) << "Rejecting service connection from unpaired device "
               << device_chromeos->GetAddress() << " for UUID " << uuid;
  callback.Run(REJECTED);
}

void BluetoothAdapterChromeOS::Cancel() {
  DCHECK(IsPresent());
  DCHECK(agent_.get());
  VLOG(1) << "Cancel";
}

void BluetoothAdapterChromeOS::OnRegisterAgent() {
  VLOG(1) << "Pairing agent registered, requesting to be made default";

  DBusThreadManager::Get()->GetBluetoothAgentManagerClient()->
      RequestDefaultAgent(
          dbus::ObjectPath(kAgentPath),
          base::Bind(&BluetoothAdapterChromeOS::OnRequestDefaultAgent,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BluetoothAdapterChromeOS::OnRequestDefaultAgentError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothAdapterChromeOS::OnRegisterAgentError(
    const std::string& error_name,
    const std::string& error_message) {
  // Our agent being already registered isn't an error.
  if (error_name == bluetooth_agent_manager::kErrorAlreadyExists)
    return;

  LOG(WARNING) << ": Failed to register pairing agent: "
               << error_name << ": " << error_message;
}

void BluetoothAdapterChromeOS::OnRequestDefaultAgent() {
  VLOG(1) << "Pairing agent now default";
}

void BluetoothAdapterChromeOS::OnRequestDefaultAgentError(
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << ": Failed to make pairing agent default: "
               << error_name << ": " << error_message;
}

void BluetoothAdapterChromeOS::OnRegisterAudioSink(
    const device::BluetoothAdapter::AcquiredCallback& callback,
    const device::BluetoothAudioSink::ErrorCallback& error_callback,
    scoped_refptr<BluetoothAudioSink> audio_sink) {
  if (!IsPresent()) {
    VLOG(1) << "Failed to register audio sink, adapter not present";
    error_callback.Run(BluetoothAudioSink::ERROR_INVALID_ADAPTER);
    return;
  }
  DCHECK(audio_sink.get());
  callback.Run(audio_sink);
}

BluetoothDeviceChromeOS*
BluetoothAdapterChromeOS::GetDeviceWithPath(
    const dbus::ObjectPath& object_path) {
  if (!IsPresent())
    return nullptr;

  for (DevicesMap::iterator iter = devices_.begin(); iter != devices_.end();
       ++iter) {
    BluetoothDeviceChromeOS* device_chromeos =
        static_cast<BluetoothDeviceChromeOS*>(iter->second);
    if (device_chromeos->object_path() == object_path)
      return device_chromeos;
  }

  return nullptr;
}

BluetoothPairingChromeOS* BluetoothAdapterChromeOS::GetPairing(
    const dbus::ObjectPath& object_path) {
  DCHECK(IsPresent());
  BluetoothDeviceChromeOS* device_chromeos = GetDeviceWithPath(object_path);
  if (!device_chromeos) {
    LOG(WARNING) << "Pairing Agent request for unknown device: "
                 << object_path.value();
    return nullptr;
  }

  BluetoothPairingChromeOS* pairing = device_chromeos->GetPairing();
  if (pairing)
    return pairing;

  // The device doesn't have its own pairing context, so this is an incoming
  // pairing request that should use our best default delegate (if we have one).
  BluetoothDevice::PairingDelegate* pairing_delegate = DefaultPairingDelegate();
  if (!pairing_delegate)
    return nullptr;

  return device_chromeos->BeginPairing(pairing_delegate);
}

void BluetoothAdapterChromeOS::SetAdapter(const dbus::ObjectPath& object_path) {
  DCHECK(!IsPresent());
  DCHECK(!dbus_is_shutdown_);
  object_path_ = object_path;

  VLOG(1) << object_path_.value() << ": using adapter.";

  VLOG(1) << "Registering pairing agent";
  DBusThreadManager::Get()->GetBluetoothAgentManagerClient()->
      RegisterAgent(
          dbus::ObjectPath(kAgentPath),
          bluetooth_agent_manager::kKeyboardDisplayCapability,
          base::Bind(&BluetoothAdapterChromeOS::OnRegisterAgent,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BluetoothAdapterChromeOS::OnRegisterAgentError,
                     weak_ptr_factory_.GetWeakPtr()));

  SetDefaultAdapterName();

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  PresentChanged(true);

  if (properties->powered.value())
    PoweredChanged(true);
  if (properties->discoverable.value())
    DiscoverableChanged(true);
  if (properties->discovering.value())
    DiscoveringChanged(true);

  std::vector<dbus::ObjectPath> device_paths =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetDevicesForAdapter(object_path_);

  for (std::vector<dbus::ObjectPath>::iterator iter = device_paths.begin();
       iter != device_paths.end(); ++iter) {
    DeviceAdded(*iter);
  }
}

void BluetoothAdapterChromeOS::SetDefaultAdapterName() {
  DCHECK(IsPresent());
  std::string board = base::SysInfo::GetLsbReleaseBoard();
  std::string alias;
  if (board.substr(0, 6) == "stumpy") {
    alias = "Chromebox";
  } else if (board.substr(0, 4) == "link") {
    alias = "Chromebook Pixel";
  } else {
    alias = "Chromebook";
  }

  SetName(alias, base::Bind(&base::DoNothing), base::Bind(&base::DoNothing));
}

void BluetoothAdapterChromeOS::RemoveAdapter() {
  DCHECK(IsPresent());
  VLOG(1) << object_path_.value() << ": adapter removed.";

  BluetoothAdapterClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothAdapterClient()->
          GetProperties(object_path_);

  object_path_ = dbus::ObjectPath("");

  if (properties->powered.value())
    PoweredChanged(false);
  if (properties->discoverable.value())
    DiscoverableChanged(false);
  if (properties->discovering.value())
    DiscoveringChanged(false);

  // Copy the devices list here and clear the original so that when we
  // send DeviceRemoved(), GetDevices() returns no devices.
  DevicesMap devices = devices_;
  devices_.clear();

  for (DevicesMap::iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceRemoved(this, iter->second));
    delete iter->second;
  }

  PresentChanged(false);
}

void BluetoothAdapterChromeOS::PoweredChanged(bool powered) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPoweredChanged(this, powered));
}

void BluetoothAdapterChromeOS::DiscoverableChanged(bool discoverable) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoverableChanged(this, discoverable));
}

void BluetoothAdapterChromeOS::DiscoveringChanged(
    bool discovering) {
  // If the adapter stopped discovery due to a reason other than a request by
  // us, reset the count to 0.
  VLOG(1) << "Discovering changed: " << discovering;
  if (!discovering && !discovery_request_pending_ &&
      num_discovery_sessions_ > 0) {
    VLOG(1) << "Marking sessions as inactive.";
    num_discovery_sessions_ = 0;
    MarkDiscoverySessionsAsInactive();
  }
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterDiscoveringChanged(this, discovering));
}

void BluetoothAdapterChromeOS::PresentChanged(bool present) {
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    AdapterPresentChanged(this, present));
}

void BluetoothAdapterChromeOS::NotifyDeviceChanged(
    BluetoothDeviceChromeOS* device) {
  DCHECK(device);
  DCHECK(device->adapter_ == this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceChanged(this, device));
}

void BluetoothAdapterChromeOS::NotifyGattServiceAdded(
    BluetoothRemoteGattServiceChromeOS* service) {
  DCHECK_EQ(service->GetAdapter(), this);
  DCHECK_EQ(
      static_cast<BluetoothDeviceChromeOS*>(service->GetDevice())->adapter_,
      this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattServiceAdded(this, service->GetDevice(), service));
}

void BluetoothAdapterChromeOS::NotifyGattServiceRemoved(
    BluetoothRemoteGattServiceChromeOS* service) {
  DCHECK_EQ(service->GetAdapter(), this);
  DCHECK_EQ(
      static_cast<BluetoothDeviceChromeOS*>(service->GetDevice())->adapter_,
      this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattServiceRemoved(this, service->GetDevice(), service));
}

void BluetoothAdapterChromeOS::NotifyGattServiceChanged(
    BluetoothRemoteGattServiceChromeOS* service) {
  DCHECK_EQ(service->GetAdapter(), this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattServiceChanged(this, service));
}

void BluetoothAdapterChromeOS::NotifyGattDiscoveryComplete(
    BluetoothRemoteGattServiceChromeOS* service) {
  DCHECK_EQ(service->GetAdapter(), this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattDiscoveryCompleteForService(this, service));
}

void BluetoothAdapterChromeOS::NotifyGattCharacteristicAdded(
    BluetoothRemoteGattCharacteristicChromeOS* characteristic) {
  DCHECK_EQ(static_cast<BluetoothRemoteGattServiceChromeOS*>(
                characteristic->GetService())->GetAdapter(),
            this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattCharacteristicAdded(this, characteristic));
}

void BluetoothAdapterChromeOS::NotifyGattCharacteristicRemoved(
    BluetoothRemoteGattCharacteristicChromeOS* characteristic) {
  DCHECK_EQ(static_cast<BluetoothRemoteGattServiceChromeOS*>(
                characteristic->GetService())->GetAdapter(),
            this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattCharacteristicRemoved(this, characteristic));
}

void BluetoothAdapterChromeOS::NotifyGattDescriptorAdded(
    BluetoothRemoteGattDescriptorChromeOS* descriptor) {
  DCHECK_EQ(static_cast<BluetoothRemoteGattServiceChromeOS*>(
                descriptor->GetCharacteristic()->GetService())->GetAdapter(),
            this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattDescriptorAdded(this, descriptor));
}

void BluetoothAdapterChromeOS::NotifyGattDescriptorRemoved(
    BluetoothRemoteGattDescriptorChromeOS* descriptor) {
  DCHECK_EQ(static_cast<BluetoothRemoteGattServiceChromeOS*>(
                descriptor->GetCharacteristic()->GetService())->GetAdapter(),
            this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattDescriptorRemoved(this, descriptor));
}

void BluetoothAdapterChromeOS::NotifyGattCharacteristicValueChanged(
    BluetoothRemoteGattCharacteristicChromeOS* characteristic,
    const std::vector<uint8>& value) {
  DCHECK_EQ(static_cast<BluetoothRemoteGattServiceChromeOS*>(
                characteristic->GetService())->GetAdapter(),
            this);

  FOR_EACH_OBSERVER(
      BluetoothAdapter::Observer,
      observers_,
      GattCharacteristicValueChanged(this, characteristic, value));
}

void BluetoothAdapterChromeOS::NotifyGattDescriptorValueChanged(
    BluetoothRemoteGattDescriptorChromeOS* descriptor,
    const std::vector<uint8>& value) {
  DCHECK_EQ(static_cast<BluetoothRemoteGattServiceChromeOS*>(
                descriptor->GetCharacteristic()->GetService())->GetAdapter(),
            this);

  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    GattDescriptorValueChanged(this, descriptor, value));
}

void BluetoothAdapterChromeOS::UseProfile(
    const BluetoothUUID& uuid,
    const dbus::ObjectPath& device_path,
    const BluetoothProfileManagerClient::Options& options,
    BluetoothProfileServiceProvider::Delegate* delegate,
    const ProfileRegisteredCallback& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(delegate);

  if (!IsPresent()) {
    VLOG(2) << "Adapter not present, erroring out";
    error_callback.Run("Adapter not present");
    return;
  }

  if (profiles_.find(uuid) != profiles_.end()) {
    // TODO(jamuraa) check that the options are the same and error when they are
    // not.
    SetProfileDelegate(uuid, device_path, delegate, success_callback,
                       error_callback);
    return;
  }

  if (profile_queues_.find(uuid) == profile_queues_.end()) {
    BluetoothAdapterProfileChromeOS::Register(
        uuid, options,
        base::Bind(&BluetoothAdapterChromeOS::OnRegisterProfile, this, uuid),
        base::Bind(&BluetoothAdapterChromeOS::OnRegisterProfileError, this,
                   uuid));

    profile_queues_[uuid] = new std::vector<RegisterProfileCompletionPair>();
  }

  profile_queues_[uuid]->push_back(std::make_pair(
      base::Bind(&BluetoothAdapterChromeOS::SetProfileDelegate, this, uuid,
                 device_path, delegate, success_callback, error_callback),
      error_callback));
}

void BluetoothAdapterChromeOS::ReleaseProfile(
    const dbus::ObjectPath& device_path,
    BluetoothAdapterProfileChromeOS* profile) {
  VLOG(2) << "Releasing Profile: " << profile->uuid().canonical_value()
          << " from " << device_path.value();
  profile->RemoveDelegate(
      device_path, base::Bind(&BluetoothAdapterChromeOS::RemoveProfile,
                              weak_ptr_factory_.GetWeakPtr(), profile->uuid()));
}

void BluetoothAdapterChromeOS::RemoveProfile(const BluetoothUUID& uuid) {
  VLOG(2) << "Remove Profile: " << uuid.canonical_value();

  if (profiles_.find(uuid) != profiles_.end()) {
    delete profiles_[uuid];
    profiles_.erase(uuid);
  }
}

void BluetoothAdapterChromeOS::OnRegisterProfile(
    const BluetoothUUID& uuid,
    scoped_ptr<BluetoothAdapterProfileChromeOS> profile) {
  profiles_[uuid] = profile.release();

  if (profile_queues_.find(uuid) == profile_queues_.end())
    return;

  for (auto& it : *profile_queues_[uuid])
    it.first.Run();
  delete profile_queues_[uuid];
  profile_queues_.erase(uuid);
}

void BluetoothAdapterChromeOS::SetProfileDelegate(
    const BluetoothUUID& uuid,
    const dbus::ObjectPath& device_path,
    BluetoothProfileServiceProvider::Delegate* delegate,
    const ProfileRegisteredCallback& success_callback,
    const ErrorCompletionCallback& error_callback) {
  if (profiles_.find(uuid) == profiles_.end()) {
    error_callback.Run("Cannot find profile!");
    return;
  }

  if (profiles_[uuid]->SetDelegate(device_path, delegate)) {
    success_callback.Run(profiles_[uuid]);
    return;
  }
  // Already set
  error_callback.Run(bluetooth_agent_manager::kErrorAlreadyExists);
}

void BluetoothAdapterChromeOS::OnRegisterProfileError(
    const BluetoothUUID& uuid,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(2) << object_path_.value() << ": Failed to register profile: "
          << error_name << ": " << error_message;
  if (profile_queues_.find(uuid) == profile_queues_.end())
    return;

  for (auto& it : *profile_queues_[uuid])
    it.second.Run(error_message);

  delete profile_queues_[uuid];
  profile_queues_.erase(uuid);
}

void BluetoothAdapterChromeOS::OnSetDiscoverable(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (!IsPresent()) {
    error_callback.Run();
    return;
  }

  // Set the discoverable_timeout property to zero so the adapter remains
  // discoverable forever.
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      GetProperties(object_path_)->discoverable_timeout.Set(
          0,
          base::Bind(&BluetoothAdapterChromeOS::OnPropertyChangeCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     error_callback));
}

void BluetoothAdapterChromeOS::OnPropertyChangeCompleted(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (IsPresent() && success) {
    callback.Run();
  } else {
    error_callback.Run();
  }
}

void BluetoothAdapterChromeOS::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!IsPresent()) {
    error_callback.Run();
    return;
  }
  VLOG(1) << __func__;
  if (discovery_request_pending_) {
    // The pending request is either to stop a previous session or to start a
    // new one. Either way, queue this one.
    DCHECK(num_discovery_sessions_ == 1 || num_discovery_sessions_ == 0);
    VLOG(1) << "Pending request to start/stop device discovery. Queueing "
            << "request to start a new discovery session.";
    discovery_request_queue_.push(
        std::make_tuple(discovery_filter, callback, error_callback));
    return;
  }

  // The adapter is already discovering.
  if (num_discovery_sessions_ > 0) {
    DCHECK(IsDiscovering());
    DCHECK(!discovery_request_pending_);
    num_discovery_sessions_++;
    SetDiscoveryFilter(BluetoothDiscoveryFilter::Merge(
                           GetMergedDiscoveryFilter().get(), discovery_filter),
                       callback, error_callback);
    return;
  }

  // There are no active discovery sessions.
  DCHECK_EQ(num_discovery_sessions_, 0);

  if (discovery_filter) {
    discovery_request_pending_ = true;

    scoped_ptr<BluetoothDiscoveryFilter> df(new BluetoothDiscoveryFilter(
        BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL));
    df->CopyFrom(*discovery_filter);
    SetDiscoveryFilter(
        df.Pass(),
        base::Bind(&BluetoothAdapterChromeOS::OnPreSetDiscoveryFilter,
                   weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
        base::Bind(&BluetoothAdapterChromeOS::OnPreSetDiscoveryFilterError,
                   weak_ptr_factory_.GetWeakPtr(), callback, error_callback));
    return;
  } else {
    current_filter_.reset();
  }

  // This is the first request to start device discovery.
  discovery_request_pending_ = true;
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->StartDiscovery(
      object_path_,
      base::Bind(&BluetoothAdapterChromeOS::OnStartDiscovery,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
      base::Bind(&BluetoothAdapterChromeOS::OnStartDiscoveryError,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback));
}

void BluetoothAdapterChromeOS::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!IsPresent()) {
    error_callback.Run();
    return;
  }

  VLOG(1) << __func__;
  // There are active sessions other than the one currently being removed.
  if (num_discovery_sessions_ > 1) {
    DCHECK(IsDiscovering());
    DCHECK(!discovery_request_pending_);
    num_discovery_sessions_--;

    SetDiscoveryFilter(GetMergedDiscoveryFilterMasked(discovery_filter),
                       callback, error_callback);
    return;
  }

  // If there is a pending request to BlueZ, then queue this request.
  if (discovery_request_pending_) {
    VLOG(1) << "Pending request to start/stop device discovery. Queueing "
            << "request to stop discovery session.";
    error_callback.Run();
    return;
  }

  // There are no active sessions. Return error.
  if (num_discovery_sessions_ == 0) {
    // TODO(armansito): This should never happen once we have the
    // DiscoverySession API. Replace this case with an assert once it's
    // the deprecated methods have been removed. (See crbug.com/3445008).
    VLOG(1) << "No active discovery sessions. Returning error.";
    error_callback.Run();
    return;
  }

  // There is exactly one active discovery session. Request BlueZ to stop
  // discovery.
  DCHECK_EQ(num_discovery_sessions_, 1);
  discovery_request_pending_ = true;
  DBusThreadManager::Get()->GetBluetoothAdapterClient()->
      StopDiscovery(
          object_path_,
          base::Bind(&BluetoothAdapterChromeOS::OnStopDiscovery,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback),
          base::Bind(&BluetoothAdapterChromeOS::OnStopDiscoveryError,
                     weak_ptr_factory_.GetWeakPtr(),
                     error_callback));
}

void BluetoothAdapterChromeOS::SetDiscoveryFilter(
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!IsPresent()) {
    error_callback.Run();
    return;
  }

  // If old and new filter are equal (null) then don't make request, just call
  // succes callback
  if (!current_filter_ && !discovery_filter.get()) {
    callback.Run();
    return;
  }

  // If old and new filter are not null and equal then don't make request, just
  // call succes callback
  if (current_filter_ && discovery_filter &&
      current_filter_->Equals(*discovery_filter)) {
    callback.Run();
    return;
  }

  current_filter_.reset(discovery_filter.release());

  chromeos::BluetoothAdapterClient::DiscoveryFilter dbus_discovery_filter;

  if (current_filter_.get()) {
    uint16_t pathloss;
    int16_t rssi;
    uint8_t transport;
    std::set<device::BluetoothUUID> uuids;

    if (current_filter_->GetPathloss(&pathloss))
      dbus_discovery_filter.pathloss.reset(new uint16_t(pathloss));

    if (current_filter_->GetRSSI(&rssi))
      dbus_discovery_filter.rssi.reset(new int16_t(rssi));

    transport = current_filter_->GetTransport();
    if (transport == BluetoothDiscoveryFilter::Transport::TRANSPORT_LE) {
      dbus_discovery_filter.transport.reset(new std::string("le"));
    } else if (transport ==
               BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC) {
      dbus_discovery_filter.transport.reset(new std::string("bredr"));
    } else if (transport ==
               BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL) {
      dbus_discovery_filter.transport.reset(new std::string("auto"));
    }

    current_filter_->GetUUIDs(uuids);
    if (uuids.size()) {
      dbus_discovery_filter.uuids =
          scoped_ptr<std::vector<std::string>>(new std::vector<std::string>);

      for (const auto& it : uuids)
        dbus_discovery_filter.uuids.get()->push_back(it.value());
    }
  }

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->SetDiscoveryFilter(
      object_path_, dbus_discovery_filter,
      base::Bind(&BluetoothAdapterChromeOS::OnSetDiscoveryFilter,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
      base::Bind(&BluetoothAdapterChromeOS::OnSetDiscoveryFilterError,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback));
}

void BluetoothAdapterChromeOS::OnStartDiscovery(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // Report success on the original request and increment the count.
  VLOG(1) << __func__;
  DCHECK(discovery_request_pending_);
  DCHECK_EQ(num_discovery_sessions_, 0);
  discovery_request_pending_ = false;
  num_discovery_sessions_++;
  if (IsPresent()) {
    callback.Run();
  } else {
    error_callback.Run();
  }

  // Try to add a new discovery session for each queued request.
  ProcessQueuedDiscoveryRequests();
}

void BluetoothAdapterChromeOS::OnStartDiscoveryError(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to start discovery: "
               << error_name << ": " << error_message;

  // Failed to start discovery. This can only happen if the count is at 0.
  DCHECK_EQ(num_discovery_sessions_, 0);
  DCHECK(discovery_request_pending_);
  discovery_request_pending_ = false;

  // Discovery request may fail if discovery was previously initiated by Chrome,
  // but the session were invalidated due to the discovery state unexpectedly
  // changing to false and then back to true. In this case, report success.
  if (IsPresent() && error_name == bluetooth_device::kErrorInProgress &&
      IsDiscovering()) {
    VLOG(1) << "Discovery previously initiated. Reporting success.";
    num_discovery_sessions_++;
    callback.Run();
  } else {
    error_callback.Run();
  }

  // Try to add a new discovery session for each queued request.
  ProcessQueuedDiscoveryRequests();
}

void BluetoothAdapterChromeOS::OnStopDiscovery(const base::Closure& callback) {
  // Report success on the original request and decrement the count.
  VLOG(1) << __func__;
  DCHECK(discovery_request_pending_);
  DCHECK_EQ(num_discovery_sessions_, 1);
  discovery_request_pending_ = false;
  num_discovery_sessions_--;
  callback.Run();

  current_filter_.reset();

  // Try to add a new discovery session for each queued request.
  ProcessQueuedDiscoveryRequests();
}

void BluetoothAdapterChromeOS::OnStopDiscoveryError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value() << ": Failed to stop discovery: "
               << error_name << ": " << error_message;

  // Failed to stop discovery. This can only happen if the count is at 1.
  DCHECK(discovery_request_pending_);
  DCHECK_EQ(num_discovery_sessions_, 1);
  discovery_request_pending_ = false;
  error_callback.Run();

  // Try to add a new discovery session for each queued request.
  ProcessQueuedDiscoveryRequests();
}

void BluetoothAdapterChromeOS::OnPreSetDiscoveryFilter(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // This is the first request to start device discovery.
  DCHECK(discovery_request_pending_);
  DCHECK_EQ(num_discovery_sessions_, 0);

  DBusThreadManager::Get()->GetBluetoothAdapterClient()->StartDiscovery(
      object_path_,
      base::Bind(&BluetoothAdapterChromeOS::OnStartDiscovery,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
      base::Bind(&BluetoothAdapterChromeOS::OnStartDiscoveryError,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback));
}

void BluetoothAdapterChromeOS::OnPreSetDiscoveryFilterError(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to pre set discovery filter.";

  // Failed to start discovery. This can only happen if the count is at 0.
  DCHECK_EQ(num_discovery_sessions_, 0);
  DCHECK(discovery_request_pending_);
  discovery_request_pending_ = false;

  error_callback.Run();

  // Try to add a new discovery session for each queued request.
  ProcessQueuedDiscoveryRequests();
}

void BluetoothAdapterChromeOS::OnSetDiscoveryFilter(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // Report success on the original request and increment the count.
  VLOG(1) << __func__;
  if (IsPresent()) {
    callback.Run();
  } else {
    error_callback.Run();
  }
}

void BluetoothAdapterChromeOS::OnSetDiscoveryFilterError(
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << object_path_.value()
               << ": Failed to set discovery filter: " << error_name << ": "
               << error_message;

  error_callback.Run();

  // Try to add a new discovery session for each queued request.
  ProcessQueuedDiscoveryRequests();
}

void BluetoothAdapterChromeOS::ProcessQueuedDiscoveryRequests() {
  while (!discovery_request_queue_.empty()) {
    VLOG(1) << "Process queued discovery request.";
    DiscoveryParamTuple params = discovery_request_queue_.front();
    discovery_request_queue_.pop();
    AddDiscoverySession(std::get<0>(params), std::get<1>(params),
                        std::get<2>(params));

    // If the queued request resulted in a pending call, then let it
    // asynchonously process the remaining queued requests once the pending
    // call returns.
    if (discovery_request_pending_)
      return;
  }
}

}  // namespace chromeos
