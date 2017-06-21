// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <iphlpapi.h>
#include <wlanapi.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces_win.h"
#include "url/gurl.h"

namespace net {

namespace {

// Converts Windows defined types to NetworkInterfaceType.
NetworkChangeNotifier::ConnectionType GetNetworkInterfaceType(DWORD ifType) {
  // Bail out for pre-Vista versions of Windows which are documented to give
  // inaccurate results like returning Ethernet for WiFi.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa366058.aspx
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;

  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::CONNECTION_UNKNOWN;
  if (ifType == IF_TYPE_ETHERNET_CSMACD) {
    type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  } else if (ifType == IF_TYPE_IEEE80211) {
    type = NetworkChangeNotifier::CONNECTION_WIFI;
  }
  // TODO(mallinath) - Cellular?
  return type;
}

}  // namespace

namespace internal {

base::LazyInstance<WlanApi>::Leaky lazy_wlanapi =
  LAZY_INSTANCE_INITIALIZER;

WlanApi& WlanApi::GetInstance() {
  return lazy_wlanapi.Get();
}

WlanApi::WlanApi() : initialized(false) {
  // Use an absolute path to load the DLL to avoid DLL preloading attacks.
  static const wchar_t* const kDLL = L"%WINDIR%\\system32\\wlanapi.dll";
  wchar_t path[MAX_PATH] = {0};
  ExpandEnvironmentStrings(kDLL, path, arraysize(path));
  module = ::LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!module)
    return;

  open_handle_func = reinterpret_cast<WlanOpenHandleFunc>(
      ::GetProcAddress(module, "WlanOpenHandle"));
  enum_interfaces_func = reinterpret_cast<WlanEnumInterfacesFunc>(
      ::GetProcAddress(module, "WlanEnumInterfaces"));
  query_interface_func = reinterpret_cast<WlanQueryInterfaceFunc>(
      ::GetProcAddress(module, "WlanQueryInterface"));
  set_interface_func = reinterpret_cast<WlanSetInterfaceFunc>(
      ::GetProcAddress(module, "WlanSetInterface"));
  free_memory_func = reinterpret_cast<WlanFreeMemoryFunc>(
      ::GetProcAddress(module, "WlanFreeMemory"));
  close_handle_func = reinterpret_cast<WlanCloseHandleFunc>(
      ::GetProcAddress(module, "WlanCloseHandle"));
  initialized = open_handle_func && enum_interfaces_func &&
      query_interface_func && set_interface_func &&
      free_memory_func && close_handle_func;
}

bool GetNetworkListImpl(NetworkInterfaceList* networks,
                        int policy,
                        bool is_xp,
                        const IP_ADAPTER_ADDRESSES* adapters) {
  for (const IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != NULL;
       adapter = adapter->Next) {
    // Ignore the loopback device.
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
      continue;
    }

    if (adapter->OperStatus != IfOperStatusUp) {
      continue;
    }

    // Ignore any HOST side vmware adapters with a description like:
    // VMware Virtual Ethernet Adapter for VMnet1
    // but don't ignore any GUEST side adapters with a description like:
    // VMware Accelerated AMD PCNet Adapter #2
    if ((policy & EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES) &&
        strstr(adapter->AdapterName, "VMnet") != NULL) {
      continue;
    }

    for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
         address; address = address->Next) {
      int family = address->Address.lpSockaddr->sa_family;
      if (family == AF_INET || family == AF_INET6) {
        IPEndPoint endpoint;
        if (endpoint.FromSockAddr(address->Address.lpSockaddr,
                                  address->Address.iSockaddrLength)) {
          // XP has no OnLinkPrefixLength field.
          size_t prefix_length = is_xp ? 0 : address->OnLinkPrefixLength;
          if (is_xp) {
            // Prior to Windows Vista the FirstPrefix pointed to the list with
            // single prefix for each IP address assigned to the adapter.
            // Order of FirstPrefix does not match order of FirstUnicastAddress,
            // so we need to find corresponding prefix.
            for (IP_ADAPTER_PREFIX* prefix = adapter->FirstPrefix; prefix;
                 prefix = prefix->Next) {
              int prefix_family = prefix->Address.lpSockaddr->sa_family;
              IPEndPoint network_endpoint;
              if (prefix_family == family &&
                  network_endpoint.FromSockAddr(prefix->Address.lpSockaddr,
                      prefix->Address.iSockaddrLength) &&
                  IPNumberMatchesPrefix(endpoint.address(),
                                        network_endpoint.address(),
                                        prefix->PrefixLength)) {
                prefix_length =
                    std::max<size_t>(prefix_length, prefix->PrefixLength);
              }
            }
          }

          // If the duplicate address detection (DAD) state is not changed to
          // Preferred, skip this address.
          if (address->DadState != IpDadStatePreferred) {
            continue;
          }

          uint32_t index =
              (family == AF_INET) ? adapter->IfIndex : adapter->Ipv6IfIndex;

          // From http://technet.microsoft.com/en-us/ff568768(v=vs.60).aspx, the
          // way to identify a temporary IPv6 Address is to check if
          // PrefixOrigin is equal to IpPrefixOriginRouterAdvertisement and
          // SuffixOrigin equal to IpSuffixOriginRandom.
          int ip_address_attributes = IP_ADDRESS_ATTRIBUTE_NONE;
          if (family == AF_INET6) {
            if (address->PrefixOrigin == IpPrefixOriginRouterAdvertisement &&
                address->SuffixOrigin == IpSuffixOriginRandom) {
              ip_address_attributes |= IP_ADDRESS_ATTRIBUTE_TEMPORARY;
            }
            if (address->PreferredLifetime == 0) {
              ip_address_attributes |= IP_ADDRESS_ATTRIBUTE_DEPRECATED;
            }
          }
          networks->push_back(NetworkInterface(
              adapter->AdapterName,
              base::SysWideToNativeMB(adapter->FriendlyName), index,
              GetNetworkInterfaceType(adapter->IfType), endpoint.address(),
              prefix_length, ip_address_attributes));
        }
      }
    }
  }
  return true;
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  bool is_xp = base::win::GetVersion() < base::win::VERSION_VISTA;
  ULONG len = 0;
  ULONG flags = is_xp ? GAA_FLAG_INCLUDE_PREFIX : 0;
  // GetAdaptersAddresses() may require IO operations.
  base::ThreadRestrictions::AssertIOAllowed();
  ULONG result = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, NULL, &len);
  if (result != ERROR_BUFFER_OVERFLOW) {
    // There are 0 networks.
    return true;
  }
  scoped_ptr<char[]> buf(new char[len]);
  IP_ADAPTER_ADDRESSES* adapters =
      reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get());
  result = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapters, &len);
  if (result != NO_ERROR) {
    LOG(ERROR) << "GetAdaptersAddresses failed: " << result;
    return false;
  }

  return internal::GetNetworkListImpl(networks, policy, is_xp, adapters);
}

WifiPHYLayerProtocol GetWifiPHYLayerProtocol() {
  const internal::WlanApi& wlanapi = internal::WlanApi::GetInstance();
  if (!wlanapi.initialized)
    return WIFI_PHY_LAYER_PROTOCOL_NONE;

  internal::WlanHandle client;
  DWORD cur_version = 0;
  const DWORD kMaxClientVersion = 2;
  {
    // TODO(rtenneti): Remove ScopedTracker below once crbug.com/422516 is
    // fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("422516 OpenHandle()"));
    DWORD result = wlanapi.OpenHandle(kMaxClientVersion, &cur_version, &client);
    if (result != ERROR_SUCCESS)
      return WIFI_PHY_LAYER_PROTOCOL_NONE;
  }

  WLAN_INTERFACE_INFO_LIST* interface_list_ptr = NULL;
  DWORD result =
      wlanapi.enum_interfaces_func(client.Get(), NULL, &interface_list_ptr);
  if (result != ERROR_SUCCESS)
    return WIFI_PHY_LAYER_PROTOCOL_NONE;
  scoped_ptr<WLAN_INTERFACE_INFO_LIST, internal::WlanApiDeleter> interface_list(
      interface_list_ptr);

  // Assume at most one connected wifi interface.
  WLAN_INTERFACE_INFO* info = NULL;
  for (unsigned i = 0; i < interface_list->dwNumberOfItems; ++i) {
    if (interface_list->InterfaceInfo[i].isState ==
        wlan_interface_state_connected) {
      info = &interface_list->InterfaceInfo[i];
      break;
    }
  }

  if (info == NULL)
    return WIFI_PHY_LAYER_PROTOCOL_NONE;

  WLAN_CONNECTION_ATTRIBUTES* conn_info_ptr;
  DWORD conn_info_size = 0;
  WLAN_OPCODE_VALUE_TYPE op_code;
  result = wlanapi.query_interface_func(
      client.Get(), &info->InterfaceGuid, wlan_intf_opcode_current_connection,
      NULL, &conn_info_size, reinterpret_cast<VOID**>(&conn_info_ptr),
      &op_code);
  if (result != ERROR_SUCCESS)
    return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
  scoped_ptr<WLAN_CONNECTION_ATTRIBUTES, internal::WlanApiDeleter> conn_info(
      conn_info_ptr);

  switch (conn_info->wlanAssociationAttributes.dot11PhyType) {
    case dot11_phy_type_fhss:
      return WIFI_PHY_LAYER_PROTOCOL_ANCIENT;
    case dot11_phy_type_dsss:
      return WIFI_PHY_LAYER_PROTOCOL_B;
    case dot11_phy_type_irbaseband:
      return WIFI_PHY_LAYER_PROTOCOL_ANCIENT;
    case dot11_phy_type_ofdm:
      return WIFI_PHY_LAYER_PROTOCOL_A;
    case dot11_phy_type_hrdsss:
      return WIFI_PHY_LAYER_PROTOCOL_B;
    case dot11_phy_type_erp:
      return WIFI_PHY_LAYER_PROTOCOL_G;
    case dot11_phy_type_ht:
      return WIFI_PHY_LAYER_PROTOCOL_N;
    default:
      return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
  }
}

// Note: There is no need to explicitly set the options back
// as the OS will automatically set them back when the WlanHandle
// is closed.
class WifiOptionSetter : public ScopedWifiOptions {
 public:
  WifiOptionSetter(int options) {
    const internal::WlanApi& wlanapi = internal::WlanApi::GetInstance();
    if (!wlanapi.initialized)
      return;

    DWORD cur_version = 0;
    const DWORD kMaxClientVersion = 2;
    DWORD result = wlanapi.OpenHandle(
        kMaxClientVersion, &cur_version, &client_);
    if (result != ERROR_SUCCESS)
      return;

    WLAN_INTERFACE_INFO_LIST* interface_list_ptr = NULL;
    result = wlanapi.enum_interfaces_func(client_.Get(), NULL,
                                          &interface_list_ptr);
    if (result != ERROR_SUCCESS)
      return;
    scoped_ptr<WLAN_INTERFACE_INFO_LIST, internal::WlanApiDeleter>
        interface_list(interface_list_ptr);

    for (unsigned i = 0; i < interface_list->dwNumberOfItems; ++i) {
      WLAN_INTERFACE_INFO* info = &interface_list->InterfaceInfo[i];
      if (options & WIFI_OPTIONS_DISABLE_SCAN) {
        BOOL data = false;
        wlanapi.set_interface_func(client_.Get(),
                                   &info->InterfaceGuid,
                                   wlan_intf_opcode_background_scan_enabled,
                                   sizeof(data),
                                   &data,
                                   NULL);
      }
      if (options & WIFI_OPTIONS_MEDIA_STREAMING_MODE) {
        BOOL data = true;
        wlanapi.set_interface_func(client_.Get(),
                                   &info->InterfaceGuid,
                                   wlan_intf_opcode_media_streaming_mode,
                                   sizeof(data),
                                   &data,
                                   NULL);
      }
    }
  }

 private:
  internal::WlanHandle client_;
};

scoped_ptr<ScopedWifiOptions> SetWifiOptions(int options) {
  return scoped_ptr<ScopedWifiOptions>(new WifiOptionSetter(options));
}

std::string GetWifiSSID() {
  NOTIMPLEMENTED();
  return "";
}

}  // namespace net
