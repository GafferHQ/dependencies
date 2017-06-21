// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/child_process_host.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/runner/context.h"
#include "mojo/runner/switches.h"
#include "mojo/runner/task_runners.h"
#include "ui/gl/gl_switches.h"

namespace mojo {
namespace runner {

ChildProcessHost::ChildProcessHost(Context* context, const std::string& name)
    : context_(context), name_(name), channel_info_(nullptr) {
  platform_channel_ = platform_channel_pair_.PassServerHandle();
  DCHECK(!name.empty());
  CHECK(platform_channel_.is_valid());
}

ChildProcessHost::~ChildProcessHost() {
  if (child_process_.IsValid()) {
    LOG(WARNING) << "Destroying ChildProcessHost with unjoined child";
    child_process_.Close();
  }
}

void ChildProcessHost::Start() {
  DCHECK(!child_process_.IsValid());
  DCHECK(platform_channel_.is_valid());

  ScopedMessagePipeHandle handle(embedder::CreateChannel(
      platform_channel_.Pass(),
      base::Bind(&ChildProcessHost::DidCreateChannel, base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get()));

  controller_.Bind(InterfacePtrInfo<ChildController>(handle.Pass(), 0u));

  CHECK(base::PostTaskAndReplyWithResult(
      context_->task_runners()->blocking_pool(), FROM_HERE,
      base::Bind(&ChildProcessHost::DoLaunch, base::Unretained(this)),
      base::Bind(&ChildProcessHost::DidStart, base::Unretained(this))));
}

int ChildProcessHost::Join() {
  DCHECK(child_process_.IsValid());
  int rv = -1;
  LOG_IF(ERROR, !child_process_.WaitForExit(&rv))
      << "Failed to wait for child process";
  child_process_.Close();
  return rv;
}

void ChildProcessHost::StartApp(
    const String& app_path,
    bool clean_app_path,
    InterfaceRequest<Application> application_request,
    const ChildController::StartAppCallback& on_app_complete) {
  DCHECK(controller_);

  on_app_complete_ = on_app_complete;
  controller_->StartApp(
      app_path, clean_app_path, application_request.Pass(),
      base::Bind(&ChildProcessHost::AppCompleted, base::Unretained(this)));
}

void ChildProcessHost::ExitNow(int32_t exit_code) {
  DCHECK(controller_);

  controller_->ExitNow(exit_code);
}

void ChildProcessHost::DidStart(bool success) {
  DVLOG(2) << "ChildProcessHost::DidStart()";

  if (!success) {
    LOG(ERROR) << "Failed to start child process";
    AppCompleted(MOJO_RESULT_UNKNOWN);
    return;
  }
}

bool ChildProcessHost::DoLaunch() {
  static const char* kForwardSwitches[] = {
      switches::kOverrideUseGLWithOSMesaForTests,
      switches::kTraceToConsole,
      switches::kV,
      switches::kVModule,
      switches::kWaitForDebugger,
  };

  const base::CommandLine* parent_command_line =
      base::CommandLine::ForCurrentProcess();
  base::CommandLine child_command_line(parent_command_line->GetProgram());
  child_command_line.CopySwitchesFrom(*parent_command_line, kForwardSwitches,
                                      arraysize(kForwardSwitches));
  child_command_line.AppendSwitchASCII(switches::kApp, name_);
  child_command_line.AppendSwitch(switches::kChildProcess);

  auto args = parent_command_line->GetArgs();
  for (const auto& arg : args)
    child_command_line.AppendArgNative(arg);

  embedder::HandlePassingInformation handle_passing_info;
  platform_channel_pair_.PrepareToPassClientHandleToChildProcess(
      &child_command_line, &handle_passing_info);

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.handles_to_inherit = &handle_passing_info;
#elif defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#endif
  DVLOG(2) << "Launching child with command line: "
           << child_command_line.GetCommandLineString();
  child_process_ = base::LaunchProcess(child_command_line, options);
  if (!child_process_.IsValid())
    return false;

  platform_channel_pair_.ChildProcessLaunched();
  return true;
}

void ChildProcessHost::AppCompleted(int32_t result) {
  if (!on_app_complete_.is_null()) {
    auto on_app_complete = on_app_complete_;
    on_app_complete_.reset();
    on_app_complete.Run(result);
  }
}

void ChildProcessHost::DidCreateChannel(embedder::ChannelInfo* channel_info) {
  DVLOG(2) << "AppChildProcessHost::DidCreateChannel()";

  CHECK(channel_info);
  channel_info_ = channel_info;
}

}  // namespace runner
}  // namespace mojo
