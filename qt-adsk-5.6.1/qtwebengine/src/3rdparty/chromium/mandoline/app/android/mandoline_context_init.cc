// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mandoline/app/core_services_initialization.h"
#include "mojo/runner/context.h"

namespace mojo {
namespace runner {

void InitContext(Context* context) {
  mandoline::InitCoreServicesForContext(context);
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&Context::Run,
                 base::Unretained(context),
                 GURL("mojo:browser")));
}

}  // namespace runner
}  // namespace mojo
