// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/android/android_handler_loader.h"

namespace mojo {
namespace runner {

AndroidHandlerLoader::AndroidHandlerLoader() {
}

AndroidHandlerLoader::~AndroidHandlerLoader() {
}

void AndroidHandlerLoader::Load(
    const GURL& url,
    InterfaceRequest<Application> application_request) {
  DCHECK(application_request.is_pending());
  application_.reset(
      new ApplicationImpl(&android_handler_, application_request.Pass()));
}

}  // namespace runner
}  // namespace mojo
