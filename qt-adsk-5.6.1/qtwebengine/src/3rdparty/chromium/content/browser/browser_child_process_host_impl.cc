// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/histogram_message_filter.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/profiler_message_filter.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"

#if defined(OS_MACOSX)
#include "content/browser/mach_broker_mac.h"
#endif

namespace content {
namespace {

static base::LazyInstance<BrowserChildProcessHostImpl::BrowserChildProcessList>
    g_child_process_list = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<BrowserChildProcessObserver>>
    g_observers = LAZY_INSTANCE_INITIALIZER;

void NotifyProcessHostConnected(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessHostConnected(data));
}

void NotifyProcessHostDisconnected(const ChildProcessData& data) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessHostDisconnected(data));
}

void NotifyProcessCrashed(const ChildProcessData& data, int exit_code) {
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessCrashed(data, exit_code));
}

}  // namespace

BrowserChildProcessHost* BrowserChildProcessHost::Create(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate) {
  return new BrowserChildProcessHostImpl(process_type, delegate);
}

BrowserChildProcessHost* BrowserChildProcessHost::FromID(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserChildProcessHostImpl::BrowserChildProcessList* process_list =
      g_child_process_list.Pointer();
  for (BrowserChildProcessHostImpl* host : *process_list) {
    if (host->GetData().id == child_process_id)
      return host;
  }
  return nullptr;
}

#if defined(OS_MACOSX)
base::ProcessMetrics::PortProvider* BrowserChildProcessHost::GetPortProvider() {
  return MachBroker::GetInstance();
}
#endif

// static
BrowserChildProcessHostImpl::BrowserChildProcessList*
    BrowserChildProcessHostImpl::GetIterator() {
  return g_child_process_list.Pointer();
}

// static
void BrowserChildProcessHostImpl::AddObserver(
    BrowserChildProcessObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_observers.Get().AddObserver(observer);
}

// static
void BrowserChildProcessHostImpl::RemoveObserver(
    BrowserChildProcessObserver* observer) {
  // TODO(phajdan.jr): Check thread after fixing http://crbug.com/167126.
  g_observers.Get().RemoveObserver(observer);
}

BrowserChildProcessHostImpl::BrowserChildProcessHostImpl(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate)
    : data_(process_type),
      delegate_(delegate),
      power_monitor_message_broadcaster_(this) {
  data_.id = ChildProcessHostImpl::GenerateChildProcessUniqueId();

  child_process_host_.reset(ChildProcessHost::Create(this));
  AddFilter(new TraceMessageFilter(data_.id));
  AddFilter(new ProfilerMessageFilter(process_type));
  AddFilter(new HistogramMessageFilter);

  g_child_process_list.Get().push_back(this);
  GetContentClient()->browser()->BrowserChildProcessHostCreated(this);

  power_monitor_message_broadcaster_.Init();
}

BrowserChildProcessHostImpl::~BrowserChildProcessHostImpl() {
  g_child_process_list.Get().remove(this);
}

// static
void BrowserChildProcessHostImpl::TerminateAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Make a copy since the BrowserChildProcessHost dtor mutates the original
  // list.
  BrowserChildProcessList copy = g_child_process_list.Get();
  for (BrowserChildProcessList::iterator it = copy.begin();
       it != copy.end(); ++it) {
    delete (*it)->delegate();  // ~*HostDelegate deletes *HostImpl.
  }
}

void BrowserChildProcessHostImpl::Launch(
    SandboxedProcessLauncherDelegate* delegate,
    base::CommandLine* cmd_line,
    bool terminate_on_shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line, data_.id);

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char* kForwardSwitches[] = {
    switches::kDisableLogging,
    switches::kEnableLogging,
    switches::kIPCConnectionTimeout,
    switches::kLoggingLevel,
    switches::kTraceToConsole,
    switches::kV,
    switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches,
                             arraysize(kForwardSwitches));

  child_process_.reset(new ChildProcessLauncher(
      delegate,
      cmd_line,
      data_.id,
      this,
      terminate_on_shutdown));
}

const ChildProcessData& BrowserChildProcessHostImpl::GetData() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return data_;
}

ChildProcessHost* BrowserChildProcessHostImpl::GetHost() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return child_process_host_.get();
}

const base::Process& BrowserChildProcessHostImpl::GetProcess() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(child_process_.get())
      << "Requesting a child process handle before launching.";
  DCHECK(child_process_->GetProcess().IsValid())
      << "Requesting a child process handle before launch has completed OK.";
  return child_process_->GetProcess();
}

void BrowserChildProcessHostImpl::SetName(const base::string16& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.name = name;
}

void BrowserChildProcessHostImpl::SetHandle(base::ProcessHandle handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.handle = handle;
}

ServiceRegistry* BrowserChildProcessHostImpl::GetServiceRegistry() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return delegate_->GetServiceRegistry();
}

void BrowserChildProcessHostImpl::ForceShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_child_process_list.Get().remove(this);
  child_process_host_->ForceShutdown();
}

void BrowserChildProcessHostImpl::SetBackgrounded(bool backgrounded) {
  child_process_->SetProcessBackgrounded(backgrounded);
}

void BrowserChildProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  child_process_host_->AddFilter(filter->GetFilter());
}

void BrowserChildProcessHostImpl::NotifyProcessInstanceCreated(
    const ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  FOR_EACH_OBSERVER(BrowserChildProcessObserver, g_observers.Get(),
                    BrowserChildProcessInstanceCreated(data));
}

void BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
    int process_type) {
  UMA_HISTOGRAM_ENUMERATION("ChildProcess.BadMessgeTerminated", process_type,
                            PROCESS_TYPE_MAX);
}

base::TerminationStatus BrowserChildProcessHostImpl::GetTerminationStatus(
    bool known_dead, int* exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!child_process_)  // If the delegate doesn't use Launch() helper.
    return base::GetTerminationStatus(data_.handle, exit_code);
  return child_process_->GetChildTerminationStatus(known_dead,
                                                   exit_code);
}

bool BrowserChildProcessHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  return delegate_->OnMessageReceived(message);
}

void BrowserChildProcessHostImpl::OnChannelConnected(int32 peer_pid) {
#if defined(OS_WIN)
  // From this point onward, the exit of the child process is detected by an
  // error on the IPC channel.
  early_exit_watcher_.StopWatching();
#endif

  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotifyProcessHostConnected, data_));

  delegate_->OnChannelConnected(peer_pid);
}

void BrowserChildProcessHostImpl::OnChannelError() {
  delegate_->OnChannelError();
}

void BrowserChildProcessHostImpl::OnBadMessageReceived(
    const IPC::Message& message) {
  HistogramBadMessageTerminated(data_.process_type);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableKillAfterBadIPC)) {
    return;
  }
  LOG(ERROR) << "Terminating child process for bad IPC message of type "
      << message.type();

  // Create a memory dump. This will contain enough stack frames to work out
  // what the bad message was.
  base::debug::DumpWithoutCrashing();

  child_process_->GetProcess().Terminate(RESULT_CODE_KILLED_BAD_MESSAGE, false);
}

bool BrowserChildProcessHostImpl::CanShutdown() {
  return delegate_->CanShutdown();
}

void BrowserChildProcessHostImpl::OnChildDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_WIN)
  // OnChildDisconnected may be called without OnChannelConnected, so stop the
  // early exit watcher so GetTerminationStatus can close the process handle.
  early_exit_watcher_.StopWatching();
#endif
  if (child_process_.get() || data_.handle) {
    int exit_code;
    base::TerminationStatus status = GetTerminationStatus(
        true /* known_dead */, &exit_code);
    switch (status) {
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
        delegate_->OnProcessCrashed(exit_code);
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&NotifyProcessCrashed, data_, exit_code));
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Crashed2",
                                  data_.process_type,
                                  PROCESS_TYPE_MAX);
        break;
      }
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
        delegate_->OnProcessCrashed(exit_code);
        // Report that this child process was killed.
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2",
                                  data_.process_type,
                                  PROCESS_TYPE_MAX);
        break;
      }
      case base::TERMINATION_STATUS_STILL_RUNNING: {
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.DisconnectedAlive2",
                                  data_.process_type,
                                  PROCESS_TYPE_MAX);
      }
      default:
        break;
    }
    UMA_HISTOGRAM_ENUMERATION("ChildProcess.Disconnected2",
                              data_.process_type,
                              PROCESS_TYPE_MAX);
#if defined(OS_CHROMEOS)
    if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM) {
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2.OOM",
                                data_.process_type,
                                PROCESS_TYPE_MAX);
    }
#endif
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotifyProcessHostDisconnected, data_));
  delete delegate_;  // Will delete us
}

bool BrowserChildProcessHostImpl::Send(IPC::Message* message) {
  return child_process_host_->Send(message);
}

void BrowserChildProcessHostImpl::OnProcessLaunchFailed() {
  delegate_->OnProcessLaunchFailed();
  delete delegate_;  // Will delete us
}

void BrowserChildProcessHostImpl::OnProcessLaunched() {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465841
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465841 BrowserChildProcessHostImpl::OnProcessLaunched"));
  const base::Process& process = child_process_->GetProcess();
  DCHECK(process.IsValid());

#if defined(OS_WIN)
  // Start a WaitableEventWatcher that will invoke OnProcessExitedEarly if the
  // child process exits. This watcher is stopped once the IPC channel is
  // connected and the exit of the child process is detecter by an error on the
  // IPC channel thereafter.
  DCHECK(!early_exit_watcher_.GetWatchedObject());
  early_exit_watcher_.StartWatching(process.Handle(), this);
#endif

  // TODO(rvargas) crbug.com/417532: Don't store a handle.
  data_.handle = process.Handle();
  delegate_->OnProcessLaunched();
}

#if defined(OS_WIN)

void BrowserChildProcessHostImpl::OnObjectSignaled(HANDLE object) {
  OnChildDisconnected();
}

#endif

}  // namespace content
