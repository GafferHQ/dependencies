// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See network_change_notifier_android.h for design explanations.

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "net/android/network_change_notifier_android.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class NetworkChangeNotifierDelegateAndroidObserver
    : public NetworkChangeNotifierDelegateAndroid::Observer {
 public:
  NetworkChangeNotifierDelegateAndroidObserver()
      : type_notifications_count_(0), max_bandwidth_notifications_count_(0) {}

  // NetworkChangeNotifierDelegateAndroid::Observer:
  void OnConnectionTypeChanged() override { type_notifications_count_++; }

  void OnMaxBandwidthChanged(double max_bandwidth_mbps) override {
    max_bandwidth_notifications_count_++;
  }

  int type_notifications_count() const { return type_notifications_count_; }
  int bandwidth_notifications_count() const {
    return max_bandwidth_notifications_count_;
  }

 private:
  int type_notifications_count_;
  int max_bandwidth_notifications_count_;
};

class NetworkChangeNotifierObserver
    : public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  NetworkChangeNotifierObserver() : notifications_count_(0) {}

  // NetworkChangeNotifier::Observer:
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType connection_type) override {
    notifications_count_++;
  }

  int notifications_count() const {
    return notifications_count_;
  }

 private:
  int notifications_count_;
};

class DNSChangeObserver : public NetworkChangeNotifier::DNSObserver {
 public:
  DNSChangeObserver()
      : change_notifications_count_(0), initial_notifications_count_(0) {}

  // NetworkChangeNotifier::DNSObserver:
  void OnDNSChanged() override { change_notifications_count_++; }

  void OnInitialDNSConfigRead() override {
    initial_notifications_count_++;
    base::MessageLoop::current()->Quit();
  }

  int change_notifications_count() const { return change_notifications_count_; }

  int initial_notifications_count() const {
    return initial_notifications_count_;
  }

 private:
  int change_notifications_count_;
  int initial_notifications_count_;
};

}  // namespace

class BaseNetworkChangeNotifierAndroidTest : public testing::Test {
 protected:
  typedef NetworkChangeNotifier::ConnectionType ConnectionType;

  ~BaseNetworkChangeNotifierAndroidTest() override {}

  void RunTest(
      const base::Callback<int(void)>& notifications_count_getter,
      const base::Callback<ConnectionType(void)>&  connection_type_getter) {
    EXPECT_EQ(0, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
              connection_type_getter.Run());

    // Changing from online to offline should trigger a notification.
    SetOffline();
    EXPECT_EQ(1, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
              connection_type_getter.Run());

    // No notification should be triggered when the offline state hasn't
    // changed.
    SetOffline();
    EXPECT_EQ(1, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
              connection_type_getter.Run());

    // Going from offline to online should trigger a notification.
    SetOnline();
    EXPECT_EQ(2, notifications_count_getter.Run());
    EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
              connection_type_getter.Run());
  }

  void SetOnline() {
    delegate_.SetOnline();
    // Note that this is needed because base::ObserverListThreadSafe uses
    // PostTask().
    base::MessageLoop::current()->RunUntilIdle();
  }

  void SetOffline() {
    delegate_.SetOffline();
    // See comment above.
    base::MessageLoop::current()->RunUntilIdle();
  }

  NetworkChangeNotifierDelegateAndroid delegate_;
};

// Tests that NetworkChangeNotifierDelegateAndroid is initialized with the
// actual connection type rather than a hardcoded one (e.g.
// CONNECTION_UNKNOWN). Initializing the connection type to CONNECTION_UNKNOWN
// and relying on the first network change notification to set it correctly can
// be problematic in case there is a long delay between the delegate's
// construction and the notification.
TEST_F(BaseNetworkChangeNotifierAndroidTest,
       DelegateIsInitializedWithCurrentConnectionType) {
  SetOffline();
  ASSERT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            delegate_.GetCurrentConnectionType());
  // Instantiate another delegate to validate that it uses the actual
  // connection type at construction.
  scoped_ptr<NetworkChangeNotifierDelegateAndroid> other_delegate(
      new NetworkChangeNotifierDelegateAndroid());
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            other_delegate->GetCurrentConnectionType());

  // Toggle the global connectivity state and instantiate another delegate
  // again.
  SetOnline();
  ASSERT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
            delegate_.GetCurrentConnectionType());
  other_delegate.reset(new NetworkChangeNotifierDelegateAndroid());
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
            other_delegate->GetCurrentConnectionType());
}

class NetworkChangeNotifierDelegateAndroidTest
    : public BaseNetworkChangeNotifierAndroidTest {
 protected:
  NetworkChangeNotifierDelegateAndroidTest() {
    delegate_.AddObserver(&delegate_observer_);
    delegate_.AddObserver(&other_delegate_observer_);
  }

  ~NetworkChangeNotifierDelegateAndroidTest() override {
    delegate_.RemoveObserver(&delegate_observer_);
    delegate_.RemoveObserver(&other_delegate_observer_);
  }

  NetworkChangeNotifierDelegateAndroidObserver delegate_observer_;
  NetworkChangeNotifierDelegateAndroidObserver other_delegate_observer_;
};

// Tests that the NetworkChangeNotifierDelegateAndroid's observers are notified.
// A testing-only observer is used here for testing. In production the
// delegate's observers are instances of NetworkChangeNotifierAndroid.
TEST_F(NetworkChangeNotifierDelegateAndroidTest, DelegateObserverNotified) {
  // Test the logic with a single observer.
  RunTest(base::Bind(&NetworkChangeNotifierDelegateAndroidObserver::
                         type_notifications_count,
                     base::Unretained(&delegate_observer_)),
          base::Bind(
              &NetworkChangeNotifierDelegateAndroid::GetCurrentConnectionType,
              base::Unretained(&delegate_)));
  // Check that *all* the observers are notified. Both observers should have the
  // same state.
  EXPECT_EQ(delegate_observer_.type_notifications_count(),
            other_delegate_observer_.type_notifications_count());
}

class NetworkChangeNotifierAndroidTest
    : public BaseNetworkChangeNotifierAndroidTest {
 protected:
  void SetUp() override {
    IPAddressNumber dns_number;
    ASSERT_TRUE(ParseIPLiteralToNumber("8.8.8.8", &dns_number));
    dns_config_.nameservers.push_back(
        IPEndPoint(dns_number, dns_protocol::kDefaultPort));
    notifier_.reset(new NetworkChangeNotifierAndroid(&delegate_, &dns_config_));
    NetworkChangeNotifier::AddConnectionTypeObserver(
        &connection_type_observer_);
    NetworkChangeNotifier::AddConnectionTypeObserver(
        &other_connection_type_observer_);
  }

  NetworkChangeNotifierObserver connection_type_observer_;
  NetworkChangeNotifierObserver other_connection_type_observer_;
  NetworkChangeNotifier::DisableForTest disable_for_test_;
  DnsConfig dns_config_;
  scoped_ptr<NetworkChangeNotifierAndroid> notifier_;
};

// When a NetworkChangeNotifierAndroid is observing a
// NetworkChangeNotifierDelegateAndroid for network state changes, and the
// NetworkChangeNotifierDelegateAndroid's connectivity state changes, the
// NetworkChangeNotifierAndroid should reflect that state.
TEST_F(NetworkChangeNotifierAndroidTest,
       NotificationsSentToNetworkChangeNotifierAndroid) {
  RunTest(base::Bind(&NetworkChangeNotifierObserver::notifications_count,
                     base::Unretained(&connection_type_observer_)),
          base::Bind(&NetworkChangeNotifierAndroid::GetCurrentConnectionType,
                     base::Unretained(notifier_.get())));
}

// When a NetworkChangeNotifierAndroid's connection state changes, it should
// notify all of its observers.
TEST_F(NetworkChangeNotifierAndroidTest,
       NotificationsSentToClientsOfNetworkChangeNotifier) {
  RunTest(
      base::Bind(
          &NetworkChangeNotifierObserver::notifications_count,
          base::Unretained(&connection_type_observer_)),
      base::Bind(&NetworkChangeNotifier::GetConnectionType));
  // Check that *all* the observers are notified.
  EXPECT_EQ(connection_type_observer_.notifications_count(),
            other_connection_type_observer_.notifications_count());
}

TEST_F(NetworkChangeNotifierAndroidTest, MaxBandwidth) {
  SetOnline();
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_UNKNOWN,
            notifier_->GetConnectionType());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            notifier_->GetMaxBandwidth());
  SetOffline();
  EXPECT_EQ(NetworkChangeNotifier::CONNECTION_NONE,
            notifier_->GetConnectionType());
  EXPECT_EQ(0.0, notifier_->GetMaxBandwidth());
}

TEST_F(NetworkChangeNotifierDelegateAndroidTest,
       MaxBandwidthNotifiedOnConnectionChange) {
  EXPECT_EQ(0, delegate_observer_.bandwidth_notifications_count());
  SetOffline();
  EXPECT_EQ(1, delegate_observer_.bandwidth_notifications_count());
  SetOnline();
  EXPECT_EQ(2, delegate_observer_.bandwidth_notifications_count());
  SetOnline();
  EXPECT_EQ(2, delegate_observer_.bandwidth_notifications_count());
}

TEST_F(NetworkChangeNotifierAndroidTest, InitialSignal) {
  DNSChangeObserver dns_change_observer;
  NetworkChangeNotifier::AddDNSObserver(&dns_change_observer);
  base::MessageLoop::current()->Run();
  EXPECT_EQ(1, dns_change_observer.initial_notifications_count());
  EXPECT_EQ(0, dns_change_observer.change_notifications_count());
  NetworkChangeNotifier::RemoveDNSObserver(&dns_change_observer);
}

}  // namespace net
