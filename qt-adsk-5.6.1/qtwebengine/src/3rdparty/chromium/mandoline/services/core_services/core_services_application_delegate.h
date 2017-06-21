// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_H_
#define MANDOLINE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/common/weak_binding_set.h"

namespace core_services {

class ApplicationThread;

// The CoreServices application is a singleton ServiceProvider. There is one
// instance of the CoreServices ServiceProvider.
class CoreServicesApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::ContentHandler>,
      public mojo::ContentHandler {
 public:
  CoreServicesApplicationDelegate();
  ~CoreServicesApplicationDelegate() override;

  void ApplicationThreadDestroyed(ApplicationThread* thread);

 private:
  // Overridden from mojo::ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;
  void Quit() override;

  // Overridden from mojo::InterfaceFactory<mojo::ContentHandler>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::ContentHandler> request) override;

  // Overridden from mojo::ContentHandler:
  void StartApplication(
      mojo::InterfaceRequest<mojo::Application> request,
      mojo::URLResponsePtr response) override;

  // Bindings for all of our connections.
  mojo::WeakBindingSet<ContentHandler> handler_bindings_;

  ScopedVector<ApplicationThread> application_threads_;

  base::WeakPtrFactory<CoreServicesApplicationDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CoreServicesApplicationDelegate);
};

}  // namespace core_services

#endif  // MANDONLINE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_H_
