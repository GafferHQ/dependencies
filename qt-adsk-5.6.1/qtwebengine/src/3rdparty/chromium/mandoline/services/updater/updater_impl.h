// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_SERVICES_UPDATER_UPDATER_IMPL_H_
#define MANDOLINE_SERVICES_UPDATER_UPDATER_IMPL_H_

#include "base/macros.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/updater/updater.mojom.h"

namespace updater {

class ApplicationImpl;
class UpdaterApp;

class UpdaterImpl : public Updater {
 public:
  UpdaterImpl(mojo::ApplicationImpl* app_impl,
              UpdaterApp* application,
              mojo::InterfaceRequest<Updater> request);
  ~UpdaterImpl() override;

  // Updater implementation:
  void GetPathForApp(const mojo::String& url,
                     const Updater::GetPathForAppCallback& callback) override;

 private:
  UpdaterApp* const application_;
  mojo::ApplicationImpl* const app_impl_;
  mojo::StrongBinding<Updater> binding_;

  DISALLOW_COPY_AND_ASSIGN(UpdaterImpl);
};

}  // namespace updater

#endif  // MANDOLINE_SERVICES_UPDATER_UPDATER_IMPL_H_
