// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/lib/service_registry.h"

#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/service_connector.h"

namespace mojo {
namespace internal {

ServiceRegistry::ServiceRegistry(
    ApplicationImpl* application_impl,
    const std::string& connection_url,
    const std::string& remote_url,
    ServiceProviderPtr remote_services,
    InterfaceRequest<ServiceProvider> local_services)
    : application_impl_(application_impl),
      connection_url_(connection_url),
      remote_url_(remote_url),
      local_binding_(this),
      remote_service_provider_(remote_services.Pass()) {
  if (local_services.is_pending())
    local_binding_.Bind(local_services.Pass());
}

ServiceRegistry::ServiceRegistry()
    : application_impl_(nullptr), local_binding_(this) {
}

void ServiceRegistry::SetServiceConnector(ServiceConnector* connector) {
  service_connector_registry_.set_service_connector(connector);
}

void ServiceRegistry::SetServiceConnectorForName(
    ServiceConnector* service_connector,
    const std::string& interface_name) {
  service_connector_registry_.SetServiceConnectorForName(service_connector,
                                                         interface_name);
}

ServiceProvider* ServiceRegistry::GetLocalServiceProvider() {
  return this;
}

void ServiceRegistry::RemoveServiceConnectorForName(
    const std::string& interface_name) {
  service_connector_registry_.RemoveServiceConnectorForName(interface_name);
  if (service_connector_registry_.empty())
    remote_service_provider_.reset();
}

const std::string& ServiceRegistry::GetConnectionURL() {
  return connection_url_;
}

const std::string& ServiceRegistry::GetRemoteApplicationURL() {
  return remote_url_;
}

ServiceProvider* ServiceRegistry::GetServiceProvider() {
  return remote_service_provider_.get();
}

ServiceRegistry::~ServiceRegistry() {
}

void ServiceRegistry::OnCloseConnection() {
  if (application_impl_)
    application_impl_->CloseConnection(this);
}

void ServiceRegistry::ConnectToService(const mojo::String& service_name,
                                       ScopedMessagePipeHandle client_handle) {
  service_connector_registry_.ConnectToService(this, service_name,
                                               client_handle.Pass());
}

}  // namespace internal
}  // namespace mojo
