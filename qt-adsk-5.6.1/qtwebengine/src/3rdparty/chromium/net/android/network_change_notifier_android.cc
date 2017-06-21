// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

////////////////////////////////////////////////////////////////////////////////
// Threading considerations:
//
// This class is designed to meet various threading guarantees starting from the
// ones imposed by NetworkChangeNotifier:
// - The notifier can be constructed on any thread.
// - GetCurrentConnectionType() can be called on any thread.
//
// The fact that this implementation of NetworkChangeNotifier is backed by a
// Java side singleton class (see NetworkChangeNotifier.java) adds another
// threading constraint:
// - The calls to the Java side (stateful) object must be performed from a
//   single thread. This object happens to be a singleton which is used on the
//   application side on the main thread. Therefore all the method calls from
//   the native NetworkChangeNotifierAndroid class to its Java counterpart are
//   performed on the main thread.
//
// This leads to a design involving the following native classes:
// 1) NetworkChangeNotifierFactoryAndroid ('factory')
// 2) NetworkChangeNotifierDelegateAndroid ('delegate')
// 3) NetworkChangeNotifierAndroid ('notifier')
//
// The factory constructs and owns the delegate. The factory is constructed and
// destroyed on the main thread which makes it construct and destroy the
// delegate on the main thread too. This guarantees that the calls to the Java
// side are performed on the main thread.
// Note that after the factory's construction, the factory's creation method can
// be called from any thread since the delegate's construction (performing the
// JNI calls) already happened on the main thread (when the factory was
// constructed).
//
////////////////////////////////////////////////////////////////////////////////
// Propagation of network change notifications:
//
// When the factory is requested to create a new instance of the notifier, the
// factory passes the delegate to the notifier (without transferring ownership).
// Note that there is a one-to-one mapping between the factory and the
// delegate as explained above. But the factory naturally creates multiple
// instances of the notifier. That means that there is a one-to-many mapping
// between delegate and notifier (i.e. a single delegate can be shared by
// multiple notifiers).
// At construction the notifier (which is also an observer) subscribes to
// notifications fired by the delegate. These notifications, received by the
// delegate (and forwarded to the notifier(s)), are sent by the Java side
// notifier (see NetworkChangeNotifier.java) and are initiated by the Android
// platform.
// Notifications from the Java side always arrive on the main thread. The
// delegate then forwards these notifications to the threads of each observer
// (network change notifier). The network change notifier than processes the
// state change, and notifies each of its observers on their threads.
//
// This can also be seen as:
// Android platform -> NetworkChangeNotifier (Java) ->
// NetworkChangeNotifierDelegateAndroid -> NetworkChangeNotifierAndroid.

#include "net/android/network_change_notifier_android.h"

#include "base/threading/thread.h"
#include "net/base/address_tracker_linux.h"
#include "net/dns/dns_config_service_posix.h"

namespace net {

// Thread on which we can run DnsConfigService, which requires a TYPE_IO
// message loop to monitor /system/etc/hosts.
class NetworkChangeNotifierAndroid::DnsConfigServiceThread
    : public base::Thread,
      public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  DnsConfigServiceThread(const DnsConfig* dns_config_for_testing)
      : base::Thread("DnsConfigService"),
        dns_config_for_testing_(dns_config_for_testing),
        creation_time_(base::Time::Now()),
        address_tracker_(base::Bind(base::DoNothing),
                         base::Bind(base::DoNothing),
                         // We're only interested in tunnel interface changes.
                         base::Bind(NotifyNetworkChangeNotifierObservers),
                         base::hash_set<std::string>()) {}

  ~DnsConfigServiceThread() override {
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
    Stop();
  }

  void InitAfterStart() {
    DCHECK(IsRunning());
    NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  void Init() override {
    address_tracker_.Init();
    dns_config_service_.reset(new internal::DnsConfigServicePosix());
    if (dns_config_for_testing_)
      dns_config_service_->SetDnsConfigForTesting(dns_config_for_testing_);
    dns_config_service_->WatchConfig(
        base::Bind(&DnsConfigServiceThread::DnsConfigChangeCallback,
                   base::Unretained(this)));
  }

  void CleanUp() override { dns_config_service_.reset(); }

  static void NotifyNetworkChangeNotifierObservers() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
    NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
  }

 private:
  void DnsConfigChangeCallback(const DnsConfig& config) {
    DCHECK(task_runner()->BelongsToCurrentThread());
    if (dns_config_service_->SeenChangeSince(creation_time_)) {
      NetworkChangeNotifier::SetDnsConfig(config);
    } else {
      NetworkChangeNotifier::SetInitialDnsConfig(config);
    }
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation:
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) override {
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&internal::DnsConfigServicePosix::OnNetworkChanged,
                   base::Unretained(dns_config_service_.get()), type));
  }

  const DnsConfig* dns_config_for_testing_;
  const base::Time creation_time_;
  scoped_ptr<internal::DnsConfigServicePosix> dns_config_service_;
  // Used to detect tunnel state changes.
  internal::AddressTrackerLinux address_tracker_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServiceThread);
};

NetworkChangeNotifierAndroid::~NetworkChangeNotifierAndroid() {
  delegate_->RemoveObserver(this);
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierAndroid::GetCurrentConnectionType() const {
  return delegate_->GetCurrentConnectionType();
}

double NetworkChangeNotifierAndroid::GetCurrentMaxBandwidth() const {
  return delegate_->GetCurrentMaxBandwidth();
}

void NetworkChangeNotifierAndroid::OnConnectionTypeChanged() {
  DnsConfigServiceThread::NotifyNetworkChangeNotifierObservers();
}

void NetworkChangeNotifierAndroid::OnMaxBandwidthChanged(
    double max_bandwidth_mbps) {
  NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChange(
      max_bandwidth_mbps);
}

// static
bool NetworkChangeNotifierAndroid::Register(JNIEnv* env) {
  return NetworkChangeNotifierDelegateAndroid::Register(env);
}

NetworkChangeNotifierAndroid::NetworkChangeNotifierAndroid(
    NetworkChangeNotifierDelegateAndroid* delegate,
    const DnsConfig* dns_config_for_testing)
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsAndroid()),
      delegate_(delegate),
      dns_config_service_thread_(
          new DnsConfigServiceThread(dns_config_for_testing)) {
  delegate_->AddObserver(this);
  dns_config_service_thread_->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  // Wait until Init is called on the DNS config thread before
  // calling InitAfterStart.
  dns_config_service_thread_->WaitUntilThreadStarted();
  dns_config_service_thread_->InitAfterStart();
}

// static
NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierAndroid::NetworkChangeCalculatorParamsAndroid() {
  NetworkChangeCalculatorParams params;
  // IPAddressChanged is produced immediately prior to ConnectionTypeChanged
  // so delay IPAddressChanged so they get merged with the following
  // ConnectionTypeChanged signal.
  params.ip_address_offline_delay_ = base::TimeDelta::FromSeconds(1);
  params.ip_address_online_delay_ = base::TimeDelta::FromSeconds(1);
  params.connection_type_offline_delay_ = base::TimeDelta::FromSeconds(0);
  params.connection_type_online_delay_ = base::TimeDelta::FromSeconds(0);
  return params;
}

}  // namespace net
