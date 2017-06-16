// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattConnection;
using device::BluetoothGattDescriptor;
using device::BluetoothGattService;
using device::BluetoothGattNotifySession;
using device::BluetoothUUID;
using device::TestBluetoothAdapterObserver;

namespace chromeos {

namespace {

const BluetoothUUID kHeartRateMeasurementUUID(
    FakeBluetoothGattCharacteristicClient::kHeartRateMeasurementUUID);
const BluetoothUUID kBodySensorLocationUUID(
    FakeBluetoothGattCharacteristicClient::kBodySensorLocationUUID);
const BluetoothUUID kHeartRateControlPointUUID(
    FakeBluetoothGattCharacteristicClient::kHeartRateControlPointUUID);

// Compares GATT characteristic/descriptor values. Returns true, if the values
// are equal.
bool ValuesEqual(const std::vector<uint8>& value0,
                 const std::vector<uint8>& value1) {
  if (value0.size() != value1.size())
    return false;
  for (size_t i = 0; i < value0.size(); ++i)
    if (value0[i] != value1[i])
      return false;
  return true;
}

}  // namespace

class BluetoothGattChromeOSTest : public testing::Test {
 public:
  BluetoothGattChromeOSTest()
      : fake_bluetooth_gatt_service_client_(NULL),
        success_callback_count_(0),
        error_callback_count_(0) {
  }

  void SetUp() override {
    scoped_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    fake_bluetooth_device_client_ = new FakeBluetoothDeviceClient;
    fake_bluetooth_gatt_service_client_ =
        new FakeBluetoothGattServiceClient;
    fake_bluetooth_gatt_characteristic_client_ =
        new FakeBluetoothGattCharacteristicClient;
    fake_bluetooth_gatt_descriptor_client_ =
        new FakeBluetoothGattDescriptorClient;
    dbus_setter->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(
            fake_bluetooth_device_client_));
    dbus_setter->SetBluetoothGattServiceClient(
        scoped_ptr<BluetoothGattServiceClient>(
            fake_bluetooth_gatt_service_client_));
    dbus_setter->SetBluetoothGattCharacteristicClient(
        scoped_ptr<BluetoothGattCharacteristicClient>(
            fake_bluetooth_gatt_characteristic_client_));
    dbus_setter->SetBluetoothGattDescriptorClient(
        scoped_ptr<BluetoothGattDescriptorClient>(
            fake_bluetooth_gatt_descriptor_client_));
    dbus_setter->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(new FakeBluetoothAdapterClient));
    dbus_setter->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    dbus_setter->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));

    GetAdapter();

    adapter_->SetPowered(
        true,
        base::Bind(&base::DoNothing),
        base::Bind(&base::DoNothing));
    ASSERT_TRUE(adapter_->IsPowered());
  }

  void TearDown() override {
    adapter_ = NULL;
    update_sessions_.clear();
    gatt_conn_.reset();
    DBusThreadManager::Shutdown();
  }

  void GetAdapter() {
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothGattChromeOSTest::AdapterCallback,
                   base::Unretained(this)));
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
  }

  void SuccessCallback() {
    ++success_callback_count_;
  }

  void ValueCallback(const std::vector<uint8>& value) {
    ++success_callback_count_;
    last_read_value_ = value;
  }

  void GattConnectionCallback(scoped_ptr<BluetoothGattConnection> conn) {
    ++success_callback_count_;
    gatt_conn_ = conn.Pass();
  }

  void NotifySessionCallback(scoped_ptr<BluetoothGattNotifySession> session) {
    ++success_callback_count_;
    update_sessions_.push_back(session.release());
    QuitMessageLoop();
  }

  void ServiceErrorCallback(BluetoothGattService::GattErrorCode err) {
    ++error_callback_count_;
    last_service_error_ = err;
  }

  void ErrorCallback() {
    ++error_callback_count_;
  }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
  }

  void ConnectErrorCallback(BluetoothDevice::ConnectErrorCode error) {
    ++error_callback_count_;
  }

 protected:
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }

  base::MessageLoop message_loop_;

  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  FakeBluetoothGattServiceClient* fake_bluetooth_gatt_service_client_;
  FakeBluetoothGattCharacteristicClient*
      fake_bluetooth_gatt_characteristic_client_;
  FakeBluetoothGattDescriptorClient* fake_bluetooth_gatt_descriptor_client_;
  scoped_ptr<device::BluetoothGattConnection> gatt_conn_;
  ScopedVector<BluetoothGattNotifySession> update_sessions_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int success_callback_count_;
  int error_callback_count_;
  std::vector<uint8> last_read_value_;
  BluetoothGattService::GattErrorCode last_service_error_;
};

TEST_F(BluetoothGattChromeOSTest, GattConnection) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);
  ASSERT_FALSE(device->IsConnected());
  ASSERT_FALSE(gatt_conn_.get());
  ASSERT_EQ(0, success_callback_count_);
  ASSERT_EQ(0, error_callback_count_);

  device->CreateGattConnection(
      base::Bind(&BluetoothGattChromeOSTest::GattConnectionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(device->IsConnected());
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_TRUE(gatt_conn_->IsConnected());
  EXPECT_EQ(FakeBluetoothDeviceClient::kLowEnergyAddress,
            gatt_conn_->GetDeviceAddress());

  gatt_conn_->Disconnect(
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(gatt_conn_->IsConnected());

  device->CreateGattConnection(
      base::Bind(&BluetoothGattChromeOSTest::GattConnectionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(device->IsConnected());
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_TRUE(gatt_conn_->IsConnected());

  device->Disconnect(
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_FALSE(gatt_conn_->IsConnected());

  device->CreateGattConnection(
      base::Bind(&BluetoothGattChromeOSTest::GattConnectionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(5, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(gatt_conn_->IsConnected());

  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_TRUE(gatt_conn_.get());
  EXPECT_FALSE(gatt_conn_->IsConnected());
}

TEST_F(BluetoothGattChromeOSTest, GattServiceAddedAndRemoved) {
  // Create a fake LE device. We store the device pointer here because this is a
  // test. It's unsafe to do this in production as the device might get deleted.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.gatt_service_added_count());
  EXPECT_EQ(0, observer.gatt_service_removed_count());
  EXPECT_TRUE(observer.last_gatt_service_id().empty());
  EXPECT_FALSE(observer.last_gatt_service_uuid().IsValid());
  EXPECT_TRUE(device->GetGattServices().empty());

  // Expose the fake Heart Rate Service.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(1, observer.gatt_service_added_count());
  EXPECT_EQ(0, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid());

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_EQ(service, device->GetGattService(service->GetIdentifier()));

  EXPECT_EQ(observer.last_gatt_service_uuid(), service->GetUUID());

  // Hide the service.
  observer.last_gatt_service_uuid() = BluetoothUUID();
  observer.last_gatt_service_id().clear();
  fake_bluetooth_gatt_service_client_->HideHeartRateService();

  EXPECT_EQ(1, observer.gatt_service_added_count());
  EXPECT_EQ(1, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_TRUE(device->GetGattServices().empty());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid());

  EXPECT_EQ(NULL, device->GetGattService(observer.last_gatt_service_id()));

  // Expose the service again.
  observer.last_gatt_service_uuid() = BluetoothUUID();
  observer.last_gatt_service_id().clear();
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  EXPECT_EQ(2, observer.gatt_service_added_count());
  EXPECT_EQ(1, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_EQ(1U, device->GetGattServices().size());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid());

  // The object |service| points to should have been deallocated. |device|
  // should contain a brand new instance.
  service = device->GetGattService(observer.last_gatt_service_id());
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());

  EXPECT_EQ(observer.last_gatt_service_uuid(), service->GetUUID());

  // Remove the device. The observer should be notified of the removed service.
  // |device| becomes invalid after this.
  observer.last_gatt_service_uuid() = BluetoothUUID();
  observer.last_gatt_service_id().clear();
  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));

  EXPECT_EQ(2, observer.gatt_service_added_count());
  EXPECT_EQ(2, observer.gatt_service_removed_count());
  EXPECT_FALSE(observer.last_gatt_service_id().empty());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      observer.last_gatt_service_uuid());
  EXPECT_EQ(
      NULL, adapter_->GetDevice(FakeBluetoothDeviceClient::kLowEnergyAddress));
}

TEST_F(BluetoothGattChromeOSTest, GattCharacteristicAddedAndRemoved) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(0, observer.gatt_discovery_complete_count());
  EXPECT_EQ(0, observer.gatt_characteristic_added_count());
  EXPECT_EQ(0, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();

  // 3 characteristics should appear. Only 1 of the characteristics sends
  // value changed signals. Service changed should be fired once for
  // descriptor added.
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_discovery_complete_count());
  EXPECT_EQ(3, observer.gatt_characteristic_added_count());
  EXPECT_EQ(0, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  // Hide the characteristics. 3 removed signals should be received.
  fake_bluetooth_gatt_characteristic_client_->HideHeartRateCharacteristics();
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(3, observer.gatt_characteristic_added_count());
  EXPECT_EQ(3, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Re-expose the heart rate characteristics. We shouldn't get another
  // GattDiscoveryCompleteForService call, since the service thinks that
  // discovery is done. On the bluetoothd side, characteristics will be removed
  // only if the service will also be subsequently removed.
  fake_bluetooth_gatt_characteristic_client_->ExposeHeartRateCharacteristics(
      fake_bluetooth_gatt_service_client_->GetHeartRateServicePath());
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_discovery_complete_count());
  EXPECT_EQ(6, observer.gatt_characteristic_added_count());
  EXPECT_EQ(3, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  // Hide the service. All characteristics should disappear.
  fake_bluetooth_gatt_service_client_->HideHeartRateService();
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(6, observer.gatt_characteristic_added_count());
  EXPECT_EQ(6, observer.gatt_characteristic_removed_count());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
}

TEST_F(BluetoothGattChromeOSTest, GattDescriptorAddedAndRemoved) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_added_count());
  EXPECT_EQ(0, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();
  EXPECT_EQ(0, observer.gatt_service_changed_count());

  // Only the Heart Rate Measurement characteristic has a descriptor.
  EXPECT_EQ(1, observer.gatt_descriptor_added_count());
  EXPECT_EQ(0, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateControlPointPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());

  BluetoothGattDescriptor* descriptor = characteristic->GetDescriptors()[0];
  EXPECT_FALSE(descriptor->IsLocal());
  EXPECT_EQ(BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid(),
            descriptor->GetUUID());
  EXPECT_EQ(descriptor->GetUUID(), observer.last_gatt_descriptor_uuid());
  EXPECT_EQ(descriptor->GetIdentifier(), observer.last_gatt_descriptor_id());

  // Hide the descriptor.
  fake_bluetooth_gatt_descriptor_client_->HideDescriptor(
      dbus::ObjectPath(descriptor->GetIdentifier()));
  EXPECT_TRUE(characteristic->GetDescriptors().empty());
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_added_count());
  EXPECT_EQ(1, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  // Expose the descriptor again.
  observer.last_gatt_descriptor_id().clear();
  observer.last_gatt_descriptor_uuid() = BluetoothUUID();
  fake_bluetooth_gatt_descriptor_client_->ExposeDescriptor(
      dbus::ObjectPath(characteristic->GetIdentifier()),
      FakeBluetoothGattDescriptorClient::
          kClientCharacteristicConfigurationUUID);
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());
  EXPECT_EQ(2, observer.gatt_descriptor_added_count());
  EXPECT_EQ(1, observer.gatt_descriptor_removed_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());

  descriptor = characteristic->GetDescriptors()[0];
  EXPECT_FALSE(descriptor->IsLocal());
  EXPECT_EQ(BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid(),
            descriptor->GetUUID());
  EXPECT_EQ(descriptor->GetUUID(), observer.last_gatt_descriptor_uuid());
  EXPECT_EQ(descriptor->GetIdentifier(), observer.last_gatt_descriptor_id());
}

TEST_F(BluetoothGattChromeOSTest, AdapterAddedAfterGattService) {
  // This unit test tests that all remote GATT objects are created for D-Bus
  // objects that were already exposed.
  adapter_ = NULL;
  ASSERT_FALSE(device::BluetoothAdapterFactory::HasSharedInstanceForTesting());

  // Create the fake D-Bus objects.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  while (!fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible())
    base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_bluetooth_gatt_service_client_->IsHeartRateVisible());
  ASSERT_TRUE(fake_bluetooth_gatt_characteristic_client_->IsHeartRateVisible());

  // Create the adapter. This should create all the GATT objects.
  GetAdapter();
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);
  EXPECT_EQ(1U, device->GetGattServices().size());

  BluetoothGattService* service = device->GetGattServices()[0];
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsLocal());
  EXPECT_TRUE(service->IsPrimary());
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattServiceClient::kHeartRateServiceUUID),
      service->GetUUID());
  EXPECT_EQ(service, device->GetGattServices()[0]);
  EXPECT_EQ(service, device->GetGattService(service->GetIdentifier()));
  EXPECT_FALSE(service->IsLocal());
  EXPECT_EQ(3U, service->GetCharacteristics().size());

  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattCharacteristicClient::
          kBodySensorLocationUUID),
      characteristic->GetUUID());
  EXPECT_FALSE(characteristic->IsLocal());
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateControlPointPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattCharacteristicClient::
          kHeartRateControlPointUUID),
      characteristic->GetUUID());
  EXPECT_FALSE(characteristic->IsLocal());
  EXPECT_TRUE(characteristic->GetDescriptors().empty());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      BluetoothUUID(FakeBluetoothGattCharacteristicClient::
          kHeartRateMeasurementUUID),
      characteristic->GetUUID());
  EXPECT_FALSE(characteristic->IsLocal());
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());

  BluetoothGattDescriptor* descriptor = characteristic->GetDescriptors()[0];
  ASSERT_TRUE(descriptor);
  EXPECT_EQ(BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid(),
            descriptor->GetUUID());
  EXPECT_FALSE(descriptor->IsLocal());
}

TEST_F(BluetoothGattChromeOSTest, GattCharacteristicValue) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();

  // Issue write request to non-writable characteristics.
  observer.Reset();

  std::vector<uint8> write_value;
  write_value.push_back(0x01);
  BluetoothGattCharacteristic* characteristic =
      service->GetCharacteristic(fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetHeartRateMeasurementPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateMeasurementUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_NOT_SUPPORTED,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetBodySensorLocationPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_NOT_PERMITTED,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue write request to writable characteristic. The "Body Sensor Location"
  // characteristic does not send notifications and WriteValue does not result
  // in a CharacteristicValueChanged event, thus no such event should be
  // received.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateControlPointPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetHeartRateControlPointPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kHeartRateControlPointUUID, characteristic->GetUUID());
  characteristic->WriteRemoteCharacteristic(
      write_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
  EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue some invalid write requests to the characteristic.
  // The value should still not change.

  std::vector<uint8> invalid_write_length;
  invalid_write_length.push_back(0x01);
  invalid_write_length.push_back(0x00);
  characteristic->WriteRemoteCharacteristic(
      invalid_write_length,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(3, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_INVALID_LENGTH,
            last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  std::vector<uint8> invalid_write_value;
  invalid_write_value.push_back(0x02);
  characteristic->WriteRemoteCharacteristic(
      invalid_write_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_FAILED, last_service_error_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Issue a read request. A successful read results in a
  // CharacteristicValueChanged notification.
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(fake_bluetooth_gatt_characteristic_client_->
                GetBodySensorLocationPath().value(),
            characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(ValuesEqual(characteristic->GetValue(), last_read_value_));

  // Test long-running actions.
  fake_bluetooth_gatt_characteristic_client_->SetExtraProcessing(1);
  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(
      fake_bluetooth_gatt_characteristic_client_->GetBodySensorLocationPath()
          .value(),
      characteristic->GetIdentifier());
  EXPECT_EQ(kBodySensorLocationUUID, characteristic->GetUUID());
  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));

  // Callback counts shouldn't change, this one will be delayed until after
  // tne next one.
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(4, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());

  // Next read should error because IN_PROGRESS
  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(5, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_IN_PROGRESS, last_service_error_);

  // But previous call finished.
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(2, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(ValuesEqual(characteristic->GetValue(), last_read_value_));
  fake_bluetooth_gatt_characteristic_client_->SetExtraProcessing(0);

  // Test unauthorized actions.
  fake_bluetooth_gatt_characteristic_client_->SetAuthorized(false);
  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(6, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED,
            last_service_error_);
  EXPECT_EQ(2, observer.gatt_characteristic_value_changed_count());
  fake_bluetooth_gatt_characteristic_client_->SetAuthorized(true);

  // Test unauthenticated / needs login.
  fake_bluetooth_gatt_characteristic_client_->SetAuthenticated(false);
  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(7, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_NOT_PAIRED, last_service_error_);
  EXPECT_EQ(2, observer.gatt_characteristic_value_changed_count());
  fake_bluetooth_gatt_characteristic_client_->SetAuthenticated(true);
}

TEST_F(BluetoothGattChromeOSTest, GattCharacteristicProperties) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();

  BluetoothGattCharacteristic *characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetBodySensorLocationPath().value());
  EXPECT_EQ(BluetoothGattCharacteristic::PROPERTY_READ,
            characteristic->GetProperties());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateControlPointPath().value());
  EXPECT_EQ(BluetoothGattCharacteristic::PROPERTY_WRITE,
            characteristic->GetProperties());

  characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  EXPECT_EQ(BluetoothGattCharacteristic::PROPERTY_NOTIFY,
            characteristic->GetProperties());
}

TEST_F(BluetoothGattChromeOSTest, GattDescriptorValue) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(0, observer.gatt_discovery_complete_count());
  EXPECT_EQ(0, observer.gatt_descriptor_value_changed_count());
  EXPECT_TRUE(service->GetCharacteristics().empty());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_discovery_complete_count());

  // Only the Heart Rate Measurement characteristic has a descriptor.
  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->
          GetHeartRateMeasurementPath().value());
  ASSERT_TRUE(characteristic);
  EXPECT_EQ(1U, characteristic->GetDescriptors().size());
  EXPECT_FALSE(characteristic->IsNotifying());

  BluetoothGattDescriptor* descriptor = characteristic->GetDescriptors()[0];
  EXPECT_FALSE(descriptor->IsLocal());
  EXPECT_EQ(BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid(),
            descriptor->GetUUID());

  std::vector<uint8_t> desc_value = {0x00, 0x00};

  /* The cached value will be empty until the first read request */
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_TRUE(descriptor->GetValue().empty());

  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(last_read_value_.empty());

  // Read value. GattDescriptorValueChanged event will be sent after a
  // successful read.
  descriptor->ReadRemoteDescriptor(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_TRUE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_value_changed_count());

  // Write value. Writes to this descriptor will fail.
  desc_value[0] = 0x03;
  descriptor->WriteRemoteDescriptor(
      desc_value,
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothGattService::GATT_ERROR_NOT_PERMITTED,
            last_service_error_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_value_changed_count());

  // Read value. The value should remain unchanged.
  descriptor->ReadRemoteDescriptor(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.gatt_descriptor_value_changed_count());

  // Start notifications on the descriptor's characteristic. The descriptor
  // value should change.
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  base::MessageLoop::current()->Run();
  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(1U, update_sessions_.size());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Read the new descriptor value. We should receive a value updated event.
  descriptor->ReadRemoteDescriptor(
      base::Bind(&BluetoothGattChromeOSTest::ValueCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_TRUE(ValuesEqual(last_read_value_, descriptor->GetValue()));
  EXPECT_FALSE(ValuesEqual(desc_value, descriptor->GetValue()));
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(2, observer.gatt_descriptor_value_changed_count());
}

TEST_F(BluetoothGattChromeOSTest, NotifySessions) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();

  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());

  // Request to start notifications.
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));

  // The operation still hasn't completed but we should have received the first
  // notification.
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(update_sessions_.empty());

  // Send a two more requests, which should get queued.
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(update_sessions_.empty());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Run the main loop. The initial call should complete. The queued call should
  // succeed immediately.
  base::MessageLoop::current()->Run();

  EXPECT_EQ(3, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(3U, update_sessions_.size());

  // Notifications should be getting sent regularly now.
  base::MessageLoop::current()->Run();
  EXPECT_GT(observer.gatt_characteristic_value_changed_count(), 1);

  // Stop one of the sessions. The session should become inactive but the
  // characteristic should still be notifying.
  BluetoothGattNotifySession* session = update_sessions_[0];
  EXPECT_TRUE(session->IsActive());
  session->Stop(base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                           base::Unretained(this)));
  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(session->IsActive());
  EXPECT_EQ(characteristic->GetIdentifier(),
            session->GetCharacteristicIdentifier());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Delete another session. Characteristic should still be notifying.
  update_sessions_.pop_back();
  EXPECT_EQ(2U, update_sessions_.size());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_FALSE(update_sessions_[0]->IsActive());
  EXPECT_TRUE(update_sessions_[1]->IsActive());

  // Clear the last session.
  update_sessions_.clear();
  EXPECT_TRUE(update_sessions_.empty());
  EXPECT_FALSE(characteristic->IsNotifying());

  success_callback_count_ = 0;
  observer.Reset();

  // Enable notifications again.
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(update_sessions_.empty());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Run the message loop. Notifications should begin.
  base::MessageLoop::current()->Run();

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(1U, update_sessions_.size());
  EXPECT_TRUE(update_sessions_[0]->IsActive());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Check that notifications are happening.
  base::MessageLoop::current()->Run();
  EXPECT_GT(observer.gatt_characteristic_value_changed_count(), 1);

  // Request another session. This should return immediately.
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(2, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2U, update_sessions_.size());
  EXPECT_TRUE(update_sessions_[0]->IsActive());
  EXPECT_TRUE(update_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic->IsNotifying());

  // Hide the characteristic. The sessions should become inactive.
  fake_bluetooth_gatt_characteristic_client_->HideHeartRateCharacteristics();
  EXPECT_EQ(2U, update_sessions_.size());
  EXPECT_FALSE(update_sessions_[0]->IsActive());
  EXPECT_FALSE(update_sessions_[1]->IsActive());
}

TEST_F(BluetoothGattChromeOSTest, NotifySessionsMadeInactive) {
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  BluetoothDevice* device =
      adapter_->GetDevice(FakeBluetoothDeviceClient::kLowEnergyAddress);
  ASSERT_TRUE(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Expose the fake Heart Rate service. This will asynchronously expose
  // characteristics.
  fake_bluetooth_gatt_service_client_->ExposeHeartRateService(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kLowEnergyPath));
  ASSERT_EQ(1, observer.gatt_service_added_count());

  BluetoothGattService* service =
      device->GetGattService(observer.last_gatt_service_id());

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());

  // Run the message loop so that the characteristics appear.
  base::MessageLoop::current()->Run();

  BluetoothGattCharacteristic* characteristic = service->GetCharacteristic(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath()
          .value());
  ASSERT_TRUE(characteristic);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());

  // Send several requests to start notifications.
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));

  // The operation still hasn't completed but we should have received the first
  // notification.
  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());

  // Run the main loop. The initial call should complete. The queued calls
  // should succeed immediately.
  base::MessageLoop::current()->Run();

  EXPECT_EQ(4, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_EQ(4U, update_sessions_.size());

  for (int i = 0; i < 4; i++)
    EXPECT_TRUE(update_sessions_[0]->IsActive());

  // Stop notifications directly through the client. The sessions should get
  // marked as inactive.
  fake_bluetooth_gatt_characteristic_client_->StopNotify(
      fake_bluetooth_gatt_characteristic_client_->GetHeartRateMeasurementPath(),
      base::Bind(&BluetoothGattChromeOSTest::SuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(5, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(characteristic->IsNotifying());
  EXPECT_EQ(4U, update_sessions_.size());

  for (int i = 0; i < 4; i++)
    EXPECT_FALSE(update_sessions_[0]->IsActive());

  // It should be possible to restart notifications and the call should reset
  // the session count and make a request through the client.
  update_sessions_.clear();
  success_callback_count_ = 0;
  observer.Reset();
  characteristic->StartNotifySession(
      base::Bind(&BluetoothGattChromeOSTest::NotifySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothGattChromeOSTest::ServiceErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_TRUE(update_sessions_.empty());

  base::MessageLoop::current()->Run();

  EXPECT_EQ(1, success_callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_TRUE(characteristic->IsNotifying());
  EXPECT_EQ(1U, update_sessions_.size());
  EXPECT_TRUE(update_sessions_[0]->IsActive());
}

}  // namespace chromeos
