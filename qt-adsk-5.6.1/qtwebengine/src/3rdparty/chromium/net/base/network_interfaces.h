// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_INTERFACES_H_
#define NET_BASE_NETWORK_INTERFACES_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <ws2tcpip.h>
#elif defined(OS_POSIX)
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "net/base/address_family.h"
#include "net/base/escape.h"
#include "net/base/ip_address_number.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

class GURL;

namespace base {
class Time;
}

namespace url {
struct CanonHostInfo;
struct Parsed;
}

namespace net {

// struct that is used by GetNetworkList() to represent a network
// interface.
struct NET_EXPORT NetworkInterface {
  NetworkInterface();
  NetworkInterface(const std::string& name,
                   const std::string& friendly_name,
                   uint32_t interface_index,
                   NetworkChangeNotifier::ConnectionType type,
                   const IPAddressNumber& address,
                   uint32_t prefix_length,
                   int ip_address_attributes);
  ~NetworkInterface();

  std::string name;
  std::string friendly_name;  // Same as |name| on non-Windows.
  uint32_t interface_index;  // Always 0 on Android.
  NetworkChangeNotifier::ConnectionType type;
  IPAddressNumber address;
  uint32_t prefix_length;
  int ip_address_attributes;  // Combination of |IPAddressAttributes|.
};

typedef std::vector<NetworkInterface> NetworkInterfaceList;

// Policy settings to include/exclude network interfaces.
enum HostAddressSelectionPolicy {
  INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES           = 0x0,
  EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES           = 0x1,
};

// Returns list of network interfaces except loopback interface. If an
// interface has more than one address, a separate entry is added to
// the list for each address.
// Can be called only on a thread that allows IO.
NET_EXPORT bool GetNetworkList(NetworkInterfaceList* networks,
                               int policy);

// Gets the SSID of the currently associated WiFi access point if there is one.
// Otherwise, returns empty string.
// Currently only implemented on Linux, ChromeOS and Android.
NET_EXPORT std::string GetWifiSSID();

// General category of the IEEE 802.11 (wifi) physical layer operating mode.
enum WifiPHYLayerProtocol {
  // No wifi support or no associated AP.
  WIFI_PHY_LAYER_PROTOCOL_NONE,
  // An obsolete modes introduced by the original 802.11, e.g. IR, FHSS.
  WIFI_PHY_LAYER_PROTOCOL_ANCIENT,
  // 802.11a, OFDM-based rates.
  WIFI_PHY_LAYER_PROTOCOL_A,
  // 802.11b, DSSS or HR DSSS.
  WIFI_PHY_LAYER_PROTOCOL_B,
  // 802.11g, same rates as 802.11a but compatible with 802.11b.
  WIFI_PHY_LAYER_PROTOCOL_G,
  // 802.11n, HT rates.
  WIFI_PHY_LAYER_PROTOCOL_N,
  // Unclassified mode or failure to identify.
  WIFI_PHY_LAYER_PROTOCOL_UNKNOWN
};

// Characterize the PHY mode of the currently associated access point.
// Currently only available on OS_WIN.
NET_EXPORT WifiPHYLayerProtocol GetWifiPHYLayerProtocol();

enum WifiOptions {
  // Disables background SSID scans.
  WIFI_OPTIONS_DISABLE_SCAN =  1 << 0,
  // Enables media streaming mode.
  WIFI_OPTIONS_MEDIA_STREAMING_MODE = 1 << 1
};

class NET_EXPORT ScopedWifiOptions {
 public:
  ScopedWifiOptions() {}
  virtual ~ScopedWifiOptions();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedWifiOptions);
};

// Set temporary options on all wifi interfaces.
// |options| is an ORed bitfield of WifiOptions.
// Options are automatically disabled when the scoped pointer
// is freed. Currently only available on OS_WIN.
NET_EXPORT scoped_ptr<ScopedWifiOptions> SetWifiOptions(int options);

}  // namespace net

#endif  // NET_BASE_NETWORK_INTERFACES_H_
