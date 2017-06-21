// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_network_manager.h"

#include <string>
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_byteorder.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/common/content_switches.h"
#include "net/base/ip_address_number.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"

namespace content {

namespace {

rtc::AdapterType ConvertConnectionTypeToAdapterType(
    net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
        return rtc::ADAPTER_TYPE_UNKNOWN;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
        return rtc::ADAPTER_TYPE_ETHERNET;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
        return rtc::ADAPTER_TYPE_WIFI;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
        return rtc::ADAPTER_TYPE_CELLULAR;
    default:
        return rtc::ADAPTER_TYPE_UNKNOWN;
  }
}

}  // namespace

IpcNetworkManager::IpcNetworkManager(NetworkListManager* network_list_manager)
    : network_list_manager_(network_list_manager),
      start_count_(0),
      network_list_received_(false),
      weak_factory_(this) {
  network_list_manager_->AddNetworkListObserver(this);
}

IpcNetworkManager::~IpcNetworkManager() {
  DCHECK(!start_count_);
  network_list_manager_->RemoveNetworkListObserver(this);
}

void IpcNetworkManager::StartUpdating() {
  if (network_list_received_) {
    // Post a task to avoid reentrancy.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&IpcNetworkManager::SendNetworksChangedSignal,
                              weak_factory_.GetWeakPtr()));
  }
  ++start_count_;
}

void IpcNetworkManager::StopUpdating() {
  DCHECK_GT(start_count_, 0);
  --start_count_;
}

void IpcNetworkManager::OnNetworkListChanged(
    const net::NetworkInterfaceList& list) {

  // Update flag if network list received for the first time.
  if (!network_list_received_)
    network_list_received_ = true;

  // rtc::Network uses these prefix_length to compare network
  // interfaces discovered.
  std::vector<rtc::Network*> networks;
  for (net::NetworkInterfaceList::const_iterator it = list.begin();
       it != list.end(); it++) {
    if (it->address.size() == net::kIPv4AddressSize) {
      uint32 address;
      memcpy(&address, &it->address[0], sizeof(uint32));
      address = rtc::NetworkToHost32(address);
      rtc::IPAddress prefix =
          rtc::TruncateIP(rtc::IPAddress(address), it->prefix_length);
      rtc::Network* network =
          new rtc::Network(it->name, it->name, prefix, it->prefix_length,
                           ConvertConnectionTypeToAdapterType(it->type));
      network->AddIP(rtc::IPAddress(address));
      networks.push_back(network);
    } else if (it->address.size() == net::kIPv6AddressSize) {
      in6_addr address;
      memcpy(&address, &it->address[0], sizeof(in6_addr));
      rtc::InterfaceAddress ip6_addr(address, it->ip_address_attributes);

      // Only allow non-deprecated IPv6 addresses which don't contain MAC.
      if (rtc::IPIsMacBased(ip6_addr) ||
          (it->ip_address_attributes & net::IP_ADDRESS_ATTRIBUTE_DEPRECATED)) {
        continue;
      }

      if (!rtc::IPIsPrivate(ip6_addr)) {
        rtc::IPAddress prefix =
            rtc::TruncateIP(rtc::IPAddress(ip6_addr), it->prefix_length);
        rtc::Network* network =
            new rtc::Network(it->name, it->name, prefix, it->prefix_length,
                             ConvertConnectionTypeToAdapterType(it->type));
        network->AddIP(ip6_addr);
        networks.push_back(network);
      }
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowLoopbackInPeerConnection)) {
    std::string name_v4("loopback_ipv4");
    rtc::IPAddress ip_address_v4(INADDR_LOOPBACK);
    rtc::Network* network_v4 = new rtc::Network(
        name_v4, name_v4, ip_address_v4, 32, rtc::ADAPTER_TYPE_UNKNOWN);
    network_v4->AddIP(ip_address_v4);
    networks.push_back(network_v4);

    std::string name_v6("loopback_ipv6");
    rtc::IPAddress ip_address_v6(in6addr_loopback);
    rtc::Network* network_v6 = new rtc::Network(
        name_v6, name_v6, ip_address_v6, 64, rtc::ADAPTER_TYPE_UNKNOWN);
    network_v6->AddIP(ip_address_v6);
    networks.push_back(network_v6);
  }

  bool changed = false;
  NetworkManager::Stats stats;
  MergeNetworkList(networks, &changed, &stats);
  if (changed)
    SignalNetworksChanged();

  // Send interface counts to UMA.
  UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv4Interfaces",
                           stats.ipv4_network_count);
  UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv6Interfaces",
                           stats.ipv6_network_count);
}

void IpcNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace content
