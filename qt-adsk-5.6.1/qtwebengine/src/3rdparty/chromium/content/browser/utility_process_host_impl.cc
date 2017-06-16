// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_process_host_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/mojo/mojo_application_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_switches.h"
#include "ui/base/ui_base_switches.h"

namespace content {

// NOTE: changes to this class need to be reviewed by the security team.
class UtilitySandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  UtilitySandboxedProcessLauncherDelegate(const base::FilePath& exposed_dir,
                                          bool launch_elevated,
                                          bool no_sandbox,
                                          const base::EnvironmentMap& env,
                                          ChildProcessHost* host)
      : exposed_dir_(exposed_dir),
#if defined(OS_WIN)
        launch_elevated_(launch_elevated)
#elif defined(OS_POSIX)
        env_(env),
        no_sandbox_(no_sandbox),
        ipc_fd_(host->TakeClientFileDescriptor())
#endif  // OS_WIN
  {}

  ~UtilitySandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool ShouldLaunchElevated() override { return launch_elevated_; }
  void PreSandbox(bool* disable_default_policy,
                  base::FilePath* exposed_dir) override {
    *exposed_dir = exposed_dir_;
  }
#elif defined(OS_POSIX)

  bool ShouldUseZygote() override {
    return !no_sandbox_ && exposed_dir_.empty();
  }
  base::EnvironmentMap GetEnvironment() override { return env_; }
  base::ScopedFD TakeIpcFd() override { return ipc_fd_.Pass(); }
#endif  // OS_WIN

  SandboxType GetSandboxType() override {
    return SANDBOX_TYPE_UTILITY;
  }

 private:
  base::FilePath exposed_dir_;

#if defined(OS_WIN)
  bool launch_elevated_;
#elif defined(OS_POSIX)
  base::EnvironmentMap env_;
  bool no_sandbox_;
  base::ScopedFD ipc_fd_;
#endif  // OS_WIN
};

UtilityMainThreadFactoryFunction g_utility_main_thread_factory = NULL;

UtilityProcessHost* UtilityProcessHost::Create(
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner) {
  return new UtilityProcessHostImpl(client, client_task_runner);
}

void UtilityProcessHostImpl::RegisterUtilityMainThreadFactory(
    UtilityMainThreadFactoryFunction create) {
  g_utility_main_thread_factory = create;
}

UtilityProcessHostImpl::UtilityProcessHostImpl(
    const scoped_refptr<UtilityProcessHostClient>& client,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner)
    : client_(client),
      client_task_runner_(client_task_runner),
      is_batch_mode_(false),
      is_mdns_enabled_(false),
      no_sandbox_(false),
      run_elevated_(false),
#if defined(OS_LINUX)
      child_flags_(ChildProcessHost::CHILD_ALLOW_SELF),
#else
      child_flags_(ChildProcessHost::CHILD_NORMAL),
#endif
      started_(false),
      name_(base::ASCIIToUTF16("utility process")) {
}

UtilityProcessHostImpl::~UtilityProcessHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_batch_mode_)
    EndBatchMode();

  // We could be destroyed as a result of Chrome shutdown. When that happens,
  // the Mojo channel doesn't get the opportunity to shut down cleanly because
  // it posts to the IO thread (the current thread) which is being destroyed.
  // To guarantee proper shutdown of the Mojo channel, do it explicitly here.
  if (mojo_application_host_)
    mojo_application_host_->ShutdownOnIOThread();
}

bool UtilityProcessHostImpl::Send(IPC::Message* message) {
  if (!StartProcess())
    return false;

  return process_->Send(message);
}

bool UtilityProcessHostImpl::StartBatchMode()  {
  CHECK(!is_batch_mode_);
  is_batch_mode_ = StartProcess();
  Send(new UtilityMsg_BatchMode_Started());
  return is_batch_mode_;
}

void UtilityProcessHostImpl::EndBatchMode()  {
  CHECK(is_batch_mode_);
  is_batch_mode_ = false;
  Send(new UtilityMsg_BatchMode_Finished());
}

void UtilityProcessHostImpl::SetExposedDir(const base::FilePath& dir) {
  exposed_dir_ = dir;
}

void UtilityProcessHostImpl::EnableMDns() {
  is_mdns_enabled_ = true;
}

void UtilityProcessHostImpl::DisableSandbox() {
  no_sandbox_ = true;
}

#if defined(OS_WIN)
void UtilityProcessHostImpl::ElevatePrivileges() {
  no_sandbox_ = true;
  run_elevated_ = true;
}
#endif

const ChildProcessData& UtilityProcessHostImpl::GetData() {
  return process_->GetData();
}

#if defined(OS_POSIX)

void UtilityProcessHostImpl::SetEnv(const base::EnvironmentMap& env) {
  env_ = env;
}

#endif  // OS_POSIX

bool UtilityProcessHostImpl::StartMojoMode() {
  CHECK(!mojo_application_host_);
  mojo_application_host_.reset(new MojoApplicationHost);

  bool mojo_result = mojo_application_host_->Init();
  if (!mojo_result)
    return false;

  return StartProcess();
}

ServiceRegistry* UtilityProcessHostImpl::GetServiceRegistry() {
  if (mojo_application_host_)
    return mojo_application_host_->service_registry();
  return nullptr;
}

void UtilityProcessHostImpl::SetName(const base::string16& name) {
  name_ = name;
}

bool UtilityProcessHostImpl::StartProcess() {
  if (started_)
    return true;
  started_ = true;

  if (is_batch_mode_)
    return true;

  // Name must be set or metrics_service will crash in any test which
  // launches a UtilityProcessHost.
  process_.reset(new BrowserChildProcessHostImpl(PROCESS_TYPE_UTILITY, this));
  process_->SetName(name_);

  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

  if (RenderProcessHost::run_renderer_in_process()) {
    DCHECK(g_utility_main_thread_factory);
    // See comment in RenderProcessHostImpl::Init() for the background on why we
    // support single process mode this way.
    in_process_thread_.reset(
        g_utility_main_thread_factory(InProcessChildThreadParams(
            channel_id, BrowserThread::UnsafeGetMessageLoopForThread(
                            BrowserThread::IO)->task_runner())));
    in_process_thread_->Start();
    OnProcessLaunched();
  } else {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    int child_flags = child_flags_;

    bool has_cmd_prefix = browser_command_line.HasSwitch(
        switches::kUtilityCmdPrefix);

    // When running under gdb, forking /proc/self/exe ends up forking the gdb
    // executable instead of Chromium. It is almost safe to assume that no
    // updates will happen while a developer is running with
    // |switches::kUtilityCmdPrefix|. See ChildProcessHost::GetChildPath() for
    // a similar case with Valgrind.
    if (has_cmd_prefix)
      child_flags = ChildProcessHost::CHILD_NORMAL;

    base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
    if (exe_path.empty()) {
      NOTREACHED() << "Unable to get utility process binary name.";
      return false;
    }

    base::CommandLine* cmd_line = new base::CommandLine(exe_path);
    cmd_line->AppendSwitchASCII(switches::kProcessType,
                                switches::kUtilityProcess);
    cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
    std::string locale = GetContentClient()->browser()->GetApplicationLocale();
    cmd_line->AppendSwitchASCII(switches::kLang, locale);

    if (no_sandbox_)
      cmd_line->AppendSwitch(switches::kNoSandbox);

    // Browser command-line switches to propagate to the utility process.
    static const char* const kSwitchNames[] = {
      switches::kDebugPluginLoading,
      switches::kNoSandbox,
      switches::kProfilerTiming,
#if defined(OS_MACOSX)
      switches::kEnableSandboxLogging,
#endif
    };
    cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                               arraysize(kSwitchNames));

    if (has_cmd_prefix) {
      // Launch the utility child process with some prefix
      // (usually "xterm -e gdb --args").
      cmd_line->PrependWrapper(browser_command_line.GetSwitchValueNative(
          switches::kUtilityCmdPrefix));
    }

    if (!exposed_dir_.empty()) {
      cmd_line->AppendSwitchPath(switches::kUtilityProcessAllowedDir,
                                 exposed_dir_);
    }

    if (is_mdns_enabled_)
      cmd_line->AppendSwitch(switches::kUtilityProcessEnableMDns);

#if defined(OS_WIN)
    // Let the utility process know if it is intended to be elevated.
    if (run_elevated_)
      cmd_line->AppendSwitch(switches::kUtilityProcessRunningElevated);
#endif

    process_->Launch(
        new UtilitySandboxedProcessLauncherDelegate(exposed_dir_,
                                                    run_elevated_,
                                                    no_sandbox_, env_,
                                                    process_->GetHost()),
        cmd_line,
        true);
  }

  return true;
}

bool UtilityProcessHostImpl::OnMessageReceived(const IPC::Message& message) {
  if (!client_.get())
    return true;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&UtilityProcessHostClient::OnMessageReceived),
          client_.get(),
          message));

  return true;
}

void UtilityProcessHostImpl::OnProcessLaunchFailed() {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UtilityProcessHostClient::OnProcessLaunchFailed,
                 client_.get()));
}

void UtilityProcessHostImpl::OnProcessCrashed(int exit_code) {
  if (!client_.get())
    return;

  client_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UtilityProcessHostClient::OnProcessCrashed, client_.get(),
            exit_code));
}

void UtilityProcessHostImpl::OnProcessLaunched() {
  if (mojo_application_host_) {
    base::ProcessHandle handle;
    if (RenderProcessHost::run_renderer_in_process())
      handle = base::GetCurrentProcessHandle();
    else
      handle = process_->GetData().handle;

    mojo_application_host_->Activate(this, handle);
  }
}

}  // namespace content
