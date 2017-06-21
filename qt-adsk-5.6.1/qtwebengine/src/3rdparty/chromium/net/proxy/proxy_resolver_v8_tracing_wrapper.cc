// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_v8_tracing_wrapper.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_resolver_error_observer.h"

namespace net {
namespace {

// Returns event parameters for a PAC error message (line number + message).
scoped_ptr<base::Value> NetLogErrorCallback(
    int line_number,
    const base::string16* message,
    NetLogCaptureMode /* capture_mode */) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("line_number", line_number);
  dict->SetString("message", *message);
  return dict.Pass();
}

class BindingsImpl : public ProxyResolverV8Tracing::Bindings {
 public:
  BindingsImpl(ProxyResolverErrorObserver* error_observer,
               HostResolver* host_resolver,
               NetLog* net_log,
               const BoundNetLog& bound_net_log)
      : error_observer_(error_observer),
        host_resolver_(host_resolver),
        net_log_(net_log),
        bound_net_log_(bound_net_log) {}

  // ProxyResolverV8Tracing::Bindings overrides.
  void Alert(const base::string16& message) override {
    // Send to the NetLog.
    LogEventToCurrentRequestAndGlobally(
        NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
        NetLog::StringCallback("message", &message));
  }

  void OnError(int line_number, const base::string16& message) override {
    // Send the error to the NetLog.
    LogEventToCurrentRequestAndGlobally(
        NetLog::TYPE_PAC_JAVASCRIPT_ERROR,
        base::Bind(&NetLogErrorCallback, line_number, &message));
    if (error_observer_)
      error_observer_->OnPACScriptError(line_number, message);
  }

  HostResolver* GetHostResolver() override { return host_resolver_; }

  BoundNetLog GetBoundNetLog() override { return bound_net_log_; }

 private:
  void LogEventToCurrentRequestAndGlobally(
      NetLog::EventType type,
      const NetLog::ParametersCallback& parameters_callback) {
    bound_net_log_.AddEvent(type, parameters_callback);

    // Emit to the global NetLog event stream.
    if (net_log_)
      net_log_->AddGlobalEntry(type, parameters_callback);
  }

  ProxyResolverErrorObserver* error_observer_;
  HostResolver* host_resolver_;
  NetLog* net_log_;
  BoundNetLog bound_net_log_;
};

class ProxyResolverV8TracingWrapper : public ProxyResolver {
 public:
  ProxyResolverV8TracingWrapper(
      scoped_ptr<ProxyResolverV8Tracing> resolver_impl,
      NetLog* net_log,
      HostResolver* host_resolver,
      scoped_ptr<ProxyResolverErrorObserver> error_observer);

  int GetProxyForURL(const GURL& url,
                     ProxyInfo* results,
                     const CompletionCallback& callback,
                     RequestHandle* request,
                     const BoundNetLog& net_log) override;

  void CancelRequest(RequestHandle request) override;

  LoadState GetLoadState(RequestHandle request) const override;

 private:
  scoped_ptr<ProxyResolverV8Tracing> resolver_impl_;
  NetLog* net_log_;
  HostResolver* host_resolver_;
  scoped_ptr<ProxyResolverErrorObserver> error_observer_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverV8TracingWrapper);
};

ProxyResolverV8TracingWrapper::ProxyResolverV8TracingWrapper(
    scoped_ptr<ProxyResolverV8Tracing> resolver_impl,
    NetLog* net_log,
    HostResolver* host_resolver,
    scoped_ptr<ProxyResolverErrorObserver> error_observer)
    : resolver_impl_(resolver_impl.Pass()),
      net_log_(net_log),
      host_resolver_(host_resolver),
      error_observer_(error_observer.Pass()) {
}

int ProxyResolverV8TracingWrapper::GetProxyForURL(
    const GURL& url,
    ProxyInfo* results,
    const CompletionCallback& callback,
    RequestHandle* request,
    const BoundNetLog& net_log) {
  resolver_impl_->GetProxyForURL(
      url, results, callback, request,
      make_scoped_ptr(new BindingsImpl(error_observer_.get(), host_resolver_,
                                       net_log_, net_log)));
  return ERR_IO_PENDING;
}

void ProxyResolverV8TracingWrapper::CancelRequest(RequestHandle request) {
  resolver_impl_->CancelRequest(request);
}

LoadState ProxyResolverV8TracingWrapper::GetLoadState(
    RequestHandle request) const {
  return resolver_impl_->GetLoadState(request);
}

}  // namespace

ProxyResolverFactoryV8TracingWrapper::ProxyResolverFactoryV8TracingWrapper(
    HostResolver* host_resolver,
    NetLog* net_log,
    const base::Callback<scoped_ptr<ProxyResolverErrorObserver>()>&
        error_observer_factory)
    : ProxyResolverFactory(true),
      factory_impl_(ProxyResolverV8TracingFactory::Create()),
      host_resolver_(host_resolver),
      net_log_(net_log),
      error_observer_factory_(error_observer_factory) {
}

ProxyResolverFactoryV8TracingWrapper::~ProxyResolverFactoryV8TracingWrapper() =
    default;

int ProxyResolverFactoryV8TracingWrapper::CreateProxyResolver(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    scoped_ptr<ProxyResolver>* resolver,
    const CompletionCallback& callback,
    scoped_ptr<Request>* request) {
  scoped_ptr<scoped_ptr<ProxyResolverV8Tracing>> v8_resolver(
      new scoped_ptr<ProxyResolverV8Tracing>);
  scoped_ptr<ProxyResolverErrorObserver> error_observer =
      error_observer_factory_.Run();
  // Note: Argument evaluation order is unspecified, so make copies before
  // passing |v8_resolver| and |error_observer|.
  scoped_ptr<ProxyResolverV8Tracing>* v8_resolver_local = v8_resolver.get();
  ProxyResolverErrorObserver* error_observer_local = error_observer.get();
  factory_impl_->CreateProxyResolverV8Tracing(
      pac_script,
      make_scoped_ptr(new BindingsImpl(error_observer_local, host_resolver_,
                                       net_log_, BoundNetLog())),
      v8_resolver_local,
      base::Bind(&ProxyResolverFactoryV8TracingWrapper::OnProxyResolverCreated,
                 base::Unretained(this), base::Passed(&v8_resolver), resolver,
                 callback, base::Passed(&error_observer)),
      request);
  return ERR_IO_PENDING;
}

void ProxyResolverFactoryV8TracingWrapper::OnProxyResolverCreated(
    scoped_ptr<scoped_ptr<ProxyResolverV8Tracing>> v8_resolver,
    scoped_ptr<ProxyResolver>* resolver,
    const CompletionCallback& callback,
    scoped_ptr<ProxyResolverErrorObserver> error_observer,
    int error) {
  if (error == OK) {
    resolver->reset(new ProxyResolverV8TracingWrapper(
        v8_resolver->Pass(), net_log_, host_resolver_, error_observer.Pass()));
  }
  callback.Run(error);
}

}  // namespace net
