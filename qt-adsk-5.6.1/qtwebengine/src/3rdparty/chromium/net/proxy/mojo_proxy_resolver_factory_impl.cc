// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_factory_impl.h"

#include <string>

#include "base/stl_util.h"
#include "net/base/net_errors.h"
#include "net/proxy/mojo_proxy_resolver_impl.h"
#include "net/proxy/mojo_proxy_resolver_v8_tracing_bindings.h"
#include "net/proxy/proxy_resolver_factory.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"

namespace net {
namespace {

// A class to manage the lifetime of a MojoProxyResolverImpl. An instance will
// remain while the message pipe for the mojo connection remains open.
class MojoProxyResolverHolder {
 public:
  MojoProxyResolverHolder(
      scoped_ptr<ProxyResolverV8Tracing> proxy_resolver_impl,
      mojo::InterfaceRequest<interfaces::ProxyResolver> request);

 private:
  // Mojo error handler.
  void OnConnectionError();

  MojoProxyResolverImpl mojo_proxy_resolver_;
  mojo::Binding<interfaces::ProxyResolver> binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoProxyResolverHolder);
};

MojoProxyResolverHolder::MojoProxyResolverHolder(
    scoped_ptr<ProxyResolverV8Tracing> proxy_resolver_impl,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request)
    : mojo_proxy_resolver_(proxy_resolver_impl.Pass()),
      binding_(&mojo_proxy_resolver_, request.Pass()) {
  binding_.set_connection_error_handler(base::Bind(
      &MojoProxyResolverHolder::OnConnectionError, base::Unretained(this)));
}

void MojoProxyResolverHolder::OnConnectionError() {
  delete this;
}

}  // namespace

class MojoProxyResolverFactoryImpl::Job {
 public:
  Job(MojoProxyResolverFactoryImpl* parent,
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      ProxyResolverV8TracingFactory* proxy_resolver_factory,
      mojo::InterfaceRequest<interfaces::ProxyResolver> request,
      interfaces::ProxyResolverFactoryRequestClientPtr client);
  ~Job();

 private:
  // Mojo error handler.
  void OnConnectionError();

  void OnProxyResolverCreated(int error);

  MojoProxyResolverFactoryImpl* const parent_;
  scoped_ptr<ProxyResolverV8Tracing> proxy_resolver_impl_;
  mojo::InterfaceRequest<interfaces::ProxyResolver> proxy_request_;
  ProxyResolverV8TracingFactory* factory_;
  scoped_ptr<net::ProxyResolverFactory::Request> request_;
  interfaces::ProxyResolverFactoryRequestClientPtr client_ptr_;

  base::WeakPtrFactory<interfaces::ProxyResolverFactoryRequestClient>
      weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

MojoProxyResolverFactoryImpl::Job::Job(
    MojoProxyResolverFactoryImpl* factory,
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    ProxyResolverV8TracingFactory* proxy_resolver_factory,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request,
    interfaces::ProxyResolverFactoryRequestClientPtr client)
    : parent_(factory),
      proxy_request_(request.Pass()),
      factory_(proxy_resolver_factory),
      client_ptr_(client.Pass()),
      weak_factory_(client_ptr_.get()) {
  client_ptr_.set_connection_error_handler(
      base::Bind(&MojoProxyResolverFactoryImpl::Job::OnConnectionError,
                 base::Unretained(this)));
  factory_->CreateProxyResolverV8Tracing(
      pac_script, make_scoped_ptr(new MojoProxyResolverV8TracingBindings<
                      interfaces::ProxyResolverFactoryRequestClient>(
                      weak_factory_.GetWeakPtr())),
      &proxy_resolver_impl_,
      base::Bind(&MojoProxyResolverFactoryImpl::Job::OnProxyResolverCreated,
                 base::Unretained(this)),
      &request_);
}

MojoProxyResolverFactoryImpl::Job::~Job() = default;

void MojoProxyResolverFactoryImpl::Job::OnConnectionError() {
  client_ptr_->ReportResult(ERR_PAC_SCRIPT_TERMINATED);
  parent_->RemoveJob(this);
}

void MojoProxyResolverFactoryImpl::Job::OnProxyResolverCreated(int error) {
  if (error == OK) {
    // The MojoProxyResolverHolder will delete itself if |proxy_request_|
    // encounters a connection error.
    new MojoProxyResolverHolder(proxy_resolver_impl_.Pass(),
                                proxy_request_.Pass());
  }
  client_ptr_->ReportResult(error);
  parent_->RemoveJob(this);
}

MojoProxyResolverFactoryImpl::MojoProxyResolverFactoryImpl(
    scoped_ptr<ProxyResolverV8TracingFactory> proxy_resolver_factory,
    mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request)
    : proxy_resolver_impl_factory_(proxy_resolver_factory.Pass()),
      binding_(this, request.Pass()) {
}

MojoProxyResolverFactoryImpl::MojoProxyResolverFactoryImpl(
    mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request)
    : MojoProxyResolverFactoryImpl(ProxyResolverV8TracingFactory::Create(),
                                   request.Pass()) {
}

MojoProxyResolverFactoryImpl::~MojoProxyResolverFactoryImpl() {
  STLDeleteElements(&jobs_);
}

void MojoProxyResolverFactoryImpl::CreateResolver(
    const mojo::String& pac_script,
    mojo::InterfaceRequest<interfaces::ProxyResolver> request,
    interfaces::ProxyResolverFactoryRequestClientPtr client) {
  // The Job will call RemoveJob on |this| when either the create request
  // finishes or |request| or |client| encounters a connection error.
  jobs_.insert(new Job(
      this, ProxyResolverScriptData::FromUTF8(pac_script.To<std::string>()),
      proxy_resolver_impl_factory_.get(), request.Pass(), client.Pass()));
}

void MojoProxyResolverFactoryImpl::RemoveJob(Job* job) {
  size_t erased = jobs_.erase(job);
  DCHECK_EQ(1u, erased);
  delete job;
}

}  // namespace net
