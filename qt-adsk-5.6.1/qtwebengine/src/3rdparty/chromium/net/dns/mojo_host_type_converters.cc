// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mojo_host_type_converters.h"

#include "net/base/address_list.h"
#include "net/base/net_util.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"

namespace net {
namespace {

AddressFamily AddressFamilyFromMojo(interfaces::AddressFamily address_family) {
  switch (address_family) {
    case interfaces::ADDRESS_FAMILY_UNSPECIFIED:
      return ADDRESS_FAMILY_UNSPECIFIED;
    case interfaces::ADDRESS_FAMILY_IPV4:
      return ADDRESS_FAMILY_IPV4;
    case interfaces::ADDRESS_FAMILY_IPV6:
      return ADDRESS_FAMILY_IPV6;
  }
  NOTREACHED();
  return ADDRESS_FAMILY_UNSPECIFIED;
}

interfaces::AddressFamily AddressFamilyToMojo(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_UNSPECIFIED:
      return interfaces::ADDRESS_FAMILY_UNSPECIFIED;
    case ADDRESS_FAMILY_IPV4:
      return interfaces::ADDRESS_FAMILY_IPV4;
    case ADDRESS_FAMILY_IPV6:
      return interfaces::ADDRESS_FAMILY_IPV6;
  }
  NOTREACHED();
  return interfaces::ADDRESS_FAMILY_UNSPECIFIED;
}

}  // namespace
}  // namespace net

namespace mojo {

// static
net::HostResolver::RequestInfo
TypeConverter<net::HostResolver::RequestInfo,
              net::interfaces::HostResolverRequestInfo>::
    Convert(const net::interfaces::HostResolverRequestInfo& obj) {
  net::HostResolver::RequestInfo result(net::HostPortPair(obj.host, obj.port));
  result.set_address_family(net::AddressFamilyFromMojo(obj.address_family));
  result.set_is_my_ip_address(obj.is_my_ip_address);
  return result;
}

// static
net::interfaces::HostResolverRequestInfoPtr
TypeConverter<net::interfaces::HostResolverRequestInfoPtr,
              net::HostResolver::RequestInfo>::
    Convert(const net::HostResolver::RequestInfo& obj) {
  net::interfaces::HostResolverRequestInfoPtr result(
      net::interfaces::HostResolverRequestInfo::New());
  result->host = obj.hostname();
  result->port = obj.port();
  result->address_family = net::AddressFamilyToMojo(obj.address_family());
  result->is_my_ip_address = obj.is_my_ip_address();
  return result.Pass();
}

// static
net::interfaces::AddressListPtr
TypeConverter<net::interfaces::AddressListPtr, net::AddressList>::Convert(
    const net::AddressList& obj) {
  net::interfaces::AddressListPtr result(net::interfaces::AddressList::New());
  for (const auto& endpoint : obj) {
    net::interfaces::IPEndPointPtr ep(net::interfaces::IPEndPoint::New());
    ep->port = endpoint.port();
    ep->address = mojo::Array<uint8_t>::From(endpoint.address());
    result->addresses.push_back(ep.Pass());
  }
  return result.Pass();
}

// static
net::AddressList
TypeConverter<net::AddressList, net::interfaces::AddressList>::Convert(
    const net::interfaces::AddressList& obj) {
  net::AddressList address_list;
  for (size_t i = 0; i < obj.addresses.size(); i++) {
    const net::interfaces::IPEndPointPtr& ep = obj.addresses[i];
    address_list.push_back(
        net::IPEndPoint(ep->address.To<net::IPAddressNumber>(), ep->port));
  }
  return address_list;
}

}  // namespace mojo
