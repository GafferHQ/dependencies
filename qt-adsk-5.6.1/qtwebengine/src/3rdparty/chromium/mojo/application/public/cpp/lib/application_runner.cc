// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/application_runner.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/common/message_pump_mojo.h"

namespace mojo {

int g_application_runner_argc;
const char* const* g_application_runner_argv;

ApplicationRunner::ApplicationRunner(ApplicationDelegate* delegate)
    : delegate_(scoped_ptr<ApplicationDelegate>(delegate)),
      message_loop_type_(base::MessageLoop::TYPE_CUSTOM),
      has_run_(false) {}

ApplicationRunner::~ApplicationRunner() {}

void ApplicationRunner::InitBaseCommandLine() {
  base::CommandLine::Init(g_application_runner_argc, g_application_runner_argv);
}

void ApplicationRunner::set_message_loop_type(base::MessageLoop::Type type) {
  DCHECK_NE(base::MessageLoop::TYPE_CUSTOM, type);
  DCHECK(!has_run_);

  message_loop_type_ = type;
}

MojoResult ApplicationRunner::Run(MojoHandle application_request_handle,
                                  bool init_base) {
  DCHECK(!has_run_);
  has_run_ = true;

  scoped_ptr<base::AtExitManager> at_exit;
  if (init_base) {
    InitBaseCommandLine();
    at_exit.reset(new base::AtExitManager);
#ifndef NDEBUG
    base::debug::EnableInProcessStackDumping();
#endif
  }

  {
    scoped_ptr<base::MessageLoop> loop;
    if (message_loop_type_ == base::MessageLoop::TYPE_CUSTOM)
      loop.reset(new base::MessageLoop(common::MessagePumpMojo::Create()));
    else
      loop.reset(new base::MessageLoop(message_loop_type_));

    ApplicationImpl impl(delegate_.get(),
                         MakeRequest<Application>(MakeScopedHandle(
                             MessagePipeHandle(application_request_handle))));
    loop->Run();
    // It's very common for the delegate to cache the app and terminate on
    // errors. If we don't delete the delegate before the app we run the risk
    // of the delegate having a stale reference to the app and trying to use it.
    // Note that we destruct the message loop first because that might trigger
    // connection error handlers and they might access objects created by the
    // delegate.
    loop.reset();
    delegate_.reset();
  }
  return MOJO_RESULT_OK;
}

MojoResult ApplicationRunner::Run(MojoHandle application_request_handle) {
  return Run(application_request_handle, true);
}

}  // namespace mojo
