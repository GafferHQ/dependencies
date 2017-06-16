// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#endif

using device::BluetoothDevice;

namespace device {

class TestBluetoothAdapter : public BluetoothAdapter {
 public:
  TestBluetoothAdapter() {
  }

  std::string GetAddress() const override { return ""; }

  std::string GetName() const override { return ""; }

  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override {}

  bool IsInitialized() const override { return false; }

  bool IsPresent() const override { return false; }

  bool IsPowered() const override { return false; }

  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override {}

  bool IsDiscoverable() const override { return false; }

  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override {}

  bool IsDiscovering() const override { return false; }

  void StartDiscoverySessionWithFilter(
      scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
      const DiscoverySessionCallback& callback,
      const ErrorCallback& error_callback) override {
    OnStartDiscoverySession(discovery_filter.Pass(), callback);
  }

  void StartDiscoverySession(const DiscoverySessionCallback& callback,
                             const ErrorCallback& error_callback) override {}
  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {}

  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override {}

  void RegisterAudioSink(
      const BluetoothAudioSink::Options& options,
      const AcquiredCallback& callback,
      const BluetoothAudioSink::ErrorCallback& error_callback) override {}

  void RegisterAdvertisement(
      scoped_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const CreateAdvertisementErrorCallback& error_callback) override {}

  void TestErrorCallback() {}

  ScopedVector<BluetoothDiscoverySession> discovery_sessions_;

  void TestOnStartDiscoverySession(
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
    discovery_sessions_.push_back(discovery_session.Pass());
  }

  void CleanupSessions() { discovery_sessions_.clear(); }

  void InjectFilteredSession(
      scoped_ptr<device::BluetoothDiscoveryFilter> discovery_filter) {
    StartDiscoverySessionWithFilter(
        discovery_filter.Pass(),
        base::Bind(&TestBluetoothAdapter::TestOnStartDiscoverySession,
                   base::Unretained(this)),
        base::Bind(&TestBluetoothAdapter::TestErrorCallback,
                   base::Unretained(this)));
  }

 protected:
  ~TestBluetoothAdapter() override {}

  void AddDiscoverySession(BluetoothDiscoveryFilter* discovery_filter,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) override {}

  void RemoveDiscoverySession(BluetoothDiscoveryFilter* discovery_filter,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) override {}

  void SetDiscoveryFilter(scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override {}

  void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override {}
};

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  void RequestPinCode(BluetoothDevice* device) override {}
  void RequestPasskey(BluetoothDevice* device) override {}
  void DisplayPinCode(BluetoothDevice* device,
                      const std::string& pincode) override {}
  void DisplayPasskey(BluetoothDevice* device, uint32 passkey) override {}
  void KeysEntered(BluetoothDevice* device, uint32 entered) override {}
  void ConfirmPasskey(BluetoothDevice* device, uint32 passkey) override {}
  void AuthorizePairing(BluetoothDevice* device) override {}
};


TEST(BluetoothAdapterTest, NoDefaultPairingDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there is no registered pairing delegate, NULL is returned.
  EXPECT_TRUE(adapter->DefaultPairingDelegate() == NULL);
}

TEST(BluetoothAdapterTest, OneDefaultPairingDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there is one registered pairing delegate, it is returned.
  TestPairingDelegate delegate;

  adapter->AddPairingDelegate(&delegate,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);

  EXPECT_EQ(&delegate, adapter->DefaultPairingDelegate());
}

TEST(BluetoothAdapterTest, SamePriorityDelegates) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there are two registered pairing delegates of the same
  // priority, the first one registered is returned.
  TestPairingDelegate delegate1, delegate2;

  adapter->AddPairingDelegate(&delegate1,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter->AddPairingDelegate(&delegate2,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);

  EXPECT_EQ(&delegate1, adapter->DefaultPairingDelegate());

  // After unregistering the first, the second can be returned.
  adapter->RemovePairingDelegate(&delegate1);

  EXPECT_EQ(&delegate2, adapter->DefaultPairingDelegate());
}

TEST(BluetoothAdapterTest, HighestPriorityDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that when there are two registered pairing delegates, the one with
  // the highest priority is returned.
  TestPairingDelegate delegate1, delegate2;

  adapter->AddPairingDelegate(&delegate1,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter->AddPairingDelegate(&delegate2,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  EXPECT_EQ(&delegate2, adapter->DefaultPairingDelegate());
}

TEST(BluetoothAdapterTest, UnregisterDelegate) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();

  // Verify that after unregistering a delegate, NULL is returned.
  TestPairingDelegate delegate;

  adapter->AddPairingDelegate(&delegate,
                              BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_LOW);
  adapter->RemovePairingDelegate(&delegate);

  EXPECT_TRUE(adapter->DefaultPairingDelegate() == NULL);
}

TEST(BluetoothAdapterTest, GetMergedDiscoveryFilterEmpty) {
  scoped_refptr<BluetoothAdapter> adapter = new TestBluetoothAdapter();
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter;

  discovery_filter = adapter->GetMergedDiscoveryFilter();
  EXPECT_TRUE(discovery_filter.get() == nullptr);

  discovery_filter = adapter->GetMergedDiscoveryFilterMasked(nullptr);
  EXPECT_TRUE(discovery_filter.get() == nullptr);
}

TEST(BluetoothAdapterTest, GetMergedDiscoveryFilterRegular) {
  scoped_refptr<TestBluetoothAdapter> adapter = new TestBluetoothAdapter();
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter;

  // make sure adapter have one session wihout filtering.
  adapter->InjectFilteredSession(discovery_filter.Pass());

  // having one reglar session should result in no filter
  scoped_ptr<BluetoothDiscoveryFilter> resulting_filter =
      adapter->GetMergedDiscoveryFilter();
  EXPECT_TRUE(resulting_filter.get() == nullptr);

  // omiting no filter when having one reglar session should result in no filter
  resulting_filter = adapter->GetMergedDiscoveryFilterMasked(nullptr);
  EXPECT_TRUE(resulting_filter.get() == nullptr);

  adapter->CleanupSessions();
}

TEST(BluetoothAdapterTest, GetMergedDiscoveryFilterRssi) {
  scoped_refptr<TestBluetoothAdapter> adapter = new TestBluetoothAdapter();
  int16_t resulting_rssi;
  uint16_t resulting_pathloss;
  scoped_ptr<BluetoothDiscoveryFilter> resulting_filter;

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-30);
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  BluetoothDiscoveryFilter* df2 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df2->SetRSSI(-65);
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter2(df2);

  // make sure adapter have one session wihout filtering.
  adapter->InjectFilteredSession(discovery_filter.Pass());

  // DO_NOTHING should have no impact
  resulting_filter = adapter->GetMergedDiscoveryFilter();
  resulting_filter->GetRSSI(&resulting_rssi);
  EXPECT_EQ(-30, resulting_rssi);

  // should not use df2 at all, as it's not associated with adapter yet
  resulting_filter = adapter->GetMergedDiscoveryFilterMasked(df2);
  resulting_filter->GetRSSI(&resulting_rssi);
  EXPECT_EQ(-30, resulting_rssi);

  adapter->InjectFilteredSession(discovery_filter2.Pass());

  // result of merging two rssi values should be lower one
  resulting_filter = adapter->GetMergedDiscoveryFilter();
  resulting_filter->GetRSSI(&resulting_rssi);
  EXPECT_EQ(-65, resulting_rssi);

  // ommit bigger value, result should stay same
  resulting_filter = adapter->GetMergedDiscoveryFilterMasked(df);
  resulting_filter->GetRSSI(&resulting_rssi);
  EXPECT_EQ(-65, resulting_rssi);

  // ommit lower value, result should change
  resulting_filter = adapter->GetMergedDiscoveryFilterMasked(df2);
  resulting_filter->GetRSSI(&resulting_rssi);
  EXPECT_EQ(-30, resulting_rssi);

  BluetoothDiscoveryFilter* df3 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df3->SetPathloss(60);
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter3(df3);

  // when rssi and pathloss are merged, both should be cleared, becuase there is
  // no way to tell which filter will be more generic
  adapter->InjectFilteredSession(discovery_filter3.Pass());
  resulting_filter = adapter->GetMergedDiscoveryFilter();
  EXPECT_FALSE(resulting_filter->GetRSSI(&resulting_rssi));
  EXPECT_FALSE(resulting_filter->GetPathloss(&resulting_pathloss));

  adapter->CleanupSessions();
}

TEST(BluetoothAdapterTest, GetMergedDiscoveryFilterTransport) {
  scoped_refptr<TestBluetoothAdapter> adapter = new TestBluetoothAdapter();
  scoped_ptr<BluetoothDiscoveryFilter> resulting_filter;

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC);
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  BluetoothDiscoveryFilter* df2 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter2(df2);

  adapter->InjectFilteredSession(discovery_filter.Pass());

  // Just one filter, make sure transport was properly rewritten
  resulting_filter = adapter->GetMergedDiscoveryFilter();
  EXPECT_EQ(BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC,
            resulting_filter->GetTransport());

  adapter->InjectFilteredSession(discovery_filter2.Pass());

  // Two filters, should have OR of both transport's
  resulting_filter = adapter->GetMergedDiscoveryFilter();
  EXPECT_EQ(BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL,
            resulting_filter->GetTransport());

  // When 1st filter is masked, 2nd filter transport should be returned.
  resulting_filter = adapter->GetMergedDiscoveryFilterMasked(df);
  EXPECT_EQ(BluetoothDiscoveryFilter::Transport::TRANSPORT_LE,
            resulting_filter->GetTransport());

  // When 2nd filter is masked, 1st filter transport should be returned.
  resulting_filter = adapter->GetMergedDiscoveryFilterMasked(df2);
  EXPECT_EQ(BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC,
            resulting_filter->GetTransport());

  BluetoothDiscoveryFilter* df3 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df3->CopyFrom(BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter3(df3);

  // Merging empty filter in should result in empty filter
  adapter->InjectFilteredSession(discovery_filter3.Pass());
  resulting_filter = adapter->GetMergedDiscoveryFilter();
  EXPECT_TRUE(resulting_filter->IsDefault());

  adapter->CleanupSessions();
}

TEST(BluetoothAdapterTest, GetMergedDiscoveryFilterAllFields) {
  scoped_refptr<TestBluetoothAdapter> adapter = new TestBluetoothAdapter();
  int16_t resulting_rssi;
  std::set<device::BluetoothUUID> resulting_uuids;

  BluetoothDiscoveryFilter* df = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df->SetRSSI(-60);
  df->AddUUID(device::BluetoothUUID("1000"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter(df);

  BluetoothDiscoveryFilter* df2 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df2->SetRSSI(-85);
  df2->SetTransport(BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df2->AddUUID(device::BluetoothUUID("1020"));
  df2->AddUUID(device::BluetoothUUID("1001"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter2(df2);

  BluetoothDiscoveryFilter* df3 = new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE);
  df3->SetRSSI(-65);
  df3->SetTransport(BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC);
  df3->AddUUID(device::BluetoothUUID("1020"));
  df3->AddUUID(device::BluetoothUUID("1003"));
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter3(df3);

  // make sure adapter have one session wihout filtering.
  adapter->InjectFilteredSession(discovery_filter.Pass());
  adapter->InjectFilteredSession(discovery_filter2.Pass());
  adapter->InjectFilteredSession(discovery_filter3.Pass());

  scoped_ptr<BluetoothDiscoveryFilter> resulting_filter =
      adapter->GetMergedDiscoveryFilter();
  resulting_filter->GetRSSI(&resulting_rssi);
  resulting_filter->GetUUIDs(resulting_uuids);
  EXPECT_TRUE(resulting_filter->GetTransport());
  EXPECT_EQ(BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL,
            resulting_filter->GetTransport());
  EXPECT_EQ(-85, resulting_rssi);
  EXPECT_EQ(4UL, resulting_uuids.size());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1000")) !=
              resulting_uuids.end());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1001")) !=
              resulting_uuids.end());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1003")) !=
              resulting_uuids.end());
  EXPECT_TRUE(resulting_uuids.find(device::BluetoothUUID("1020")) !=
              resulting_uuids.end());

  resulting_filter = adapter->GetMergedDiscoveryFilterMasked(df);
  EXPECT_EQ(BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL,
            resulting_filter->GetTransport());

  adapter->CleanupSessions();
}

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if defined(OS_ANDROID)
TEST_F(BluetoothTest, ConstructDefaultAdapter) {
  InitWithDefaultAdapter();
  if (!adapter_->IsPresent()) {
    LOG(WARNING) << "Bluetooth adapter not present; skipping unit test.";
    return;
  }
  EXPECT_GT(adapter_->GetAddress().length(), 0u);
  EXPECT_GT(adapter_->GetName().length(), 0u);
  EXPECT_TRUE(adapter_->IsPresent());
  // Don't know on test machines if adapter will be powered or not, but
  // the call should be safe to make and consistent.
  EXPECT_EQ(adapter_->IsPowered(), adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}
#endif  // defined(OS_ANDROID)

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if defined(OS_ANDROID)
TEST_F(BluetoothTest, ConstructWithoutDefaultAdapter) {
  InitWithoutDefaultAdapter();
  EXPECT_EQ(adapter_->GetAddress(), "");
  EXPECT_EQ(adapter_->GetName(), "");
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}
#endif  // defined(OS_ANDROID)

// TODO(scheib): Enable BluetoothTest fixture tests on all platforms.
#if defined(OS_ANDROID)
TEST_F(BluetoothTest, ConstructFakeAdapter) {
  InitWithFakeAdapter();
  EXPECT_EQ(adapter_->GetAddress(), "A1:B2:C3:D4:E5:F6");
  EXPECT_EQ(adapter_->GetName(), "FakeBluetoothAdapter");
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}
#endif  // defined(OS_ANDROID)

}  // namespace device
