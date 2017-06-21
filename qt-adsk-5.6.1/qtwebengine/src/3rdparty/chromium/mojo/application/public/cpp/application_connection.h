// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_CONNECTION_H_
#define MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_CONNECTION_H_

#include <string>

#include "mojo/application/public/cpp/lib/interface_factory_connector.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"

namespace mojo {

class ServiceConnector;

// Represents a connection to another application. An instance of this class is
// passed to ApplicationDelegate's ConfigureIncomingConnection() method each
// time a connection is made to this app, and to ApplicationDelegate's
// ConfigureOutgoingConnection() method when the app connects to another.
//
// To use, define a class that implements your specific service API (e.g.,
// FooImpl to implement a service named Foo). Then implement an
// InterfaceFactory<Foo> that binds instances of FooImpl to
// InterfaceRequest<Foo>s and register that on the connection like this:
//
//   connection->AddService(&factory);
//
// Or, if you have multiple factories implemented by the same type, explicitly
// specify the interface to register the factory for:
//
//   connection->AddService<Foo>(&my_foo_and_bar_factory_);
//   connection->AddService<Bar>(&my_foo_and_bar_factory_);
//
// The InterfaceFactory must outlive the ApplicationConnection.
//
// Additionally you specify a ServiceConnector. If a ServiceConnector has
// been set and an InterfaceFactory has not been registered for the interface
// request, than the interface request is sent to the ServiceConnector.
//
// Just as with InterfaceFactory, ServiceConnector must outlive
// ApplicationConnection.
//
// An ApplicationConnection's lifetime is managed by an ApplicationImpl. To
// close a connection, call CloseConnection which will destroy this object.
class ApplicationConnection {
 public:
  ApplicationConnection();

  // Closes the connection and destroys this object. This is the only valid way
  // to destroy this object.
  void CloseConnection();

  // See class description for details.
  virtual void SetServiceConnector(ServiceConnector* connector) = 0;

  // Makes Interface available as a service to the remote application.
  // |factory| will create implementations of Interface on demand.
  template <typename Interface>
  void AddService(InterfaceFactory<Interface>* factory) {
    SetServiceConnectorForName(
        new internal::InterfaceFactoryConnector<Interface>(factory),
        Interface::Name_);
  }

  // Binds |ptr| to an implemention of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // service.
  template <typename Interface>
  void ConnectToService(InterfacePtr<Interface>* ptr) {
    if (ServiceProvider* sp = GetServiceProvider()) {
      MessagePipe pipe;
      ptr->Bind(InterfacePtrInfo<Interface>(pipe.handle0.Pass(), 0u));
      sp->ConnectToService(Interface::Name_, pipe.handle1.Pass());
    }
  }

  // Returns the URL that was used by the source application to establish a
  // connection to the destination application.
  //
  // When ApplicationConnection is representing an incoming connection this can
  // be different than the URL the application was initially loaded from, if the
  // application handles multiple URLs. Note that this is the URL after all
  // URL rewriting and HTTP redirects have been performed.
  //
  // When ApplicationConnection is representing and outgoing connection, this
  // will be the same as the value returned by GetRemoveApplicationURL().
  virtual const std::string& GetConnectionURL() = 0;

  // Returns the URL identifying the remote application on this connection.
  virtual const std::string& GetRemoteApplicationURL() = 0;

  // Returns the raw proxy to the remote application's ServiceProvider
  // interface. Most applications will just use ConnectToService() instead.
  // Caller does not take ownership.
  virtual ServiceProvider* GetServiceProvider() = 0;

  // Returns the local application's ServiceProvider interface. The return
  // value is owned by this connection.
  virtual ServiceProvider* GetLocalServiceProvider() = 0;

 protected:
  virtual ~ApplicationConnection();

  // Called to give the derived type to perform some cleanup before destruction.
  virtual void OnCloseConnection() = 0;

 private:
  virtual void SetServiceConnectorForName(ServiceConnector* service_connector,
                                          const std::string& name) = 0;

  // Ensures that CloseConnection can only be called once and the
  // ApplicationConnection's destructor can only be called after the connection
  // is closed.
  bool connection_closed_;
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_CONNECTION_H_
