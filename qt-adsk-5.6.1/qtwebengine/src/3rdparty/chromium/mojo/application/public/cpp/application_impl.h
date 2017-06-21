// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_IMPL_H_
#define MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/lib/service_registry.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// Utility class for communicating with the Shell, and providing Services
// to clients.
//
// To use define a class that implements your specific server api, e.g. FooImpl
// to implement a service named Foo.
// That class must subclass an InterfaceImpl specialization.
//
// If there is context that is to be shared amongst all instances, define a
// constructor with that class as its only argument, otherwise define an empty
// constructor.
//
// class FooImpl : public InterfaceImpl<Foo> {
//  public:
//   FooImpl(ApplicationContext* app_context) {}
// };
//
// or
//
// class BarImpl : public InterfaceImpl<Bar> {
//  public:
//   // contexts will remain valid for the lifetime of BarImpl.
//   BarImpl(ApplicationContext* app_context, BarContext* service_context)
//          : app_context_(app_context), servicecontext_(context) {}
//
// Create an ApplicationImpl instance that collects any service implementations.
//
// ApplicationImpl app(service_provider_handle);
// app.AddService<FooImpl>();
//
// BarContext context;
// app.AddService<BarImpl>(&context);
//
//
class ApplicationImpl : public Application,
                        public ErrorHandler {
 public:
  // Does not take ownership of |delegate|, which must remain valid for the
  // lifetime of ApplicationImpl.
  ApplicationImpl(ApplicationDelegate* delegate,
                  InterfaceRequest<Application> request);
  // Constructs an ApplicationImpl with a custom termination closure. This
  // closure is invoked on Terminate() instead of the default behavior of
  // quitting the current MessageLoop.
  ApplicationImpl(ApplicationDelegate* delegate,
                  InterfaceRequest<Application> request,
                  const base::Closure& termination_closure);
  ~ApplicationImpl() override;

  // The Mojo shell. This will return a valid pointer after Initialize() has
  // been invoked. It will remain valid until UnbindConnections() is invoked or
  // the ApplicationImpl is destroyed.
  Shell* shell() const { return shell_.get(); }

  const std::string& url() const { return url_; }

  AppLifetimeHelper* app_lifetime_helper() { return &app_lifetime_helper_; }

  // Requests a new connection to an application. Returns a pointer to the
  // connection if the connection is permitted by this application's delegate,
  // or nullptr otherwise. Caller does not take ownership. The pointer remains
  // valid until an error occurs on the connection with the Shell, until the
  // ApplicationImpl is destroyed, or until the connection is closed through a
  // call to ApplicationConnection::CloseConnection.
  ApplicationConnection* ConnectToApplication(mojo::URLRequestPtr request);

  // Closes the |connection|.
  void CloseConnection(ApplicationConnection* connection);

  // Connect to application identified by |request->url| and connect to the
  // service implementation of the interface identified by |Interface|.
  template <typename Interface>
  void ConnectToService(mojo::URLRequestPtr request,
                        InterfacePtr<Interface>* ptr) {
    ApplicationConnection* connection = ConnectToApplication(request.Pass());
    if (!connection)
      return;
    connection->ConnectToService(ptr);
  }

  // Application implementation.
  void Initialize(ShellPtr shell, const mojo::String& url) override;

  // Block until the Application is initialized, if it is not already.
  void WaitForInitialize();

  // Unbinds the Shell and Application connections. Can be used to re-bind the
  // handles to another implementation of ApplicationImpl, for instance when
  // running apptests.
  void UnbindConnections(InterfaceRequest<Application>* application_request,
                         ShellPtr* shell);

  // Quits the main run loop for this application. It first checks with the
  // shell to ensure there are no outstanding service requests.
  void Terminate();

  // Quits without waiting to check with the shell.
  void QuitNow();

 private:
  // Application implementation.
  void AcceptConnection(const String& requestor_url,
                        InterfaceRequest<ServiceProvider> services,
                        ServiceProviderPtr exposed_services,
                        const String& url) override;
  void OnQuitRequested(const Callback<void(bool)>& callback) override;

  // ErrorHandler implementation.
  void OnConnectionError() override;

  void ClearConnections();

  typedef std::vector<internal::ServiceRegistry*> ServiceRegistryList;

  ServiceRegistryList incoming_service_registries_;
  ServiceRegistryList outgoing_service_registries_;
  ApplicationDelegate* delegate_;
  Binding<Application> binding_;
  ShellPtr shell_;
  std::string url_;
  base::Closure termination_closure_;
  AppLifetimeHelper app_lifetime_helper_;
  bool quit_requested_;
  bool in_destructor_;
  base::WeakPtrFactory<ApplicationImpl> weak_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationImpl);
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_IMPL_H_
