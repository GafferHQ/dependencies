// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_IMPL_H_
#define NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_IMPL_H_

#include <set>

#include "base/callback.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace net {
class HostResolver;
class ProxyResolverErrorObserver;
class ProxyResolverV8TracingFactory;

class MojoProxyResolverFactoryImpl : public interfaces::ProxyResolverFactory {
 public:
  explicit MojoProxyResolverFactoryImpl(
      mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request);
  MojoProxyResolverFactoryImpl(
      scoped_ptr<ProxyResolverV8TracingFactory> proxy_resolver_factory,
      mojo::InterfaceRequest<interfaces::ProxyResolverFactory> request);

  ~MojoProxyResolverFactoryImpl() override;

 private:
  class Job;

  // interfaces::ProxyResolverFactory override.
  void CreateResolver(
      const mojo::String& pac_script,
      mojo::InterfaceRequest<interfaces::ProxyResolver> request,
      interfaces::ProxyResolverFactoryRequestClientPtr client) override;

  void RemoveJob(Job* job);

  const scoped_ptr<ProxyResolverV8TracingFactory> proxy_resolver_impl_factory_;
  mojo::StrongBinding<interfaces::ProxyResolverFactory> binding_;

  std::set<Job*> jobs_;

  DISALLOW_COPY_AND_ASSIGN(MojoProxyResolverFactoryImpl);
};

}  // namespace net

#endif  // NET_PROXY_MOJO_PROXY_RESOLVER_FACTORY_IMPL_H_
