// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_impl.h"

#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/services/network/cookie_store_impl.h"
#include "mojo/services/network/http_server_impl.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/tcp_bound_socket_impl.h"
#include "mojo/services/network/udp_socket_impl.h"
#include "mojo/services/network/url_loader_impl.h"
#include "mojo/services/network/web_socket_impl.h"

namespace mojo {

NetworkServiceImpl::NetworkServiceImpl(
    ApplicationConnection* connection,
    NetworkContext* context,
    scoped_ptr<mojo::AppRefCount> app_refcount,
    InterfaceRequest<NetworkService> request)
    : context_(context),
      app_refcount_(app_refcount.Pass()),
      origin_(GURL(connection->GetRemoteApplicationURL()).GetOrigin()),
      binding_(this, request.Pass()) {
}

NetworkServiceImpl::~NetworkServiceImpl() {
}

void NetworkServiceImpl::GetCookieStore(InterfaceRequest<CookieStore> store) {
  new CookieStoreImpl(context_, origin_, app_refcount_->Clone(), store.Pass());
}

void NetworkServiceImpl::CreateWebSocket(InterfaceRequest<WebSocket> socket) {
  new WebSocketImpl(context_, app_refcount_->Clone(), socket.Pass());
}

void NetworkServiceImpl::CreateTCPBoundSocket(
    NetAddressPtr local_address,
    InterfaceRequest<TCPBoundSocket> bound_socket,
    const CreateTCPBoundSocketCallback& callback) {
  scoped_ptr<TCPBoundSocketImpl> bound(new TCPBoundSocketImpl(
      app_refcount_->Clone(), bound_socket.Pass()));
  int net_error = bound->Bind(local_address.Pass());
  if (net_error != net::OK) {
    callback.Run(MakeNetworkError(net_error), NetAddressPtr());
    return;
  }
  ignore_result(bound.release());  // Strongly owned by the message pipe.
  NetAddressPtr resulting_local_address(bound->GetLocalAddress());
  callback.Run(MakeNetworkError(net::OK), resulting_local_address.Pass());
}

void NetworkServiceImpl::CreateTCPConnectedSocket(
    NetAddressPtr remote_address,
    ScopedDataPipeConsumerHandle send_stream,
    ScopedDataPipeProducerHandle receive_stream,
    InterfaceRequest<TCPConnectedSocket> client_socket,
    const CreateTCPConnectedSocketCallback& callback) {
  // TODO(brettw) implement this. We need to know what type of socket to use
  // so we can create the right one (i.e. to pass to TCPSocket::Open) before
  // doing the connect.
  callback.Run(MakeNetworkError(net::ERR_NOT_IMPLEMENTED), NetAddressPtr());
}

void NetworkServiceImpl::CreateUDPSocket(InterfaceRequest<UDPSocket> request) {
  // The lifetime of this UDPSocketImpl is bound to that of the underlying pipe.
  new UDPSocketImpl(request.Pass(), app_refcount_->Clone());
}

void NetworkServiceImpl::CreateHttpServer(
    NetAddressPtr local_address,
    HttpServerDelegatePtr delegate,
    const CreateHttpServerCallback& callback) {
  HttpServerImpl::Create(local_address.Pass(), delegate.Pass(),
                         app_refcount_->Clone(), callback);
}

}  // namespace mojo
