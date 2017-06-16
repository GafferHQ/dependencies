// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_pairing_chromeos.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothAudioSink;
using device::BluetoothDevice;
using device::BluetoothDiscoveryFilter;
using device::BluetoothDiscoverySession;
using device::BluetoothUUID;
using device::TestBluetoothAdapterObserver;

namespace chromeos {

namespace {

// Callback for BluetoothDevice::GetConnectionInfo() that simply saves the
// connection info to the bound argument.
void SaveConnectionInfo(BluetoothDevice::ConnectionInfo* out,
                        const BluetoothDevice::ConnectionInfo& conn_info) {
  *out = conn_info;
};

class FakeBluetoothProfileServiceProviderDelegate
    : public chromeos::BluetoothProfileServiceProvider::Delegate {
 public:
  FakeBluetoothProfileServiceProviderDelegate() {}

  // BluetoothProfileServiceProvider::Delegate:
  void Released() override {}

  void NewConnection(const dbus::ObjectPath&,
                     scoped_ptr<dbus::FileDescriptor>,
                     const BluetoothProfileServiceProvider::Delegate::Options&,
                     const ConfirmationCallback&) override {}

  void RequestDisconnection(const dbus::ObjectPath&,
                            const ConfirmationCallback&) override {}

  void Cancel() override {}
};

}  // namespace

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  TestPairingDelegate()
      : call_count_(0),
        request_pincode_count_(0),
        request_passkey_count_(0),
        display_pincode_count_(0),
        display_passkey_count_(0),
        keys_entered_count_(0),
        confirm_passkey_count_(0),
        authorize_pairing_count_(0),
        last_passkey_(9999999U),
        last_entered_(999U) {}
  ~TestPairingDelegate() override {}

  void RequestPinCode(BluetoothDevice* device) override {
    ++call_count_;
    ++request_pincode_count_;
    QuitMessageLoop();
  }

  void RequestPasskey(BluetoothDevice* device) override {
    ++call_count_;
    ++request_passkey_count_;
    QuitMessageLoop();
  }

  void DisplayPinCode(BluetoothDevice* device,
                      const std::string& pincode) override {
    ++call_count_;
    ++display_pincode_count_;
    last_pincode_ = pincode;
    QuitMessageLoop();
  }

  void DisplayPasskey(BluetoothDevice* device, uint32 passkey) override {
    ++call_count_;
    ++display_passkey_count_;
    last_passkey_ = passkey;
    QuitMessageLoop();
  }

  void KeysEntered(BluetoothDevice* device, uint32 entered) override {
    ++call_count_;
    ++keys_entered_count_;
    last_entered_ = entered;
    QuitMessageLoop();
  }

  void ConfirmPasskey(BluetoothDevice* device, uint32 passkey) override {
    ++call_count_;
    ++confirm_passkey_count_;
    last_passkey_ = passkey;
    QuitMessageLoop();
  }

  void AuthorizePairing(BluetoothDevice* device) override {
    ++call_count_;
    ++authorize_pairing_count_;
    QuitMessageLoop();
  }

  int call_count_;
  int request_pincode_count_;
  int request_passkey_count_;
  int display_pincode_count_;
  int display_passkey_count_;
  int keys_entered_count_;
  int confirm_passkey_count_;
  int authorize_pairing_count_;
  uint32 last_passkey_;
  uint32 last_entered_;
  std::string last_pincode_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->Quit();
    }
  }
};

class BluetoothChromeOSTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_ptr<DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    // We need to initialize DBusThreadManager early to prevent
    // Bluetooth*::Create() methods from picking the real instead of fake
    // implementations.
    fake_bluetooth_adapter_client_ = new FakeBluetoothAdapterClient;
    dbus_setter->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(fake_bluetooth_adapter_client_));
    fake_bluetooth_device_client_ = new FakeBluetoothDeviceClient;
    dbus_setter->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(fake_bluetooth_device_client_));
    dbus_setter->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    dbus_setter->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    dbus_setter->SetBluetoothGattServiceClient(
        scoped_ptr<BluetoothGattServiceClient>(
            new FakeBluetoothGattServiceClient));

    fake_bluetooth_adapter_client_->SetSimulationIntervalMs(10);

    callback_count_ = 0;
    error_callback_count_ = 0;
    last_connect_error_ = BluetoothDevice::ERROR_UNKNOWN;
    last_client_error_ = "";
  }

  void TearDown() override {
    for (ScopedVector<BluetoothDiscoverySession>::iterator iter =
            discovery_sessions_.begin();
         iter != discovery_sessions_.end();
         ++iter) {
      BluetoothDiscoverySession* session = *iter;
      if (!session->IsActive())
        continue;
      callback_count_ = 0;
      session->Stop(GetCallback(), GetErrorCallback());
      message_loop_.Run();
      ASSERT_EQ(1, callback_count_);
    }
    discovery_sessions_.clear();
    adapter_ = nullptr;
    DBusThreadManager::Shutdown();
  }

  // Generic callbacks
  void Callback() {
    ++callback_count_;
    QuitMessageLoop();
  }

  base::Closure GetCallback() {
    return base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this));
  }

  void DiscoverySessionCallback(
      scoped_ptr<BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    discovery_sessions_.push_back(discovery_session.release());
    QuitMessageLoop();
  }

  void AudioSinkAcquiredCallback(scoped_refptr<BluetoothAudioSink>) {
    ++callback_count_;
    QuitMessageLoop();
  }

  void ProfileRegisteredCallback(BluetoothAdapterProfileChromeOS* profile) {
    adapter_profile_ = profile;
    ++callback_count_;
    QuitMessageLoop();
  }

  void ErrorCallback() {
    ++error_callback_count_;
    QuitMessageLoop();
  }

  base::Closure GetErrorCallback() {
    return base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                      base::Unretained(this));
  }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
    last_client_error_ = error_name;
    QuitMessageLoop();
  }

  void ConnectErrorCallback(BluetoothDevice::ConnectErrorCode error) {
    ++error_callback_count_;
    last_connect_error_ = error;
  }

  void AudioSinkErrorCallback(BluetoothAudioSink::ErrorCode) {
    ++error_callback_count_;
    QuitMessageLoop();
  }

  void ErrorCompletionCallback(const std::string& error_message) {
    ++error_callback_count_;
    QuitMessageLoop();
  }

  // Call to fill the adapter_ member with a BluetoothAdapter instance.
  void GetAdapter() {
    adapter_ = new BluetoothAdapterChromeOS();
    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Run a discovery phase until the named device is detected, or if the named
  // device is not created, the discovery process ends without finding it.
  //
  // The correct behavior of discovery is tested by the "Discovery" test case
  // without using this function.
  void DiscoverDevice(const std::string& address) {
    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(base::MessageLoop::current() != nullptr);
    fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

    TestBluetoothAdapterObserver observer(adapter_);

    adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        GetErrorCallback());
    base::MessageLoop::current()->Run();
    ASSERT_EQ(2, callback_count_);
    ASSERT_EQ(0, error_callback_count_);
    ASSERT_EQ((size_t)1, discovery_sessions_.size());
    ASSERT_TRUE(discovery_sessions_[0]->IsActive());
    callback_count_ = 0;

    ASSERT_TRUE(adapter_->IsPowered());
    ASSERT_TRUE(adapter_->IsDiscovering());

    while (!observer.device_removed_count() &&
           observer.last_device_address() != address)
      base::MessageLoop::current()->Run();

    discovery_sessions_[0]->Stop(GetCallback(), GetErrorCallback());
    base::MessageLoop::current()->Run();
    ASSERT_EQ(1, callback_count_);
    ASSERT_EQ(0, error_callback_count_);
    callback_count_ = 0;

    ASSERT_FALSE(adapter_->IsDiscovering());
  }

  // Run a discovery phase so we have devices that can be paired with.
  void DiscoverDevices() {
    // Pass an invalid address for the device so that the discovery process
    // completes with all devices.
    DiscoverDevice("does not exist");
  }

 protected:
  base::MessageLoop message_loop_;
  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client_;
  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int callback_count_;
  int error_callback_count_;
  enum BluetoothDevice::ConnectErrorCode last_connect_error_;
  std::string last_client_error_;
  ScopedVector<BluetoothDiscoverySession> discovery_sessions_;
  BluetoothAdapterProfileChromeOS* adapter_profile_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->Quit();
    }
  }
};

TEST_F(BluetoothChromeOSTest, AlreadyPresent) {
  GetAdapter();

  // This verifies that the class gets the list of adapters when created;
  // and initializes with an existing adapter if there is one.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // There should be a device
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  EXPECT_EQ(2U, devices.size());
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress,
            devices[1]->GetAddress());
}

TEST_F(BluetoothChromeOSTest, BecomePresent) {
  fake_bluetooth_adapter_client_->SetVisible(false);
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with true, and IsPresent() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_TRUE(observer.last_present());

  EXPECT_TRUE(adapter_->IsPresent());

  // We should have had a device announced.
  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress,
            observer.last_device_address());

  // Other callbacks shouldn't be called if the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, BecomeNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had a device removed.
  EXPECT_EQ(2, observer.device_removed_count());
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress,
            observer.last_device_address());

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, SecondAdapter) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer, then add a second adapter. Nothing should change,
  // we ignore the second adapter.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetSecondVisible(true);

  EXPECT_EQ(0, observer.present_changed_count());

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());

  // Try removing the first adapter, we should now act as if the adapter
  // is no longer present rather than fall back to the second.
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had a device removed.
  EXPECT_EQ(2, observer.device_removed_count());
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress,
            observer.last_device_address());

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());

  observer.Reset();

  // Removing the second adapter shouldn't set anything either.
  fake_bluetooth_adapter_client_->SetSecondVisible(false);

  EXPECT_EQ(0, observer.device_removed_count());
  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_EQ(0, observer.discovering_changed_count());
}

TEST_F(BluetoothChromeOSTest, BecomePowered) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with true, and IsPowered() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());

  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothChromeOSTest, BecomeNotPowered) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with false, and IsPowered() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetPowered(false, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothChromeOSTest, SetPoweredWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothChromeOSTest, ChangeAdapterName) {
  GetAdapter();

  static const std::string new_name(".__.");

  adapter_->SetName(new_name, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(new_name, adapter_->GetName());
}

TEST_F(BluetoothChromeOSTest, ChangeAdapterNameWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  adapter_->SetName("^o^", GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ("", adapter_->GetName());
}

TEST_F(BluetoothChromeOSTest, BecomeDiscoverable) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count());

  EXPECT_TRUE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothChromeOSTest, BecomeNotDiscoverable) {
  GetAdapter();
  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with false, and IsDiscoverable() to return false.
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetDiscoverable(false, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count());

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothChromeOSTest, SetDiscoverableWhenNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());

  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsDiscoverable());

  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.discoverable_changed_count());

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothChromeOSTest, StopDiscovery) {
  GetAdapter();

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // Install an observer; aside from the callback, expect the
  // AdapterDiscoveringChanged method to be called and no longer to be
  // discovering,
  TestBluetoothAdapterObserver observer(adapter_);

  discovery_sessions_[0]->Stop(GetCallback(), GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());

  EXPECT_FALSE(adapter_->IsDiscovering());
  discovery_sessions_.clear();
  callback_count_ = 0;

  // Test that the Stop callbacks get called even if the
  // BluetoothDiscoverySession objects gets deleted
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;
  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  discovery_sessions_[0]->Stop(GetCallback(), GetErrorCallback());
  discovery_sessions_.clear();

  message_loop_.Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

TEST_F(BluetoothChromeOSTest, Discovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // First two devices to appear.
  message_loop_.Run();

  EXPECT_EQ(2, observer.device_added_count());
  EXPECT_EQ(FakeBluetoothDeviceClient::kLowEnergyAddress,
            observer.last_device_address());

  // Next we should get another two devices...
  message_loop_.Run();
  EXPECT_EQ(4, observer.device_added_count());

  // Okay, let's run forward until a device is actually removed...
  while (!observer.device_removed_count())
    message_loop_.Run();

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(FakeBluetoothDeviceClient::kVanishingDeviceAddress,
            observer.last_device_address());
}

TEST_F(BluetoothChromeOSTest, PoweredAndDiscovering) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  fake_bluetooth_adapter_client_->SetVisible(false);
  ASSERT_FALSE(adapter_->IsPresent());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  // Install an observer; expect the AdapterPresentChanged,
  // AdapterPoweredChanged and AdapterDiscoveringChanged methods to be called
  // with true, and IsPresent(), IsPowered() and IsDiscovering() to all
  // return true.
  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_TRUE(observer.last_present());
  EXPECT_TRUE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_TRUE(observer.last_powered());
  EXPECT_TRUE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());

  observer.Reset();

  // Now mark the adapter not present again. Expect the methods to be called
  // again, to reset the properties back to false
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count());
  EXPECT_FALSE(observer.last_present());
  EXPECT_FALSE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count());
  EXPECT_FALSE(observer.last_powered());
  EXPECT_FALSE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

// This unit test asserts that the basic reference counting logic works
// correctly for discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothChromeOSTest, MultipleDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        GetErrorCallback());
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Request to stop discovery twice.
  for (int i = 0; i < 2; i++) {
    discovery_sessions_[i]->Stop(GetCallback(), GetErrorCallback());
  }

  // The observer should have received no additional discovering changed events,
  // the success callback should have been called 2 times and the adapter should
  // still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_FALSE(discovery_sessions_[1]->IsActive());
  EXPECT_TRUE(discovery_sessions_[2]->IsActive());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        GetErrorCallback());
  }

  // The observer should have received no additional discovering changed events,
  // the success callback should have been called 3 times and the adapter should
  // still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(8, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)6, discovery_sessions_.size());

  // Request to stop discovery 4 times.
  for (int i = 2; i < 6; i++) {
    discovery_sessions_[i]->Stop(GetCallback(), GetErrorCallback());
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 4 times and the adapter
  // should no longer be discovering.
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(12, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // All discovery sessions should be inactive.
  for (int i = 0; i < 6; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());

  // Request to stop discovery on of the inactive sessions.
  discovery_sessions_[0]->Stop(GetCallback(), GetErrorCallback());

  // The call should have failed.
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(12, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

// This unit test asserts that the reference counting logic works correctly in
// the cases when the adapter gets reset and D-Bus calls are made outside of
// the BluetoothAdapter.
TEST_F(BluetoothChromeOSTest,
       UnexpectedChangesDuringMultipleDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        GetErrorCallback());
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  // Stop device discovery behind the adapter. The adapter and the observer
  // should be notified of the change and the reference count should be reset.
  // Even though FakeBluetoothAdapterClient does its own reference counting and
  // we called 3 BluetoothAdapter::StartDiscoverySession 3 times, the
  // FakeBluetoothAdapterClient's count should be only 1 and a single call to
  // FakeBluetoothAdapterClient::StopDiscovery should work.
  fake_bluetooth_adapter_client_->StopDiscovery(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath), GetCallback(),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // All discovery session instances should have been updated.
  for (int i = 0; i < 3; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  // It should be possible to successfully start discovery.
  for (int i = 0; i < 2; i++) {
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        GetErrorCallback());
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();
  EXPECT_EQ(3, observer.discovering_changed_count());
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());

  for (int i = 0; i < 2; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  // Make the adapter disappear and appear. This will make it come back as
  // discovering. When this happens, the reference count should become and
  // remain 0 as no new request was made through the BluetoothAdapter.
  fake_bluetooth_adapter_client_->SetVisible(false);
  ASSERT_FALSE(adapter_->IsPresent());
  EXPECT_EQ(4, observer.discovering_changed_count());
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());

  for (int i = 0; i < 2; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  fake_bluetooth_adapter_client_->SetVisible(true);
  ASSERT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Start and stop discovery. At this point, FakeBluetoothAdapterClient has
  // a reference count that is equal to 1. Pretend that this was done by an
  // application other than us. Starting and stopping discovery will succeed
  // but it won't cause the discovery state to change.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();  // Run the loop, as there should have been a D-Bus call.
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(7, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  discovery_sessions_[0]->Stop(GetCallback(), GetErrorCallback());
  message_loop_.Run();  // Run the loop, as there should have been a D-Bus call.
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(8, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  discovery_sessions_.clear();

  // Start discovery again.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();  // Run the loop, as there should have been a D-Bus call.
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(9, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Stop discovery via D-Bus. The fake client's reference count will drop but
  // the discovery state won't change since our BluetoothAdapter also just
  // requested it via D-Bus.
  fake_bluetooth_adapter_client_->StopDiscovery(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath), GetCallback(),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(5, observer.discovering_changed_count());
  EXPECT_EQ(10, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Now end the discovery session. This should change the adapter's discovery
  // state.
  discovery_sessions_[0]->Stop(GetCallback(), GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(6, observer.discovering_changed_count());
  EXPECT_EQ(11, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
}

TEST_F(BluetoothChromeOSTest, InvalidatedDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        GetErrorCallback());
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  // Delete all but one discovery session.
  discovery_sessions_.pop_back();
  discovery_sessions_.pop_back();
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Stop device discovery behind the adapter. The one active discovery session
  // should become inactive, but more importantly, we shouldn't run into any
  // memory errors as the sessions that we explicitly deleted should get
  // cleaned up.
  fake_bluetooth_adapter_client_->StopDiscovery(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath), GetCallback(),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
}

TEST_F(BluetoothChromeOSTest, QueuedDiscoveryRequests) {
  GetAdapter();

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request to start discovery. The call should be pending.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  EXPECT_EQ(0, callback_count_);

  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  // The underlying adapter has started discovery, but our call hasn't returned
  // yet.
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Request to start discovery twice. These should get queued and there should
  // be no change in state.
  for (int i = 0; i < 2; i++) {
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        GetErrorCallback());
  }
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Process the pending call. The queued calls should execute and the discovery
  // session reference count should increase.
  message_loop_.Run();
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Verify the reference count by removing sessions 3 times. The last request
  // should remain pending.
  for (int i = 0; i < 3; i++) {
    discovery_sessions_[i]->Stop(GetCallback(), GetErrorCallback());
  }
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_FALSE(discovery_sessions_[1]->IsActive());
  EXPECT_TRUE(discovery_sessions_[2]->IsActive());

  // Request to stop the session whose call is pending should fail.
  discovery_sessions_[2]->Stop(GetCallback(), GetErrorCallback());
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_[2]->IsActive());

  // Request to start should get queued.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Run the pending request.
  message_loop_.Run();
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(3, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());
  EXPECT_FALSE(discovery_sessions_[2]->IsActive());

  // The queued request to start discovery should have been issued but is still
  // pending. Run the loop and verify.
  message_loop_.Run();
  EXPECT_EQ(7, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(3, observer.discovering_changed_count());
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)4, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[3]->IsActive());
}

TEST_F(BluetoothChromeOSTest, StartDiscoverySession) {
  GetAdapter();

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count());
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Request a new discovery session.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Start another session. A new one should be returned in the callback, which
  // in turn will destroy the previous session. Adapter should still be
  // discovering and the reference count should be 1.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Request a new session.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[1]->IsActive());
  EXPECT_NE(discovery_sessions_[0], discovery_sessions_[1]);

  // Stop the previous discovery session. The session should end but discovery
  // should continue.
  discovery_sessions_[0]->Stop(GetCallback(), GetErrorCallback());
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count());
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_TRUE(discovery_sessions_[1]->IsActive());

  // Delete the current active session. Discovery should eventually stop.
  discovery_sessions_.clear();
  while (observer.last_discovering())
    message_loop_.RunUntilIdle();

  EXPECT_EQ(2, observer.discovering_changed_count());
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, SetDiscoveryFilterBeforeStartDiscovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-60);
  df->AddUUID(BluetoothUUID("1000"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  adapter_->SetPowered(true, base::Bind(&BluetoothChromeOSTest::Callback,
                                        base::Unretained(this)),
                       base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));
  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  ASSERT_TRUE(df->Equals(*discovery_sessions_[0]->GetDiscoveryFilter()));

  auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_NE(nullptr, filter);
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-60, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  std::vector<std::string> uuids = *filter->uuids;
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));

  discovery_sessions_[0]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(discovery_sessions_[0]->GetDiscoveryFilter(),
            (BluetoothDiscoveryFilter*)nullptr);

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

TEST_F(BluetoothChromeOSTest, SetDiscoveryFilterBeforeStartDiscoveryFail) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-60);
  df->AddUUID(BluetoothUUID("1000"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  adapter_->SetPowered(true, base::Bind(&BluetoothChromeOSTest::Callback,
                                        base::Unretained(this)),
                       base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  callback_count_ = 0;

  fake_bluetooth_adapter_client_->MakeSetDiscoveryFilterFail();

  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1, error_callback_count_);
  error_callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)0, discovery_sessions_.size());

  auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// This test queues two requests to StartDiscovery with pre set filter. This
// should result in SetDiscoveryFilter, then StartDiscovery, and SetDiscovery
// DBus calls
TEST_F(BluetoothChromeOSTest, QueuedSetDiscoveryFilterBeforeStartDiscovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-60);
  df->AddUUID(BluetoothUUID("1000"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  BluetoothDiscoveryFilter* df2 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC);
  df2->SetRSSI(-65);
  df2->AddUUID(BluetoothUUID("1002"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter2(df2);

  adapter_->SetPowered(true, base::Bind(&BluetoothChromeOSTest::Callback,
                                        base::Unretained(this)),
                       base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  // Queue two requests to start discovery session with filter.
  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter2.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  // Run requests, on DBus level there should be call SetDiscoveryFilter, then
  // StartDiscovery, then SetDiscoveryFilter again.
  message_loop_.Run();
  message_loop_.Run();

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  ASSERT_TRUE(df->Equals(*discovery_sessions_[0]->GetDiscoveryFilter()));
  ASSERT_TRUE(discovery_sessions_[1]->IsActive());
  ASSERT_TRUE(df2->Equals(*discovery_sessions_[1]->GetDiscoveryFilter()));

  auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_NE(nullptr, filter);
  EXPECT_EQ("auto", *filter->transport);
  EXPECT_EQ(-65, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  auto uuids = *filter->uuids;
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1002"));

  discovery_sessions_[0]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  discovery_sessions_[1]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(discovery_sessions_[0]->GetDiscoveryFilter(),
            (BluetoothDiscoveryFilter*)nullptr);
  ASSERT_FALSE(discovery_sessions_[1]->IsActive());
  ASSERT_EQ(discovery_sessions_[1]->GetDiscoveryFilter(),
            (BluetoothDiscoveryFilter*)nullptr);

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// Call StartFilteredDiscovery twice (2nd time while 1st call is still pending).
// Make the first SetDiscoveryFilter fail and the second one succeed. It should
// end up with one active discovery session.
TEST_F(BluetoothChromeOSTest,
       QueuedSetDiscoveryFilterBeforeStartDiscoveryFail) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-60);
  df->AddUUID(BluetoothUUID("1000"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  BluetoothDiscoveryFilter* df2 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC);
  df2->SetRSSI(-65);
  df2->AddUUID(BluetoothUUID("1002"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter2(df2);

  adapter_->SetPowered(true, base::Bind(&BluetoothChromeOSTest::Callback,
                                        base::Unretained(this)),
                       base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  fake_bluetooth_adapter_client_->MakeSetDiscoveryFilterFail();

  // Queue two requests to start discovery session with filter.
  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter2.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  // First request to SetDiscoveryFilter should fail, resulting in no session
  // being created.
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  error_callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)0, discovery_sessions_.size());

  message_loop_.Run();

  // Second request should succeed
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  ASSERT_TRUE(df2->Equals(*discovery_sessions_[0]->GetDiscoveryFilter()));

  auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_NE(nullptr, filter);
  EXPECT_EQ("bredr", *filter->transport);
  EXPECT_EQ(-65, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  auto uuids = *filter->uuids;
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1002"));

  discovery_sessions_[0]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(discovery_sessions_[0]->GetDiscoveryFilter(),
            (BluetoothDiscoveryFilter*)nullptr);

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

TEST_F(BluetoothChromeOSTest, SetDiscoveryFilterAfterStartDiscovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->SetPowered(true, base::Bind(&BluetoothChromeOSTest::Callback,
                                        base::Unretained(this)),
                       base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  EXPECT_EQ(1, observer.discovering_changed_count());
  observer.Reset();

  auto null_instance = scoped_ptr<BluetoothDiscoveryFilter>();
  null_instance.reset();
  ASSERT_EQ(discovery_sessions_[0]->GetDiscoveryFilter(), null_instance.get());

  auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-60);
  df->AddUUID(BluetoothUUID("1000"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  discovery_sessions_[0]->SetDiscoveryFilter(
      discovery_filter.Pass(),
      base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(df->Equals(*discovery_sessions_[0]->GetDiscoveryFilter()));

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_NE(nullptr, filter);
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-60, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  std::vector<std::string> uuids = *filter->uuids;
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));

  discovery_sessions_[0]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(discovery_sessions_[0]->GetDiscoveryFilter(),
            (BluetoothDiscoveryFilter*)nullptr);

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// This unit test asserts that the basic reference counting, and filter merging
// works correctly for discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothChromeOSTest, SetDiscoveryFilterBeforeStartDiscoveryMultiple) {
  GetAdapter();
  adapter_->SetPowered(true, base::Bind(&BluetoothChromeOSTest::Callback,
                                        base::Unretained(this)),
                       base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestBluetoothAdapterObserver observer(adapter_);

  // Request device discovery with pre-set filter 3 times.
  for (int i = 0; i < 3; i++) {
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter;
    if (i == 0) {
      BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
      df->SetRSSI(-85);
      df->AddUUID(BluetoothUUID("1000"));
      discovery_filter.reset(df);
    } else if (i == 1) {
      BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
      df->SetRSSI(-60);
      df->AddUUID(BluetoothUUID("1020"));
      df->AddUUID(BluetoothUUID("1001"));
      discovery_filter.reset(df);
    } else if (i == 2) {
      BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
      df->SetRSSI(-65);
      df->AddUUID(BluetoothUUID("1020"));
      df->AddUUID(BluetoothUUID("1003"));
      discovery_filter.reset(df);
    }

    adapter_->StartDiscoverySessionWithFilter(
        discovery_filter.Pass(),
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));

    message_loop_.Run();

    if (i == 0) {
      EXPECT_EQ(1, observer.discovering_changed_count());
      observer.Reset();

      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
    } else if (i == 1) {
      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1001"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));
    } else if (i == 2) {
      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1001"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1003"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));
    }
  }

  // the success callback should have been called 3 times and the adapter should
  // be discovering.
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  callback_count_ = 0;
  // Request to stop discovery twice.
  for (int i = 0; i < 2; i++) {
    discovery_sessions_[i]->Stop(
        base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    message_loop_.Run();

    if (i == 0) {
      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(3UL, uuids.size());
      EXPECT_EQ(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1001"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1003"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));
    } else if (i == 1) {
      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(2UL, uuids.size());
      EXPECT_EQ(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
      EXPECT_EQ(uuids.end(), std::find(uuids.begin(), uuids.end(), "1001"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1003"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));
    } else if (i == 2) {
      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-65, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_EQ(0UL, uuids.size());
    }
  }

  // The success callback should have been called 2 times and the adapter should
  // still be discovering.
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_FALSE(discovery_sessions_[1]->IsActive());
  EXPECT_TRUE(discovery_sessions_[2]->IsActive());

  callback_count_ = 0;

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter;

    if (i == 0) {
      BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
      df->SetRSSI(-85);
      df->AddUUID(BluetoothUUID("1000"));
      discovery_filter.reset(df);
    } else if (i == 1) {
      BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
      df->SetRSSI(-60);
      df->AddUUID(BluetoothUUID("1020"));
      df->AddUUID(BluetoothUUID("1001"));
      discovery_filter.reset(df);
    } else if (i == 2) {
      BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
          BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
      df->SetRSSI(-65);
      df->AddUUID(BluetoothUUID("1020"));
      df->AddUUID(BluetoothUUID("1003"));
      discovery_filter.reset(df);
    }

    adapter_->StartDiscoverySessionWithFilter(
        discovery_filter.Pass(),
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));

    // each result in 1 requests.
    message_loop_.Run();

    if (i == 0) {
      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1003"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));
    } else if (i == 1 || i == 2) {
      auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
      EXPECT_EQ("le", *filter->transport);
      EXPECT_EQ(-85, *filter->rssi);
      EXPECT_EQ(nullptr, filter->pathloss.get());
      std::vector<std::string> uuids = *filter->uuids;
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1001"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1003"));
      EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));
    }
  }

  // The success callback should have been called 3 times and the adapter should
  // still be discovering.
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)6, discovery_sessions_.size());

  callback_count_ = 0;
  // Request to stop discovery 4 times.
  for (int i = 2; i < 6; i++) {
    discovery_sessions_[i]->Stop(
        base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));

    // filter no  2 is same as filter no 5, so removing it shouldn't cause any
    // filter update
    if (i != 2 && i != 5)
      message_loop_.Run();
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The success callback should have been called 4 times and the adapter should
  // no longer be discovering.
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_EQ(1, observer.discovering_changed_count());

  // All discovery sessions should be inactive.
  for (int i = 0; i < 6; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());

  auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ(nullptr, filter);
}

// This unit test asserts that filter merging logic works correctly for filtered
// discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothChromeOSTest, SetDiscoveryFilterMergingTest) {
  GetAdapter();
  adapter_->SetPowered(true, base::Bind(&BluetoothChromeOSTest::Callback,
                                        base::Unretained(this)),
                       base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                                  base::Unretained(this)));

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-15);
  df->AddUUID(BluetoothUUID("1000"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  auto filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-15, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  std::vector<std::string> uuids = *filter->uuids;
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));

  df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-60);
  df->AddUUID(BluetoothUUID("1020"));
  df->AddUUID(BluetoothUUID("1001"));
  discovery_filter = scoped_ptr<BluetoothDiscoveryFilter>(df);

  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("le", *filter->transport);
  EXPECT_EQ(-60, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  uuids = *filter->uuids;
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1001"));
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));

  BluetoothDiscoveryFilter* df3 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC);
  df3->SetRSSI(-65);
  df3->AddUUID(BluetoothUUID("1020"));
  df3->AddUUID(BluetoothUUID("1003"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter3(df3);

  adapter_->StartDiscoverySessionWithFilter(
      discovery_filter3.Pass(),
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("auto", *filter->transport);
  EXPECT_EQ(-65, *filter->rssi);
  EXPECT_EQ(nullptr, filter->pathloss.get());
  uuids = *filter->uuids;
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1000"));
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1001"));
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1003"));
  EXPECT_NE(uuids.end(), std::find(uuids.begin(), uuids.end(), "1020"));

  // start additionally classic scan
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  filter = fake_bluetooth_adapter_client_->GetDiscoveryFilter();
  EXPECT_EQ("auto", *filter->transport);
  EXPECT_EQ(nullptr, filter->rssi.get());
  EXPECT_EQ(nullptr, filter->pathloss.get());
  EXPECT_EQ(nullptr, filter->uuids.get());

  // Request to stop discovery 4 times.
  for (int i = 3; i >= 0; i--) {
    discovery_sessions_[i]->Stop(
        base::Bind(&BluetoothChromeOSTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));

    // Every session stopping would trigger filter update
    message_loop_.Run();
  }
}

TEST_F(BluetoothChromeOSTest, DeviceProperties) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  // Verify the other device properties.
  EXPECT_EQ(base::UTF8ToUTF16(FakeBluetoothDeviceClient::kPairedDeviceName),
            devices[0]->GetName());
  EXPECT_EQ(BluetoothDevice::DEVICE_COMPUTER, devices[0]->GetDeviceType());
  EXPECT_TRUE(devices[0]->IsPaired());
  EXPECT_FALSE(devices[0]->IsConnected());
  EXPECT_FALSE(devices[0]->IsConnecting());

  // Non HID devices are always connectable.
  EXPECT_TRUE(devices[0]->IsConnectable());

  BluetoothDevice::UUIDList uuids = devices[0]->GetUUIDs();
  ASSERT_EQ(2U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1800"));
  EXPECT_EQ(uuids[1], BluetoothUUID("1801"));

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, devices[0]->GetVendorIDSource());
  EXPECT_EQ(0x05ac, devices[0]->GetVendorID());
  EXPECT_EQ(0x030d, devices[0]->GetProductID());
  EXPECT_EQ(0x0306, devices[0]->GetDeviceID());
}

TEST_F(BluetoothChromeOSTest, DeviceClassChanged) {
  // Simulate a change of class of a device, as sometimes occurs
  // during discovery.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
  ASSERT_EQ(BluetoothDevice::DEVICE_COMPUTER, devices[0]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->bluetooth_class.ReplaceValue(0x002580);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[0], observer.last_device());

  EXPECT_EQ(BluetoothDevice::DEVICE_MOUSE, devices[0]->GetDeviceType());
}

TEST_F(BluetoothChromeOSTest, DeviceNameChanged) {
  // Simulate a change of name of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
  ASSERT_EQ(base::UTF8ToUTF16(FakeBluetoothDeviceClient::kPairedDeviceName),
            devices[0]->GetName());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the alias of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  static const std::string new_name("New Device Name");
  properties->alias.ReplaceValue(new_name);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[0], observer.last_device());

  EXPECT_EQ(base::UTF8ToUTF16(new_name), devices[0]->GetName());
}

TEST_F(BluetoothChromeOSTest, DeviceUuidsChanged) {
  // Simulate a change of advertised services of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  BluetoothDevice::UUIDList uuids = devices[0]->GetUUIDs();
  ASSERT_EQ(2U, uuids.size());
  ASSERT_EQ(uuids[0], BluetoothUUID("1800"));
  ASSERT_EQ(uuids[1], BluetoothUUID("1801"));

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  std::vector<std::string> new_uuids;
  new_uuids.push_back(uuids[0].canonical_value());
  new_uuids.push_back(uuids[1].canonical_value());
  new_uuids.push_back("0000110c-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110e-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110a-0000-1000-8000-00805f9b34fb");

  properties->uuids.ReplaceValue(new_uuids);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[0], observer.last_device());

  // Fetching the value should give the new one.
  uuids = devices[0]->GetUUIDs();
  ASSERT_EQ(5U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1800"));
  EXPECT_EQ(uuids[1], BluetoothUUID("1801"));
  EXPECT_EQ(uuids[2], BluetoothUUID("110c"));
  EXPECT_EQ(uuids[3], BluetoothUUID("110e"));
  EXPECT_EQ(uuids[4], BluetoothUUID("110a"));
}

TEST_F(BluetoothChromeOSTest, DeviceInquiryRSSIInvalidated) {
  // Simulate invalidation of inquiry RSSI of a device, as it occurs
  // when discovery is finished.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  // During discovery, rssi is a valid value (-75)
  properties->rssi.ReplaceValue(-75);
  properties->rssi.set_valid(true);

  ASSERT_EQ(-75, devices[0]->GetInquiryRSSI());

  // Install an observer; expect the DeviceChanged method to be called when
  // we invalidate the RSSI of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  // When discovery is over, the value should be invalidated.
  properties->rssi.set_valid(false);
  properties->NotifyPropertyChanged(properties->rssi.name());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[0], observer.last_device());

  int unknown_power = BluetoothDevice::kUnknownPower;
  EXPECT_EQ(unknown_power, devices[0]->GetInquiryRSSI());
}

TEST_F(BluetoothChromeOSTest, DeviceInquiryTxPowerInvalidated) {
  // Simulate invalidation of inquiry TxPower of a device, as it occurs
  // when discovery is finished.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  // During discovery, tx_power is a valid value (0)
  properties->tx_power.ReplaceValue(0);
  properties->tx_power.set_valid(true);

  ASSERT_EQ(0, devices[0]->GetInquiryTxPower());

  // Install an observer; expect the DeviceChanged method to be called when
  // we invalidate the tx_power of the device.
  TestBluetoothAdapterObserver observer(adapter_);

  // When discovery is over, the value should be invalidated.
  properties->tx_power.set_valid(false);
  properties->NotifyPropertyChanged(properties->tx_power.name());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(devices[0], observer.last_device());

  int unknown_power = BluetoothDevice::kUnknownPower;
  EXPECT_EQ(unknown_power, devices[0]->GetInquiryTxPower());
}

TEST_F(BluetoothChromeOSTest, ForgetDevice) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(2U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  std::string address = devices[0]->GetAddress();

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  devices[0]->Forget(GetErrorCallback());
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(address, observer.last_device_address());

  // GetDevices shouldn't return the device either.
  devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
}

TEST_F(BluetoothChromeOSTest, ForgetUnpairedDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  // Connect the device so it becomes trusted and remembered.
  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConnectUnpairablePath));
  ASSERT_TRUE(properties->trusted.value());

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Forget(GetErrorCallback());
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count());
  EXPECT_EQ(FakeBluetoothDeviceClient::kConnectUnpairableAddress,
            observer.last_device_address());

  // GetDevices shouldn't return the device either.
  device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  EXPECT_FALSE(device != nullptr);
}

TEST_F(BluetoothChromeOSTest, ConnectPairedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device is already Paired
  // this should succeed and the device should become connected.
  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected and one for for trusted
  // after connecting.
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, ConnectUnpairableDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device does not require
  // pairing, this should succeed and the device should become connected.
  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected, one for for trusted after
  // connection, and one for the reconnect mode (IsConnectable).
  EXPECT_EQ(5, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConnectUnpairablePath));
  EXPECT_TRUE(properties->trusted.value());

  // Verify is a HID device and is not connectable.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));
  EXPECT_FALSE(device->IsConnectable());
}

TEST_F(BluetoothChromeOSTest, ConnectConnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());

  // Connect again; since the device is already Connected, this shouldn't do
  // anything to initiate the connection.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // The observer will be called because Connecting will toggle true and false,
  // and the trusted property will be updated to true.
  EXPECT_EQ(3, observer.device_changed_count());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, ConnectDeviceFails) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  // Connect without a pairing delegate; since the device requires pairing,
  // this should fail with an error.
  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, DisconnectDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Disconnect(GetCallback(), GetErrorCallback());

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothChromeOSTest, DisconnectUnconnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(device->IsPaired());
  ASSERT_FALSE(device->IsConnected());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestBluetoothAdapterObserver observer(adapter_);

  device->Disconnect(GetCallback(), GetErrorCallback());

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothChromeOSTest, PairLegacyAutopair) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Legacy Autopair device requires no PIN or Passkey to pair because
  // the daemon provides 0000 to the device for us.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kLegacyAutopairPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairDisplayPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated PIN on the screen.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kDisplayPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_pincode_count_);
  EXPECT_EQ("123456", pairing_delegate.last_pincode_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPinCodePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairDisplayPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated Passkey on the screen,
  // and notifies us as it's typed in.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kDisplayPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  // One call for DisplayPasskey() and one for KeysEntered().
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_EQ(1, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(0U, pairing_delegate.last_entered_);

  EXPECT_TRUE(device->IsConnecting());

  // One call to KeysEntered() for each key, including [enter].
  for (int i = 1; i <= 7; ++i) {
    message_loop_.Run();

    EXPECT_EQ(2 + i, pairing_delegate.call_count_);
    EXPECT_EQ(1 + i, pairing_delegate.keys_entered_count_);
    EXPECT_EQ(static_cast<uint32_t>(i), pairing_delegate.last_entered_);
  }

  message_loop_.Run();

  // 8 KeysEntered notifications (0 to 7, inclusive) and one aditional call for
  // DisplayPasskey().
  EXPECT_EQ(9, pairing_delegate.call_count_);
  EXPECT_EQ(8, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(7U, pairing_delegate.last_entered_);

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));

  // And usually not connectable.
  EXPECT_FALSE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a PIN for them.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the PIN.
  device->SetPinCode("1234");
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is not a HID device.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(0U, uuids.size());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requests that we confirm a displayed passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_TRUE(device->IsConnecting());

  // Confirm the passkey.
  device->ConfirmPairing();
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a Passkey, this would be some kind of
  // device that has a display, but doesn't use "just works" - maybe a car?
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the Passkey.
  device->SetPasskey(1234);
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Uses just-works pairing, since this is an outgoing pairing, no delegate
  // interaction is required.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairUnpairableDeviceFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kUnpairableDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(FakeBluetoothDeviceClient::kVanishingDeviceAddress);

  // The vanishing device times out during pairing
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kVanishingDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_TIMEOUT, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingFailsAtConnection) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Everything seems to go according to plan with the unconnectable device;
  // it pairs, but then you can't make connections to it after.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  // Two changes for connecting, one for paired and one for trusted after
  // pairing. The device should not be connected.
  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true still (since pairing
  // worked).
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(
              FakeBluetoothDeviceClient::kUnconnectableDevicePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for passkey confirmation.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledInFlight) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing while we're waiting for the remote host.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(&pairing_delegate, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requires that we provide a PIN code.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);

  // Set the PIN.
  device->SetPinCode("1234");
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, IncomingPairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we confirm a displayed passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);

  // Confirm the passkey.
  device->ConfirmPairing();
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);

  // Set the Passkey.
  device->SetPasskey(1234);
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, IncomingPairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Uses just-works pairing so, sinec this an incoming pairing, require
  // authorization from the user.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.authorize_pairing_count_);

  // Confirm the pairing.
  device->ConfirmPairing();
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(device, observer.last_device());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPinCodeWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requires that we provide a PIN Code, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, IncomingPairConfirmPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we confirm a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we provide a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, IncomingPairJustWorksWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Uses just-works pairing and thus requires authorization for incoming
  // pairings, without a pairing delegate, that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);
}

TEST_F(BluetoothChromeOSTest, RemovePairingDelegateDuringPairing) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != nullptr);
  ASSERT_FALSE(device->IsPaired());

  TestBluetoothAdapterObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath), true,
      GetCallback(), base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                                base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);

  // A pairing context should now be set on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  ASSERT_TRUE(device_chromeos->GetPairing() != nullptr);

  // Removing the pairing delegate should remove that pairing context.
  adapter_->RemovePairingDelegate(&pairing_delegate);

  EXPECT_TRUE(device_chromeos->GetPairing() == nullptr);

  // Set the Passkey, this should now have no effect since the pairing has
  // been, in-effect, cancelled
  device->SetPasskey(1234);

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, DeviceId) {
  GetAdapter();

  // Use the built-in paired device for this test, grab its Properties
  // structure so we can adjust the underlying modalias property.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  ASSERT_TRUE(device != nullptr);
  ASSERT_TRUE(properties != nullptr);

  // Valid USB IF-assigned identifier.
  ASSERT_EQ("usb:v05ACp030Dd0306", properties->modalias.value());

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, device->GetVendorIDSource());
  EXPECT_EQ(0x05ac, device->GetVendorID());
  EXPECT_EQ(0x030d, device->GetProductID());
  EXPECT_EQ(0x0306, device->GetDeviceID());

  // Valid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_BLUETOOTH, device->GetVendorIDSource());
  EXPECT_EQ(0x00e0, device->GetVendorID());
  EXPECT_EQ(0x2400, device->GetProductID());
  EXPECT_EQ(0x0400, device->GetDeviceID());

  // Invalid USB IF-assigned identifier.
  properties->modalias.ReplaceValue("usb:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Invalid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Unknown vendor specification identifier.
  properties->modalias.ReplaceValue("chrome:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());
}

TEST_F(BluetoothChromeOSTest, GetConnectionInfoForDisconnectedDevice) {
  GetAdapter();
  BluetoothDevice* device =
      adapter_->GetDevice(FakeBluetoothDeviceClient::kPairedDeviceAddress);

  // Calling GetConnectionInfo for an unconnected device should return a result
  // in which all fields are filled with BluetoothDevice::kUnknownPower.
  BluetoothDevice::ConnectionInfo conn_info(0, 0, 0);
  device->GetConnectionInfo(base::Bind(&SaveConnectionInfo, &conn_info));
  int unknown_power = BluetoothDevice::kUnknownPower;
  EXPECT_NE(0, unknown_power);
  EXPECT_EQ(unknown_power, conn_info.rssi);
  EXPECT_EQ(unknown_power, conn_info.transmit_power);
  EXPECT_EQ(unknown_power, conn_info.max_transmit_power);
}

TEST_F(BluetoothChromeOSTest, GetConnectionInfoForConnectedDevice) {
  GetAdapter();
  BluetoothDevice* device =
      adapter_->GetDevice(FakeBluetoothDeviceClient::kPairedDeviceAddress);

  device->Connect(nullptr, GetCallback(),
                  base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                             base::Unretained(this)));
  EXPECT_TRUE(device->IsConnected());

  // Calling GetConnectionInfo for a connected device should return valid
  // results.
  fake_bluetooth_device_client_->UpdateConnectionInfo(-10, 3, 4);
  BluetoothDevice::ConnectionInfo conn_info;
  device->GetConnectionInfo(base::Bind(&SaveConnectionInfo, &conn_info));
  EXPECT_EQ(-10, conn_info.rssi);
  EXPECT_EQ(3, conn_info.transmit_power);
  EXPECT_EQ(4, conn_info.max_transmit_power);
}

// Verifies Shutdown shuts down the adapter as expected.
TEST_F(BluetoothChromeOSTest, Shutdown) {
  // Set up adapter. Set powered & discoverable, start discovery.
  GetAdapter();
  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  base::MessageLoop::current()->Run();
  ASSERT_EQ(3, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Validate running adapter state.
  EXPECT_NE("", adapter_->GetAddress());
  EXPECT_NE("", adapter_->GetName());
  EXPECT_TRUE(adapter_->IsInitialized());
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_TRUE(adapter_->IsDiscoverable());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_EQ(2U, adapter_->GetDevices().size());
  EXPECT_NE(nullptr, adapter_->GetDevice(
                         FakeBluetoothDeviceClient::kPairedDeviceAddress));
  EXPECT_NE(dbus::ObjectPath(""), static_cast<BluetoothAdapterChromeOS*>(
                                      adapter_.get())->object_path());

  // Shutdown
  adapter_->Shutdown();

  // Validate post shutdown state by calling all BluetoothAdapterChromeOS
  // members, in declaration order:

  adapter_->Shutdown();
  // DeleteOnCorrectThread omitted as we don't want to delete in this test.
  {
    TestBluetoothAdapterObserver observer(adapter_);  // Calls AddObserver
  }  // ~TestBluetoothAdapterObserver calls RemoveObserver.
  EXPECT_EQ("", adapter_->GetAddress());
  EXPECT_EQ("", adapter_->GetName());

  adapter_->SetName("", GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetName error";

  EXPECT_TRUE(adapter_->IsInitialized());
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  adapter_->SetPowered(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetPowered error";

  EXPECT_FALSE(adapter_->IsDiscoverable());

  adapter_->SetDiscoverable(true, GetCallback(), GetErrorCallback());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "SetDiscoverable error";

  EXPECT_FALSE(adapter_->IsDiscovering());
  // CreateRfcommService will DCHECK after Shutdown().
  // CreateL2capService will DCHECK after Shutdown().

  BluetoothAudioSink::Options audio_sink_options;
  adapter_->RegisterAudioSink(
      audio_sink_options,
      base::Bind(&BluetoothChromeOSTest::AudioSinkAcquiredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::AudioSinkErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "RegisterAudioSink error";

  BluetoothAdapterChromeOS* adapter_chrome_os =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());
  EXPECT_EQ(nullptr,
            adapter_chrome_os->GetDeviceWithPath(dbus::ObjectPath("")));

  // Notify methods presume objects exist that are owned by the adapter and
  // destroyed in Shutdown(). Mocks are not attempted here that won't exist,
  // as verified below by EXPECT_EQ(0U, adapter_->GetDevices().size());
  // NotifyDeviceChanged
  // NotifyGattServiceAdded
  // NotifyGattServiceRemoved
  // NotifyGattServiceChanged
  // NotifyGattDiscoveryComplete
  // NotifyGattCharacteristicAdded
  // NotifyGattCharacteristicRemoved
  // NotifyGattDescriptorAdded
  // NotifyGattDescriptorRemoved
  // NotifyGattCharacteristicValueChanged
  // NotifyGattDescriptorValueChanged

  EXPECT_EQ(dbus::ObjectPath(""), adapter_chrome_os->object_path());

  adapter_profile_ = nullptr;

  FakeBluetoothProfileServiceProviderDelegate profile_delegate;
  adapter_chrome_os->UseProfile(
      BluetoothUUID(), dbus::ObjectPath(""),
      BluetoothProfileManagerClient::Options(), &profile_delegate,
      base::Bind(&BluetoothChromeOSTest::ProfileRegisteredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCompletionCallback,
                 base::Unretained(this)));

  EXPECT_FALSE(adapter_profile_) << "UseProfile error";
  EXPECT_EQ(0, callback_count_) << "UseProfile error";
  EXPECT_EQ(1, error_callback_count_--) << "UseProfile error";

  // Protected and private methods:

  adapter_chrome_os->RemovePairingDelegateInternal(&pairing_delegate);
  // AdapterAdded() invalid post Shutdown because it calls SetAdapter.
  adapter_chrome_os->AdapterRemoved(dbus::ObjectPath("x"));
  adapter_chrome_os->AdapterPropertyChanged(dbus::ObjectPath("x"), "");
  adapter_chrome_os->DeviceAdded(dbus::ObjectPath(""));
  adapter_chrome_os->DeviceRemoved(dbus::ObjectPath(""));
  adapter_chrome_os->DevicePropertyChanged(dbus::ObjectPath(""), "");
  adapter_chrome_os->InputPropertyChanged(dbus::ObjectPath(""), "");
  // BluetoothAgentServiceProvider::Delegate omitted, dbus will be shutdown,
  //   with the exception of Released.
  adapter_chrome_os->Released();

  adapter_chrome_os->OnRegisterAgent();
  adapter_chrome_os->OnRegisterAgentError("", "");
  adapter_chrome_os->OnRequestDefaultAgent();
  adapter_chrome_os->OnRequestDefaultAgentError("", "");

  adapter_chrome_os->OnRegisterAudioSink(
      base::Bind(&BluetoothChromeOSTest::AudioSinkAcquiredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::AudioSinkErrorCallback,
                 base::Unretained(this)),
      scoped_refptr<device::BluetoothAudioSink>());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_--) << "RegisterAudioSink error";

  // GetPairing will DCHECK after Shutdown().
  // SetAdapter will DCHECK after Shutdown().
  // SetDefaultAdapterName will DCHECK after Shutdown().
  // RemoveAdapter will DCHECK after Shutdown().
  adapter_chrome_os->PoweredChanged(false);
  adapter_chrome_os->DiscoverableChanged(false);
  adapter_chrome_os->DiscoveringChanged(false);
  adapter_chrome_os->PresentChanged(false);

  adapter_chrome_os->OnSetDiscoverable(GetCallback(), GetErrorCallback(), true);
  EXPECT_EQ(0, callback_count_) << "OnSetDiscoverable error";
  EXPECT_EQ(1, error_callback_count_--) << "OnSetDiscoverable error";

  adapter_chrome_os->OnPropertyChangeCompleted(GetCallback(),
                                               GetErrorCallback(), true);
  EXPECT_EQ(0, callback_count_) << "OnPropertyChangeCompleted error";
  EXPECT_EQ(1, error_callback_count_--) << "OnPropertyChangeCompleted error";

  adapter_chrome_os->AddDiscoverySession(nullptr, GetCallback(),
                                         GetErrorCallback());
  EXPECT_EQ(0, callback_count_) << "AddDiscoverySession error";
  EXPECT_EQ(1, error_callback_count_--) << "AddDiscoverySession error";

  adapter_chrome_os->RemoveDiscoverySession(nullptr, GetCallback(),
                                            GetErrorCallback());
  EXPECT_EQ(0, callback_count_) << "RemoveDiscoverySession error";
  EXPECT_EQ(1, error_callback_count_--) << "RemoveDiscoverySession error";

  // OnStartDiscovery tested in Shutdown_OnStartDiscovery
  // OnStartDiscoveryError tested in Shutdown_OnStartDiscoveryError
  // OnStopDiscovery tested in Shutdown_OnStopDiscovery
  // OnStopDiscoveryError tested in Shutdown_OnStopDiscoveryError

  adapter_profile_ = nullptr;

  // OnRegisterProfile SetProfileDelegate, OnRegisterProfileError, require
  // UseProfile to be set first, do so again here just before calling them.
  adapter_chrome_os->UseProfile(
      BluetoothUUID(), dbus::ObjectPath(""),
      BluetoothProfileManagerClient::Options(), &profile_delegate,
      base::Bind(&BluetoothChromeOSTest::ProfileRegisteredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCompletionCallback,
                 base::Unretained(this)));

  EXPECT_FALSE(adapter_profile_) << "UseProfile error";
  EXPECT_EQ(0, callback_count_) << "UseProfile error";
  EXPECT_EQ(1, error_callback_count_--) << "UseProfile error";

  adapter_chrome_os->SetProfileDelegate(
      BluetoothUUID(), dbus::ObjectPath(""), &profile_delegate,
      base::Bind(&BluetoothChromeOSTest::ProfileRegisteredCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCompletionCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, callback_count_) << "SetProfileDelegate error";
  EXPECT_EQ(1, error_callback_count_--) << "SetProfileDelegate error";

  adapter_chrome_os->OnRegisterProfileError(BluetoothUUID(), "", "");
  EXPECT_EQ(0, callback_count_) << "OnRegisterProfileError error";
  EXPECT_EQ(0, error_callback_count_) << "OnRegisterProfileError error";

  adapter_chrome_os->ProcessQueuedDiscoveryRequests();

  // From BluetoothAdapater:

  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      GetErrorCallback());
  EXPECT_EQ(0, callback_count_) << "StartDiscoverySession error";
  EXPECT_EQ(1, error_callback_count_--) << "StartDiscoverySession error";

  EXPECT_EQ(0U, adapter_->GetDevices().size());
  EXPECT_EQ(nullptr, adapter_->GetDevice(
                         FakeBluetoothDeviceClient::kPairedDeviceAddress));
  TestPairingDelegate pairing_delegate2;
  adapter_->AddPairingDelegate(
      &pairing_delegate2, BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
  adapter_->RemovePairingDelegate(&pairing_delegate2);
}

// Verifies post-Shutdown of discovery sessions and OnStartDiscovery.
TEST_F(BluetoothChromeOSTest, Shutdown_OnStartDiscovery) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterChromeOS* adapter_chrome_os =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    adapter_chrome_os->AddDiscoverySession(nullptr, GetCallback(),
                                           GetErrorCallback());
  }
  adapter_->Shutdown();
  adapter_chrome_os->OnStartDiscovery(GetCallback(), GetErrorCallback());

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(kNumberOfDiscoverySessions, error_callback_count_);
}

// Verifies post-Shutdown of discovery sessions and OnStartDiscoveryError.
TEST_F(BluetoothChromeOSTest, Shutdown_OnStartDiscoveryError) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterChromeOS* adapter_chrome_os =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    adapter_chrome_os->AddDiscoverySession(nullptr, GetCallback(),
                                           GetErrorCallback());
  }
  adapter_->Shutdown();
  adapter_chrome_os->OnStartDiscoveryError(GetCallback(), GetErrorCallback(),
                                           "", "");

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(kNumberOfDiscoverySessions, error_callback_count_);
}

// Verifies post-Shutdown of discovery sessions and OnStartDiscovery.
TEST_F(BluetoothChromeOSTest, Shutdown_OnStopDiscovery) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterChromeOS* adapter_chrome_os =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  // In order to queue up discovery sessions before an OnStopDiscovery call
  // RemoveDiscoverySession must be called, so Add, Start, and Remove:
  adapter_chrome_os->AddDiscoverySession(nullptr, GetCallback(),
                                         GetErrorCallback());
  adapter_chrome_os->OnStartDiscovery(GetCallback(), GetErrorCallback());
  adapter_chrome_os->RemoveDiscoverySession(nullptr, GetCallback(),
                                            GetErrorCallback());
  callback_count_ = 0;
  error_callback_count_ = 0;
  // Can now queue discovery sessions while waiting for OnStopDiscovery.
  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    adapter_chrome_os->AddDiscoverySession(nullptr, GetCallback(),
                                           GetErrorCallback());
  }
  adapter_->Shutdown();
  adapter_chrome_os->OnStopDiscovery(GetCallback());

  // 1 successful stopped discovery from RemoveDiscoverySession, and
  // kNumberOfDiscoverySessions errors from AddDiscoverySession/OnStopDiscovery.
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(kNumberOfDiscoverySessions, error_callback_count_);
}

// Verifies post-Shutdown of discovery sessions and OnStopDiscoveryError.
TEST_F(BluetoothChromeOSTest, Shutdown_OnStopDiscoveryError) {
  const int kNumberOfDiscoverySessions = 10;
  GetAdapter();
  BluetoothAdapterChromeOS* adapter_chrome_os =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());

  // In order to queue up discovery sessions before an OnStopDiscoveryError call
  // RemoveDiscoverySession must be called, so Add, Start, and Remove:
  adapter_chrome_os->AddDiscoverySession(nullptr, GetCallback(),
                                         GetErrorCallback());
  adapter_chrome_os->OnStartDiscovery(GetCallback(), GetErrorCallback());
  adapter_chrome_os->RemoveDiscoverySession(nullptr, GetCallback(),
                                            GetErrorCallback());
  callback_count_ = 0;
  error_callback_count_ = 0;
  // Can now queue discovery sessions while waiting for OnStopDiscoveryError.
  for (int i = 0; i < kNumberOfDiscoverySessions; i++) {
    adapter_chrome_os->AddDiscoverySession(nullptr, GetCallback(),
                                           GetErrorCallback());
  }
  adapter_->Shutdown();
  adapter_chrome_os->OnStopDiscoveryError(GetErrorCallback(), "", "");

  // 1 error reported to RemoveDiscoverySession because of OnStopDiscoveryError,
  // and kNumberOfDiscoverySessions errors queued with AddDiscoverySession.
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1 + kNumberOfDiscoverySessions, error_callback_count_);
}

}  // namespace chromeos
