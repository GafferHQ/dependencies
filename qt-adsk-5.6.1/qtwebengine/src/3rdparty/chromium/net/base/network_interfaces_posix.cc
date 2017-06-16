// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <set>
#include <sys/types.h>

#include "base/memory/scoped_ptr.h"

#if !defined(OS_NACL)
#include "net/base/network_interfaces_posix.h"
#include <net/if.h>
#include <netinet/in.h>
#endif  // !defined(OS_NACL)

namespace net {

#if !defined(OS_NACL)
namespace internal {

// The application layer can pass |policy| defined in net_util.h to
// request filtering out certain type of interfaces.
bool ShouldIgnoreInterface(const std::string& name, int policy) {
  // Filter out VMware interfaces, typically named vmnet1 and vmnet8,
  // which might not be useful for use cases like WebRTC.
  if ((policy & EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES) &&
      ((name.find("vmnet") != std::string::npos) ||
       (name.find("vnic") != std::string::npos))) {
    return true;
  }

  return false;
}

// Check if the address is unspecified (i.e. made of zeroes) or loopback.
bool IsLoopbackOrUnspecifiedAddress(const sockaddr* addr) {
  if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6* addr_in6 =
        reinterpret_cast<const struct sockaddr_in6*>(addr);
    const struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
    if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_UNSPECIFIED(sin6_addr)) {
      return true;
    }
  } else if (addr->sa_family == AF_INET) {
    const struct sockaddr_in* addr_in =
        reinterpret_cast<const struct sockaddr_in*>(addr);
    if (addr_in->sin_addr.s_addr == INADDR_LOOPBACK ||
        addr_in->sin_addr.s_addr == 0) {
      return true;
    }
  } else {
    // Skip non-IP addresses.
    return true;
  }
  return false;
}

}  // namespace internal
#else   // OS_NACL
bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  NOTIMPLEMENTED();
  return false;
}

std::string GetWifiSSID() {
  NOTIMPLEMENTED();
  return "";
}
#endif  // OS_NACL

WifiPHYLayerProtocol GetWifiPHYLayerProtocol() {
  return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
}

scoped_ptr<ScopedWifiOptions> SetWifiOptions(int options) {
  return scoped_ptr<ScopedWifiOptions>();
}


}  // namespace net
