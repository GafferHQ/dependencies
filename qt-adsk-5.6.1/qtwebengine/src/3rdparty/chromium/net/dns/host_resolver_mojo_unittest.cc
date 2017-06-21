// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_mojo.h"

#include <string>

#include "base/memory/scoped_vector.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mojo_host_type_converters.h"
#include "net/test/event_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace net {
namespace {

void Fail(int result) {
  FAIL() << "Unexpected callback called with error " << result;
}

class MockMojoHostResolverRequest {
 public:
  MockMojoHostResolverRequest(interfaces::HostResolverRequestClientPtr client,
                              const base::Closure& error_callback);
  void OnConnectionError();

 private:
  interfaces::HostResolverRequestClientPtr client_;
  const base::Closure error_callback_;
};

MockMojoHostResolverRequest::MockMojoHostResolverRequest(
    interfaces::HostResolverRequestClientPtr client,
    const base::Closure& error_callback)
    : client_(client.Pass()), error_callback_(error_callback) {
  client_.set_connection_error_handler(base::Bind(
      &MockMojoHostResolverRequest::OnConnectionError, base::Unretained(this)));
}

void MockMojoHostResolverRequest::OnConnectionError() {
  error_callback_.Run();
}

struct HostResolverAction {
  enum Action {
    COMPLETE,
    DROP,
    RETAIN,
  };

  static scoped_ptr<HostResolverAction> ReturnError(Error error) {
    scoped_ptr<HostResolverAction> result(new HostResolverAction);
    result->error = error;
    return result.Pass();
  }

  static scoped_ptr<HostResolverAction> ReturnResult(
      const AddressList& address_list) {
    scoped_ptr<HostResolverAction> result(new HostResolverAction);
    result->addresses = interfaces::AddressList::From(address_list);
    return result.Pass();
  }

  static scoped_ptr<HostResolverAction> DropRequest() {
    scoped_ptr<HostResolverAction> result(new HostResolverAction);
    result->action = DROP;
    return result.Pass();
  }

  static scoped_ptr<HostResolverAction> RetainRequest() {
    scoped_ptr<HostResolverAction> result(new HostResolverAction);
    result->action = RETAIN;
    return result.Pass();
  }

  Action action = COMPLETE;
  interfaces::AddressListPtr addresses;
  Error error = OK;
};

class MockMojoHostResolver : public HostResolverMojo::Impl {
 public:
  explicit MockMojoHostResolver(
      const base::Closure& request_connection_error_callback);
  ~MockMojoHostResolver() override;

  void AddAction(scoped_ptr<HostResolverAction> action);

  const mojo::Array<interfaces::HostResolverRequestInfoPtr>& requests() {
    return requests_received_;
  }

  void ResolveDns(interfaces::HostResolverRequestInfoPtr request_info,
                  interfaces::HostResolverRequestClientPtr client) override;

 private:
  ScopedVector<HostResolverAction> actions_;
  size_t results_returned_ = 0;
  mojo::Array<interfaces::HostResolverRequestInfoPtr> requests_received_;
  const base::Closure request_connection_error_callback_;
  ScopedVector<MockMojoHostResolverRequest> requests_;
};

MockMojoHostResolver::MockMojoHostResolver(
    const base::Closure& request_connection_error_callback)
    : request_connection_error_callback_(request_connection_error_callback) {
}

MockMojoHostResolver::~MockMojoHostResolver() {
  EXPECT_EQ(results_returned_, actions_.size());
}

void MockMojoHostResolver::AddAction(scoped_ptr<HostResolverAction> action) {
  actions_.push_back(action.Pass());
}

void MockMojoHostResolver::ResolveDns(
    interfaces::HostResolverRequestInfoPtr request_info,
    interfaces::HostResolverRequestClientPtr client) {
  requests_received_.push_back(request_info.Pass());
  ASSERT_LE(results_returned_, actions_.size());
  switch (actions_[results_returned_]->action) {
    case HostResolverAction::COMPLETE:
      client->ReportResult(actions_[results_returned_]->error,
                           actions_[results_returned_]->addresses.Pass());
      break;
    case HostResolverAction::RETAIN:
      requests_.push_back(new MockMojoHostResolverRequest(
          client.Pass(), request_connection_error_callback_));
      break;
    case HostResolverAction::DROP:
      client.reset();
      break;
  }
  results_returned_++;
}

}  // namespace

class HostResolverMojoTest : public testing::Test {
 protected:
  enum class ConnectionErrorSource {
    REQUEST,
  };
  using Waiter = EventWaiter<ConnectionErrorSource>;

  void SetUp() override {
    mock_resolver_.reset(new MockMojoHostResolver(
        base::Bind(&Waiter::NotifyEvent, base::Unretained(&waiter_),
                   ConnectionErrorSource::REQUEST)));
    resolver_.reset(new HostResolverMojo(mock_resolver_.get()));
  }

  int Resolve(const HostResolver::RequestInfo& request_info,
              AddressList* result) {
    HostResolver::RequestHandle request_handle = nullptr;
    TestCompletionCallback callback;
    return callback.GetResult(resolver_->Resolve(
        request_info, DEFAULT_PRIORITY, result, callback.callback(),
        &request_handle, BoundNetLog()));
  }

  scoped_ptr<MockMojoHostResolver> mock_resolver_;

  scoped_ptr<HostResolverMojo> resolver_;

  Waiter waiter_;
};

TEST_F(HostResolverMojoTest, Basic) {
  AddressList address_list;
  IPAddressNumber address_number;
  ASSERT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &address_number));
  address_list.push_back(IPEndPoint(address_number, 12345));
  address_list.push_back(
      IPEndPoint(ConvertIPv4NumberToIPv6Number(address_number), 12345));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:12345"));
  AddressList result;
  EXPECT_EQ(OK, Resolve(request_info, &result));
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(address_list[0], result[0]);
  EXPECT_EQ(address_list[1], result[1]);

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  interfaces::HostResolverRequestInfo& request = *mock_resolver_->requests()[0];
  EXPECT_EQ("example.com", request.host.To<std::string>());
  EXPECT_EQ(12345, request.port);
  EXPECT_EQ(interfaces::ADDRESS_FAMILY_UNSPECIFIED, request.address_family);
  EXPECT_FALSE(request.is_my_ip_address);
}

TEST_F(HostResolverMojoTest, ResolveCachedResult) {
  AddressList address_list;
  IPAddressNumber address_number;
  ASSERT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &address_number));
  address_list.push_back(IPEndPoint(address_number, 12345));
  address_list.push_back(
      IPEndPoint(ConvertIPv4NumberToIPv6Number(address_number), 12345));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:12345"));
  AddressList result;
  ASSERT_EQ(OK, Resolve(request_info, &result));
  ASSERT_EQ(1u, mock_resolver_->requests().size());

  result.clear();
  request_info.set_host_port_pair(HostPortPair::FromString("example.com:6789"));
  EXPECT_EQ(OK, Resolve(request_info, &result));
  ASSERT_EQ(2u, result.size());
  address_list.clear();
  address_list.push_back(IPEndPoint(address_number, 6789));
  address_list.push_back(
      IPEndPoint(ConvertIPv4NumberToIPv6Number(address_number), 6789));
  EXPECT_EQ(address_list[0], result[0]);
  EXPECT_EQ(address_list[1], result[1]);
  EXPECT_EQ(1u, mock_resolver_->requests().size());

  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));
  result.clear();
  request_info.set_allow_cached_response(false);
  EXPECT_EQ(OK, Resolve(request_info, &result));
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(address_list[0], result[0]);
  EXPECT_EQ(address_list[1], result[1]);
  EXPECT_EQ(2u, mock_resolver_->requests().size());
}

TEST_F(HostResolverMojoTest, Multiple) {
  AddressList address_list;
  IPAddressNumber address_number;
  ASSERT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &address_number));
  address_list.push_back(IPEndPoint(address_number, 12345));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));
  mock_resolver_->AddAction(
      HostResolverAction::ReturnError(ERR_NAME_NOT_RESOLVED));
  HostResolver::RequestInfo request_info1(
      HostPortPair::FromString("example.com:12345"));
  request_info1.set_address_family(ADDRESS_FAMILY_IPV4);
  request_info1.set_is_my_ip_address(true);
  HostResolver::RequestInfo request_info2(
      HostPortPair::FromString("example.org:80"));
  request_info2.set_address_family(ADDRESS_FAMILY_IPV6);
  AddressList result1;
  AddressList result2;
  HostResolver::RequestHandle request_handle1 = nullptr;
  HostResolver::RequestHandle request_handle2 = nullptr;
  TestCompletionCallback callback1;
  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            resolver_->Resolve(request_info1, DEFAULT_PRIORITY, &result1,
                               callback1.callback(), &request_handle1,
                               BoundNetLog()));
  ASSERT_EQ(ERR_IO_PENDING,
            resolver_->Resolve(request_info2, DEFAULT_PRIORITY, &result2,
                               callback2.callback(), &request_handle2,
                               BoundNetLog()));
  EXPECT_EQ(OK, callback1.GetResult(ERR_IO_PENDING));
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback2.GetResult(ERR_IO_PENDING));
  ASSERT_EQ(1u, result1.size());
  EXPECT_EQ(address_list[0], result1[0]);
  ASSERT_EQ(0u, result2.size());

  ASSERT_EQ(2u, mock_resolver_->requests().size());
  interfaces::HostResolverRequestInfo& request1 =
      *mock_resolver_->requests()[0];
  EXPECT_EQ("example.com", request1.host.To<std::string>());
  EXPECT_EQ(12345, request1.port);
  EXPECT_EQ(interfaces::ADDRESS_FAMILY_IPV4, request1.address_family);
  EXPECT_TRUE(request1.is_my_ip_address);
  interfaces::HostResolverRequestInfo& request2 =
      *mock_resolver_->requests()[1];
  EXPECT_EQ("example.org", request2.host.To<std::string>());
  EXPECT_EQ(80, request2.port);
  EXPECT_EQ(interfaces::ADDRESS_FAMILY_IPV6, request2.address_family);
  EXPECT_FALSE(request2.is_my_ip_address);
}

TEST_F(HostResolverMojoTest, Error) {
  mock_resolver_->AddAction(
      HostResolverAction::ReturnError(ERR_NAME_NOT_RESOLVED));
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:8080"));
  request_info.set_address_family(ADDRESS_FAMILY_IPV4);
  AddressList result;
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, Resolve(request_info, &result));
  EXPECT_TRUE(result.empty());

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  interfaces::HostResolverRequestInfo& request = *mock_resolver_->requests()[0];
  EXPECT_EQ("example.com", request.host.To<std::string>());
  EXPECT_EQ(8080, request.port);
  EXPECT_EQ(interfaces::ADDRESS_FAMILY_IPV4, request.address_family);
  EXPECT_FALSE(request.is_my_ip_address);
}

TEST_F(HostResolverMojoTest, EmptyResult) {
  mock_resolver_->AddAction(HostResolverAction::ReturnError(OK));
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:8080"));
  AddressList result;
  EXPECT_EQ(OK, Resolve(request_info, &result));
  EXPECT_TRUE(result.empty());

  ASSERT_EQ(1u, mock_resolver_->requests().size());
}

TEST_F(HostResolverMojoTest, Cancel) {
  mock_resolver_->AddAction(HostResolverAction::RetainRequest());
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:80"));
  request_info.set_address_family(ADDRESS_FAMILY_IPV6);
  AddressList result;
  HostResolver::RequestHandle request_handle = nullptr;
  resolver_->Resolve(request_info, DEFAULT_PRIORITY, &result, base::Bind(&Fail),
                     &request_handle, BoundNetLog());
  resolver_->CancelRequest(request_handle);
  waiter_.WaitForEvent(ConnectionErrorSource::REQUEST);
  EXPECT_TRUE(result.empty());

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  interfaces::HostResolverRequestInfo& request = *mock_resolver_->requests()[0];
  EXPECT_EQ("example.com", request.host.To<std::string>());
  EXPECT_EQ(80, request.port);
  EXPECT_EQ(interfaces::ADDRESS_FAMILY_IPV6, request.address_family);
  EXPECT_FALSE(request.is_my_ip_address);
}

TEST_F(HostResolverMojoTest, ImplDropsClientConnection) {
  mock_resolver_->AddAction(HostResolverAction::DropRequest());
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:1"));
  AddressList result;
  EXPECT_EQ(ERR_FAILED, Resolve(request_info, &result));
  EXPECT_TRUE(result.empty());

  ASSERT_EQ(1u, mock_resolver_->requests().size());
  interfaces::HostResolverRequestInfo& request = *mock_resolver_->requests()[0];
  EXPECT_EQ("example.com", request.host.To<std::string>());
  EXPECT_EQ(1, request.port);
  EXPECT_EQ(interfaces::ADDRESS_FAMILY_UNSPECIFIED, request.address_family);
  EXPECT_FALSE(request.is_my_ip_address);
}

TEST_F(HostResolverMojoTest, ResolveFromCache_Miss) {
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:8080"));
  AddressList result;
  EXPECT_EQ(ERR_DNS_CACHE_MISS,
            resolver_->ResolveFromCache(request_info, &result, BoundNetLog()));
  EXPECT_TRUE(result.empty());
}

TEST_F(HostResolverMojoTest, ResolveFromCache_Hit) {
  AddressList address_list;
  IPAddressNumber address_number;
  ASSERT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &address_number));
  address_list.push_back(IPEndPoint(address_number, 12345));
  address_list.push_back(
      IPEndPoint(ConvertIPv4NumberToIPv6Number(address_number), 12345));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:12345"));
  AddressList result;
  ASSERT_EQ(OK, Resolve(request_info, &result));
  EXPECT_EQ(1u, mock_resolver_->requests().size());

  result.clear();
  EXPECT_EQ(OK,
            resolver_->ResolveFromCache(request_info, &result, BoundNetLog()));
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(address_list[0], result[0]);
  EXPECT_EQ(address_list[1], result[1]);
  EXPECT_EQ(1u, mock_resolver_->requests().size());
}

TEST_F(HostResolverMojoTest, ResolveFromCache_CacheNotAllowed) {
  AddressList address_list;
  IPAddressNumber address_number;
  ASSERT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &address_number));
  address_list.push_back(IPEndPoint(address_number, 12345));
  address_list.push_back(
      IPEndPoint(ConvertIPv4NumberToIPv6Number(address_number), 12345));
  mock_resolver_->AddAction(HostResolverAction::ReturnResult(address_list));
  HostResolver::RequestInfo request_info(
      HostPortPair::FromString("example.com:12345"));
  AddressList result;
  ASSERT_EQ(OK, Resolve(request_info, &result));
  EXPECT_EQ(1u, mock_resolver_->requests().size());

  result.clear();
  request_info.set_allow_cached_response(false);
  EXPECT_EQ(ERR_DNS_CACHE_MISS,
            resolver_->ResolveFromCache(request_info, &result, BoundNetLog()));
  EXPECT_TRUE(result.empty());
}

TEST_F(HostResolverMojoTest, GetHostCache) {
  EXPECT_TRUE(resolver_->GetHostCache());
}

}  // namespace net
