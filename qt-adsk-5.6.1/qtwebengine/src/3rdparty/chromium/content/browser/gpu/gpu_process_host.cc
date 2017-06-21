// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host.h"

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/gpu/shader_disk_cache.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_resize_helper.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "ipc/message_filter.h"
#include "media/base/media_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/latency_info.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "content/common/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/gfx/switches.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if defined(USE_X11) && !defined(OS_CHROMEOS)
#include "ui/gfx/x/x11_switches.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "content/browser/browser_io_surface_manager_mac.h"
#include "content/common/child_process_messages.h"
#endif

namespace content {

bool GpuProcessHost::gpu_enabled_ = true;
bool GpuProcessHost::hardware_gpu_enabled_ = true;
int GpuProcessHost::gpu_crash_count_ = 0;
int GpuProcessHost::gpu_recent_crash_count_ = 0;
bool GpuProcessHost::crashed_before_ = false;
int GpuProcessHost::swiftshader_crash_count_ = 0;

namespace {

// Command-line switches to propagate to the GPU process.
static const char* const kSwitchNames[] = {
  switches::kDisableAcceleratedVideoDecode,
  switches::kDisableBreakpad,
  switches::kDisableGpuSandbox,
  switches::kDisableGpuWatchdog,
  switches::kDisableGLExtensions,
  switches::kDisableLogging,
  switches::kDisableSeccompFilterSandbox,
#if defined(ENABLE_WEBRTC)
  switches::kDisableWebRtcHWEncoding,
#endif
#if defined(OS_WIN)
  switches::kEnableAcceleratedVpxDecode,
#endif
  switches::kEnableLogging,
  switches::kEnableShareGroupAsyncTextureUpload,
#if defined(OS_CHROMEOS)
  switches::kDisableVaapiAcceleratedVideoEncode,
#endif
  switches::kGpuStartupDialog,
  switches::kGpuSandboxAllowSysVShm,
  switches::kGpuSandboxFailuresFatal,
  switches::kGpuSandboxStartEarly,
  switches::kLoggingLevel,
  switches::kEnableLowEndDeviceMode,
  switches::kDisableLowEndDeviceMode,
  switches::kEnableMemoryBenchmarking,
  switches::kNoSandbox,
  switches::kProfilerTiming,
  switches::kTestGLLib,
  switches::kTraceStartup,
  switches::kTraceToConsole,
  switches::kV,
  switches::kVModule,
#if defined(OS_MACOSX)
  switches::kDisableRemoteCoreAnimation,
  switches::kDisableNSCGLSurfaceApi,
  switches::kForceNSCGLSurfaceApi,
  switches::kEnableSandboxLogging,
#endif
#if defined(USE_AURA)
  switches::kUIPrioritizeInGpuProcess,
#endif
#if defined(USE_OZONE)
  switches::kOzonePlatform,
  switches::kOzoneUseSurfaceless,
#endif
#if defined(USE_X11) && !defined(OS_CHROMEOS)
  switches::kX11Display,
#endif
};

enum GPUProcessLifetimeEvent {
  LAUNCHED,
  DIED_FIRST_TIME,
  DIED_SECOND_TIME,
  DIED_THIRD_TIME,
  DIED_FOURTH_TIME,
  GPU_PROCESS_LIFETIME_EVENT_MAX = 100
};

// Indexed by GpuProcessKind. There is one of each kind maximum. This array may
// only be accessed from the IO thread.
GpuProcessHost* g_gpu_process_hosts[GpuProcessHost::GPU_PROCESS_KIND_COUNT];


void SendGpuProcessMessage(GpuProcessHost::GpuProcessKind kind,
                           CauseForGpuLaunch cause,
                           IPC::Message* message) {
  GpuProcessHost* host = GpuProcessHost::Get(kind, cause);
  if (host) {
    host->Send(message);
  } else {
    delete message;
  }
}

// NOTE: changes to this class need to be reviewed by the security team.
class GpuSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  GpuSandboxedProcessLauncherDelegate(base::CommandLine* cmd_line,
                                      ChildProcessHost* host)
#if defined(OS_WIN)
      : cmd_line_(cmd_line) {}
#elif defined(OS_POSIX)
      : ipc_fd_(host->TakeClientFileDescriptor()) {}
#endif

  ~GpuSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool ShouldSandbox() override {
    bool sandbox = !cmd_line_->HasSwitch(switches::kDisableGpuSandbox);
    if(! sandbox) {
      DVLOG(1) << "GPU sandbox is disabled";
    }
    return sandbox;
  }

  void PreSandbox(bool* disable_default_policy,
                  base::FilePath* exposed_dir) override {
    *disable_default_policy = true;
  }

  // For the GPU process we gotten as far as USER_LIMITED. The next level
  // which is USER_RESTRICTED breaks both the DirectX backend and the OpenGL
  // backend. Note that the GPU process is connected to the interactive
  // desktop.
  void PreSpawnTarget(sandbox::TargetPolicy* policy, bool* success) override {
    if (base::win::GetVersion() > base::win::VERSION_XP) {
      if (cmd_line_->GetSwitchValueASCII(switches::kUseGL) ==
          gfx::kGLImplementationDesktopName) {
        // Open GL path.
        policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                              sandbox::USER_LIMITED);
        SetJobLevel(*cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
        policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
      } else {
        policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                              sandbox::USER_LIMITED);

        // UI restrictions break when we access Windows from outside our job.
        // However, we don't want a proxy window in this process because it can
        // introduce deadlocks where the renderer blocks on the gpu, which in
        // turn blocks on the browser UI thread. So, instead we forgo a window
        // message pump entirely and just add job restrictions to prevent child
        // processes.
        SetJobLevel(*cmd_line_,
                    sandbox::JOB_LIMITED_USER,
                    JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                    JOB_OBJECT_UILIMIT_DESKTOP |
                    JOB_OBJECT_UILIMIT_EXITWINDOWS |
                    JOB_OBJECT_UILIMIT_DISPLAYSETTINGS,
                    policy);

        policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
      }
    } else {
      SetJobLevel(*cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
      policy->SetTokenLevel(sandbox::USER_UNPROTECTED,
                            sandbox::USER_LIMITED);
    }

    // Allow the server side of GPU sockets, which are pipes that have
    // the "chrome.gpu" namespace and an arbitrary suffix.
    sandbox::ResultCode result = policy->AddRule(
        sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
        sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
        L"\\\\.\\pipe\\chrome.gpu.*");
    if (result != sandbox::SBOX_ALL_OK) {
      *success = false;
      return;
    }

    // Block this DLL even if it is not loaded by the browser process.
    policy->AddDllToUnload(L"cmsetac.dll");

#ifdef USE_AURA
    // GPU also needs to add sections to the browser for aura
    // TODO(jschuh): refactor the GPU channel to remove this. crbug.com/128786
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_HANDLES,
                             sandbox::TargetPolicy::HANDLES_DUP_BROKER,
                             L"Section");
    if (result != sandbox::SBOX_ALL_OK) {
      *success = false;
      return;
    }
#endif

    if (cmd_line_->HasSwitch(switches::kEnableLogging)) {
      base::string16 log_file_path = logging::GetLogFileFullPath();
      if (!log_file_path.empty()) {
        result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                                 sandbox::TargetPolicy::FILES_ALLOW_ANY,
                                 log_file_path.c_str());
        if (result != sandbox::SBOX_ALL_OK) {
          *success = false;
          return;
        }
      }
    }
  }
#elif defined(OS_POSIX)

  base::ScopedFD TakeIpcFd() override { return ipc_fd_.Pass(); }
#endif  // OS_WIN

  SandboxType GetSandboxType() override {
    return SANDBOX_TYPE_GPU;
  }

 private:
#if defined(OS_WIN)
  base::CommandLine* cmd_line_;
#elif defined(OS_POSIX)
  base::ScopedFD ipc_fd_;
#endif  // OS_WIN
};

}  // anonymous namespace

// static
bool GpuProcessHost::ValidateHost(GpuProcessHost* host) {
  // The Gpu process is invalid if it's not using SwiftShader, the card is
  // blacklisted, and we can kill it and start over.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU) ||
      (host->valid_ &&
       (host->swiftshader_rendering_ ||
        !GpuDataManagerImpl::GetInstance()->ShouldUseSwiftShader()))) {
    return true;
  }

  host->ForceShutdown();
  return false;
}

// static
GpuProcessHost* GpuProcessHost::Get(GpuProcessKind kind,
                                    CauseForGpuLaunch cause) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't grant further access to GPU if it is not allowed.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  if (!gpu_data_manager->GpuAccessAllowed(NULL))
    return NULL;

  if (g_gpu_process_hosts[kind] && ValidateHost(g_gpu_process_hosts[kind]))
    return g_gpu_process_hosts[kind];

  if (cause == CAUSE_FOR_GPU_LAUNCH_NO_LAUNCH)
    return NULL;

  static int last_host_id = 0;
  int host_id;
  host_id = ++last_host_id;

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLaunchCause",
                            cause,
                            CAUSE_FOR_GPU_LAUNCH_MAX_ENUM);

  GpuProcessHost* host = new GpuProcessHost(host_id, kind);
  if (host->Init())
    return host;

  delete host;
  return NULL;
}

// static
void GpuProcessHost::GetProcessHandles(
    const GpuDataManager::GetGpuProcessHandlesCallback& callback)  {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&GpuProcessHost::GetProcessHandles, callback));
    return;
  }
  std::list<base::ProcessHandle> handles;
  for (size_t i = 0; i < arraysize(g_gpu_process_hosts); ++i) {
    // TODO(rvargas) crbug/417532: don't store ProcessHandles!.
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && ValidateHost(host))
      handles.push_back(host->process_->GetProcess().Handle());
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, handles));
}

// static
void GpuProcessHost::SendOnIO(GpuProcessKind kind,
                              CauseForGpuLaunch cause,
                              IPC::Message* message) {
  if (!BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &SendGpuProcessMessage, kind, cause, message))) {
    delete message;
  }
}

GpuMainThreadFactoryFunction g_gpu_main_thread_factory = NULL;

void GpuProcessHost::RegisterGpuMainThreadFactory(
    GpuMainThreadFactoryFunction create) {
  g_gpu_main_thread_factory = create;
}

// static
GpuProcessHost* GpuProcessHost::FromID(int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (int i = 0; i < GPU_PROCESS_KIND_COUNT; ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && host->host_id_ == host_id && ValidateHost(host))
      return host;
  }

  return NULL;
}

GpuProcessHost::GpuProcessHost(int host_id, GpuProcessKind kind)
    : host_id_(host_id),
      valid_(true),
      in_process_(false),
      swiftshader_rendering_(false),
      kind_(kind),
      process_launched_(false),
      initialized_(false),
      gpu_crash_recorded_(false),
      uma_memory_stats_received_(false) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    in_process_ = true;
  }

  // If the 'single GPU process' policy ever changes, we still want to maintain
  // it for 'gpu thread' mode and only create one instance of host and thread.
  DCHECK(!in_process_ || g_gpu_process_hosts[kind] == NULL);

  g_gpu_process_hosts[kind] = this;

  // Post a task to create the corresponding GpuProcessHostUIShim.  The
  // GpuProcessHostUIShim will be destroyed if either the browser exits,
  // in which case it calls GpuProcessHostUIShim::DestroyAll, or the
  // GpuProcessHost is destroyed, which happens when the corresponding GPU
  // process terminates or fails to launch.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&GpuProcessHostUIShim::Create), host_id));

  process_.reset(new BrowserChildProcessHostImpl(PROCESS_TYPE_GPU, this));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK(CalledOnValidThread());

  SendOutstandingReplies();

  RecordProcessCrash();

  // In case we never started, clean up.
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (!io_surface_manager_token_.IsZero()) {
    BrowserIOSurfaceManager::GetInstance()->InvalidateGpuProcessToken();
    io_surface_manager_token_.SetZero();
  }
#endif

  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = NULL;

  // If there are any remaining offscreen contexts at the point the
  // GPU process exits, assume something went wrong, and block their
  // URLs from accessing client 3D APIs without prompting.
  BlockLiveOffscreenContexts();

  UMA_HISTOGRAM_COUNTS_100("GPU.AtExitSurfaceCount",
                           GpuSurfaceTracker::Get()->GetSurfaceCount());
  UMA_HISTOGRAM_BOOLEAN("GPU.AtExitReceivedMemoryStats",
                        uma_memory_stats_received_);

  if (uma_memory_stats_received_) {
    UMA_HISTOGRAM_COUNTS_100("GPU.AtExitManagedMemoryClientCount",
                             uma_memory_stats_.client_count);
    UMA_HISTOGRAM_COUNTS_100("GPU.AtExitContextGroupCount",
                             uma_memory_stats_.context_group_count);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "GPU.AtExitMBytesAllocated",
        uma_memory_stats_.bytes_allocated_current / 1024 / 1024, 1, 2000, 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "GPU.AtExitMBytesAllocatedMax",
        uma_memory_stats_.bytes_allocated_max / 1024 / 1024, 1, 2000, 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "GPU.AtExitMBytesLimit",
        uma_memory_stats_.bytes_limit / 1024 / 1024, 1, 2000, 50);
  }

  std::string message;
  if (!in_process_) {
    int exit_code;
    base::TerminationStatus status = process_->GetTerminationStatus(
        false /* known_dead */, &exit_code);
    UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationStatus",
                              status,
                              base::TERMINATION_STATUS_MAX_ENUM);

    if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
        status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
      UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessExitCode",
                                exit_code,
                                RESULT_CODE_LAST_CODE);
    }

    switch (status) {
      case base::TERMINATION_STATUS_NORMAL_TERMINATION:
        message = "The GPU process exited normally. Everything is okay.";
        break;
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
        message = base::StringPrintf(
            "The GPU process exited with code %d.",
            exit_code);
        break;
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
        message = "You killed the GPU process! Why?";
        break;
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
        message = "The GUP process was killed due to out of memory.";
        break;
#endif
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
        message = "The GPU process crashed!";
        break;
      default:
        break;
    }
  }

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&GpuProcessHostUIShim::Destroy,
                                     host_id_,
                                     message));
}

bool GpuProcessHost::Init() {
  init_start_time_ = base::TimeTicks::Now();

  TRACE_EVENT_INSTANT0("gpu", "LaunchGpuProcess", TRACE_EVENT_SCOPE_THREAD);

  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty())
    return false;

  if (in_process_) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(g_gpu_main_thread_factory);
    in_process_gpu_thread_.reset(
        g_gpu_main_thread_factory(InProcessChildThreadParams(
            channel_id, base::MessageLoop::current()->task_runner())));
    base::Thread::Options options;
#if defined(OS_WIN) && !defined(TOOLKIT_QT)
    // WGL needs to create its own window and pump messages on it.
    options.message_loop_type = base::MessageLoop::TYPE_UI;
#endif
    in_process_gpu_thread_->StartWithOptions(options);

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else if (!LaunchGpuProcess(channel_id)) {
    return false;
  }

  if (!Send(new GpuMsg_Initialize()))
    return false;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  io_surface_manager_token_ =
      BrowserIOSurfaceManager::GetInstance()->GenerateGpuProcessToken();
  // Note: A valid IOSurface manager token needs to be sent to the Gpu process
  // before any GpuMemoryBuffer allocation requests can be sent.
  Send(new ChildProcessMsg_SetIOSurfaceManagerToken(io_surface_manager_token_));
#endif

  return true;
}

void GpuProcessHost::RouteOnUIThread(const IPC::Message& message) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RouteToGpuProcessHostUIShimTask, host_id_, message));
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  if (process_->GetHost()->IsChannelOpening()) {
    queued_messages_.push(msg);
    return true;
  }

  bool result = process_->Send(msg);
  if (!result) {
    // Channel is hosed, but we may not get destroyed for a while. Send
    // outstanding channel creation failures now so that the caller can restart
    // with a new process/channel without waiting.
    SendOutstandingReplies();
  }
  return result;
}

void GpuProcessHost::AddFilter(IPC::MessageFilter* filter) {
  DCHECK(CalledOnValidThread());
  process_->GetHost()->AddFilter(filter);
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());
  IPC_BEGIN_MESSAGE_MAP(GpuProcessHost, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_Initialized, OnInitialized)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ChannelEstablished, OnChannelEstablished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_CommandBufferCreated, OnCommandBufferCreated)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DestroyCommandBuffer, OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GpuMemoryBufferCreated,
                        OnGpuMemoryBufferCreated)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DidCreateOffscreenContext,
                        OnDidCreateOffscreenContext)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DidLoseContext, OnDidLoseContext)
    IPC_MESSAGE_HANDLER(GpuHostMsg_DidDestroyOffscreenContext,
                        OnDidDestroyOffscreenContext)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GpuMemoryUmaStats,
                        OnGpuMemoryUmaStatsReceived)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_GENERIC(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                                OnAcceleratedSurfaceBuffersSwapped(message))
#endif
    IPC_MESSAGE_HANDLER(GpuHostMsg_DestroyChannel,
                        OnDestroyChannel)
    IPC_MESSAGE_HANDLER(GpuHostMsg_CacheShader,
                        OnCacheShader)

    IPC_MESSAGE_UNHANDLED(RouteOnUIThread(message))
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHost::OnChannelConnected(int32 peer_pid) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnChannelConnected");

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void GpuProcessHost::EstablishGpuChannel(
    int client_id,
    bool share_context,
    bool allow_future_sync_points,
    const EstablishChannelCallback& callback) {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("gpu", "GpuProcessHost::EstablishGpuChannel");

  // If GPU features are already blacklisted, no need to establish the channel.
  if (!GpuDataManagerImpl::GetInstance()->GpuAccessAllowed(NULL)) {
    DVLOG(1) << "GPU blacklisted, refusing to open a GPU channel.";
    callback.Run(IPC::ChannelHandle(), gpu::GPUInfo());
    return;
  }

  if (Send(new GpuMsg_EstablishChannel(
          client_id, share_context, allow_future_sync_points))) {
    channel_requests_.push(callback);
  } else {
    DVLOG(1) << "Failed to send GpuMsg_EstablishChannel.";
    callback.Run(IPC::ChannelHandle(), gpu::GPUInfo());
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuShaderDiskCache)) {
    CreateChannelCache(client_id);
  }
}

void GpuProcessHost::CreateViewCommandBuffer(
    const gfx::GLSurfaceHandle& compositing_surface,
    int surface_id,
    int client_id,
    const GPUCreateCommandBufferConfig& init_params,
    int route_id,
    const CreateCommandBufferCallback& callback) {
  TRACE_EVENT0("gpu", "GpuProcessHost::CreateViewCommandBuffer");

  DCHECK(CalledOnValidThread());

  if (!compositing_surface.is_null() &&
      Send(new GpuMsg_CreateViewCommandBuffer(
          compositing_surface, surface_id, client_id, init_params, route_id))) {
    create_command_buffer_requests_.push(callback);
    surface_refs_.insert(std::make_pair(surface_id,
        GpuSurfaceTracker::GetInstance()->GetSurfaceRefForSurface(surface_id)));
  } else {
    // Could distinguish here between compositing_surface being NULL
    // and Send failing, if desired.
    callback.Run(CREATE_COMMAND_BUFFER_FAILED_AND_CHANNEL_LOST);
  }
}

void GpuProcessHost::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id,
    int32 surface_id,
    const CreateGpuMemoryBufferCallback& callback) {
  TRACE_EVENT0("gpu", "GpuProcessHost::CreateGpuMemoryBuffer");

  DCHECK(CalledOnValidThread());

  GpuMsg_CreateGpuMemoryBuffer_Params params;
  params.id = id;
  params.size = size;
  params.format = format;
  params.usage = usage;
  params.client_id = client_id;
  params.surface_handle =
      GpuSurfaceTracker::GetInstance()->GetSurfaceHandle(surface_id).handle;
  if (Send(new GpuMsg_CreateGpuMemoryBuffer(params))) {
    create_gpu_memory_buffer_requests_.push(callback);
    create_gpu_memory_buffer_surface_refs_.push(surface_id);
    if (surface_id) {
      surface_refs_.insert(std::make_pair(
          surface_id, GpuSurfaceTracker::GetInstance()->GetSurfaceRefForSurface(
                          surface_id)));
    }
  } else {
    callback.Run(gfx::GpuMemoryBufferHandle());
  }
}

void GpuProcessHost::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                            int client_id,
                                            int sync_point) {
  TRACE_EVENT0("gpu", "GpuProcessHost::DestroyGpuMemoryBuffer");

  DCHECK(CalledOnValidThread());

  Send(new GpuMsg_DestroyGpuMemoryBuffer(id, client_id, sync_point));
}

void GpuProcessHost::OnInitialized(bool result, const gpu::GPUInfo& gpu_info) {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", result);
  initialized_ = result;

  if (!initialized_)
    GpuDataManagerImpl::GetInstance()->OnGpuProcessInitFailure();
  else if (!in_process_)
    GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info);
}

void GpuProcessHost::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnChannelEstablished");

  if (channel_requests_.empty()) {
    // This happens when GPU process is compromised.
    RouteOnUIThread(GpuHostMsg_OnLogMessage(
        logging::LOG_WARNING,
        "WARNING",
        "Received a ChannelEstablished message but no requests in queue."));
    return;
  }
  EstablishChannelCallback callback = channel_requests_.front();
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (!channel_handle.name.empty() &&
      !GpuDataManagerImpl::GetInstance()->GpuAccessAllowed(NULL)) {
    Send(new GpuMsg_CloseChannel(channel_handle));
    callback.Run(IPC::ChannelHandle(), gpu::GPUInfo());
    RouteOnUIThread(GpuHostMsg_OnLogMessage(
        logging::LOG_WARNING,
        "WARNING",
        "Hardware acceleration is unavailable."));
    return;
  }

  callback.Run(channel_handle,
               GpuDataManagerImpl::GetInstance()->GetGPUInfo());
}

void GpuProcessHost::OnCommandBufferCreated(CreateCommandBufferResult result) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnCommandBufferCreated");

  if (create_command_buffer_requests_.empty())
    return;

  CreateCommandBufferCallback callback =
      create_command_buffer_requests_.front();
  create_command_buffer_requests_.pop();
  callback.Run(result);
}

void GpuProcessHost::OnDestroyCommandBuffer(int32 surface_id) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnDestroyCommandBuffer");
  SurfaceRefMap::iterator it = surface_refs_.find(surface_id);
  if (it != surface_refs_.end()) {
    surface_refs_.erase(it);
  }
}

void GpuProcessHost::OnGpuMemoryBufferCreated(
    const gfx::GpuMemoryBufferHandle& handle) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnGpuMemoryBufferCreated");

  if (create_gpu_memory_buffer_requests_.empty())
    return;

  CreateGpuMemoryBufferCallback callback =
      create_gpu_memory_buffer_requests_.front();
  create_gpu_memory_buffer_requests_.pop();
  callback.Run(handle);

  int32 surface_id = create_gpu_memory_buffer_surface_refs_.front();
  create_gpu_memory_buffer_surface_refs_.pop();
  SurfaceRefMap::iterator it = surface_refs_.find(surface_id);
  if (it != surface_refs_.end()) {
    surface_refs_.erase(it);
  }
}

void GpuProcessHost::OnDidCreateOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.insert(url);
}

void GpuProcessHost::OnDidLoseContext(bool offscreen,
                                      gpu::error::ContextLostReason reason,
                                      const GURL& url) {
  // TODO(kbr): would be nice to see the "offscreen" flag too.
  TRACE_EVENT2("gpu", "GpuProcessHost::OnDidLoseContext",
               "reason", reason,
               "url",
               url.possibly_invalid_spec());

  if (!offscreen || url.is_empty()) {
    // Assume that the loss of the compositor's or accelerated canvas'
    // context is a serious event and blame the loss on all live
    // offscreen contexts. This more robustly handles situations where
    // the GPU process may not actually detect the context loss in the
    // offscreen context.
    BlockLiveOffscreenContexts();
    return;
  }

  GpuDataManagerImpl::DomainGuilt guilt;
  switch (reason) {
    case gpu::error::kGuilty:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_KNOWN;
      break;
    case gpu::error::kUnknown:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN;
      break;
    case gpu::error::kInnocent:
      return;
    default:
      NOTREACHED();
      return;
  }

  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(url, guilt);
}

void GpuProcessHost::OnDidDestroyOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.erase(url);
}

void GpuProcessHost::OnGpuMemoryUmaStatsReceived(
    const GPUMemoryUmaStats& stats) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnGpuMemoryUmaStatsReceived");
  uma_memory_stats_received_ = true;
  uma_memory_stats_ = stats;
}

#if defined(OS_MACOSX)
void GpuProcessHost::OnAcceleratedSurfaceBuffersSwapped(
    const IPC::Message& message) {
  RenderWidgetResizeHelper::Get()->PostGpuProcessMsg(host_id_, message);
}
#endif

void GpuProcessHost::OnProcessLaunched() {
  UMA_HISTOGRAM_TIMES("GPU.GPUProcessLaunchTime",
                      base::TimeTicks::Now() - init_start_time_);
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  SendOutstandingReplies();
  RecordProcessCrash();
  GpuDataManagerImpl::GetInstance()->ProcessCrashed(
      process_->GetTerminationStatus(true /* known_dead */, NULL));
}

GpuProcessHost::GpuProcessKind GpuProcessHost::kind() {
  return kind_;
}

void GpuProcessHost::ForceShutdown() {
  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = NULL;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (!io_surface_manager_token_.IsZero()) {
    BrowserIOSurfaceManager::GetInstance()->InvalidateGpuProcessToken();
    io_surface_manager_token_.SetZero();
  }
#endif

  process_->ForceShutdown();
}

void GpuProcessHost::BeginFrameSubscription(
    int surface_id,
    base::WeakPtr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  frame_subscribers_[surface_id] = subscriber;
}

void GpuProcessHost::EndFrameSubscription(int surface_id) {
  frame_subscribers_.erase(surface_id);
}

bool GpuProcessHost::LaunchGpuProcess(const std::string& channel_id) {
  if (!(gpu_enabled_ &&
      GpuDataManagerImpl::GetInstance()->ShouldUseSwiftShader()) &&
      !hardware_gpu_enabled_) {
    SendOutstandingReplies();
    return false;
  }

  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

#if defined(OS_ANDROID)
  // crbug.com/447735. readlink("self/proc/exe") sometimes fails on Android
  // at startup with EACCES. As a workaround ignore this here, since the
  // executable name is actually not used or useful anyways.
  base::CommandLine* cmd_line =
      new base::CommandLine(base::CommandLine::NO_PROGRAM);
#else
#if defined(OS_LINUX)
  int child_flags = gpu_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                           ChildProcessHost::CHILD_NORMAL;
#else
  int child_flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
  if (exe_path.empty())
    return false;

  base::CommandLine* cmd_line = new base::CommandLine(exe_path);
#endif
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);

  if (kind_ == GPU_PROCESS_KIND_UNSANDBOXED)
    cmd_line->AppendSwitch(switches::kDisableGpuSandbox);

  // If you want a browser command-line switch passed to the GPU process
  // you need to add it to |kSwitchNames| at the beginning of this file.
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));
  cmd_line->CopySwitchesFrom(
      browser_command_line, switches::kGpuSwitches, switches::kNumGpuSwitches);
  cmd_line->CopySwitchesFrom(
      browser_command_line, switches::kGLSwitchesCopiedFromGpuProcessHost,
      switches::kGLSwitchesCopiedFromGpuProcessHostNumSwitches);

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line, process_->GetData().id);

  GpuDataManagerImpl::GetInstance()->AppendGpuCommandLine(cmd_line);

  if (cmd_line->HasSwitch(switches::kUseGL)) {
    swiftshader_rendering_ =
        (cmd_line->GetSwitchValueASCII(switches::kUseGL) == "swiftshader");
  }

  UMA_HISTOGRAM_BOOLEAN("GPU.GPU.GPUProcessSoftwareRendering",
                        swiftshader_rendering_);

  // If specified, prepend a launcher program to the command line.
  if (!gpu_launcher.empty())
    cmd_line->PrependWrapper(gpu_launcher);

  process_->Launch(
      new GpuSandboxedProcessLauncherDelegate(cmd_line,
                                              process_->GetHost()),
      cmd_line,
      true);
  process_launched_ = true;

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            LAUNCHED, GPU_PROCESS_LIFETIME_EVENT_MAX);
  return true;
}

void GpuProcessHost::SendOutstandingReplies() {
  valid_ = false;
  // First send empty channel handles for all EstablishChannel requests.
  while (!channel_requests_.empty()) {
    EstablishChannelCallback callback = channel_requests_.front();
    channel_requests_.pop();
    callback.Run(IPC::ChannelHandle(), gpu::GPUInfo());
  }

  while (!create_command_buffer_requests_.empty()) {
    CreateCommandBufferCallback callback =
        create_command_buffer_requests_.front();
    create_command_buffer_requests_.pop();
    callback.Run(CREATE_COMMAND_BUFFER_FAILED_AND_CHANNEL_LOST);
  }

  while (!create_gpu_memory_buffer_requests_.empty()) {
    CreateGpuMemoryBufferCallback callback =
        create_gpu_memory_buffer_requests_.front();
    create_gpu_memory_buffer_requests_.pop();
    callback.Run(gfx::GpuMemoryBufferHandle());
  }
}

void GpuProcessHost::BlockLiveOffscreenContexts() {
  for (std::multiset<GURL>::iterator iter =
           urls_with_live_offscreen_contexts_.begin();
       iter != urls_with_live_offscreen_contexts_.end(); ++iter) {
    GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(
        *iter, GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN);
  }
}

void GpuProcessHost::RecordProcessCrash() {
  // Skip if a GPU process crash was already counted.
  if (gpu_crash_recorded_)
    return;

  // Maximum number of times the GPU process is allowed to crash in a session.
  // Once this limit is reached, any request to launch the GPU process will
  // fail.
  const int kGpuMaxCrashCount = 3;

  // Last time the GPU process crashed.
  static base::Time last_gpu_crash_time;

  bool disable_crash_limit = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuProcessCrashLimit);

  // Ending only acts as a failure if the GPU process was actually started and
  // was intended for actual rendering (and not just checking caps or other
  // options).
  if (process_launched_ && kind_ == GPU_PROCESS_KIND_SANDBOXED) {
    gpu_crash_recorded_ = true;
    if (swiftshader_rendering_) {
      UMA_HISTOGRAM_ENUMERATION("GPU.SwiftShaderLifetimeEvents",
                                DIED_FIRST_TIME + swiftshader_crash_count_,
                                GPU_PROCESS_LIFETIME_EVENT_MAX);

      if (++swiftshader_crash_count_ >= kGpuMaxCrashCount &&
          !disable_crash_limit) {
        // SwiftShader is too unstable to use. Disable it for current session.
        gpu_enabled_ = false;
      }
    } else {
      ++gpu_crash_count_;
      UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                                std::min(DIED_FIRST_TIME + gpu_crash_count_,
                                         GPU_PROCESS_LIFETIME_EVENT_MAX - 1),
                                GPU_PROCESS_LIFETIME_EVENT_MAX);

      // Allow about 1 GPU crash per hour to be removed from the crash count,
      // so very occasional crashes won't eventually add up and prevent the
      // GPU process from launching.
      ++gpu_recent_crash_count_;
      base::Time current_time = base::Time::Now();
      if (crashed_before_) {
        int hours_different = (current_time - last_gpu_crash_time).InHours();
        gpu_recent_crash_count_ =
            std::max(0, gpu_recent_crash_count_ - hours_different);
      }

      crashed_before_ = true;
      last_gpu_crash_time = current_time;

      if ((gpu_recent_crash_count_ >= kGpuMaxCrashCount &&
           !disable_crash_limit) ||
          !initialized_) {
#if !defined(OS_CHROMEOS)
        // The GPU process is too unstable to use. Disable it for current
        // session.
        hardware_gpu_enabled_ = false;
        GpuDataManagerImpl::GetInstance()->DisableHardwareAcceleration();
#endif
      }
    }
  }
}

std::string GpuProcessHost::GetShaderPrefixKey() {
  if (shader_prefix_key_.empty()) {
    gpu::GPUInfo info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();

    std::string in_str = GetContentClient()->GetProduct() + "-" +
#if defined(OS_ANDROID)
        base::android::BuildInfo::GetInstance()->android_build_fp() + "-" +
#endif
        info.gl_vendor + "-" + info.gl_renderer + "-" +
        info.driver_version + "-" + info.driver_vendor;

    base::Base64Encode(base::SHA1HashString(in_str), &shader_prefix_key_);
  }

  return shader_prefix_key_;
}

void GpuProcessHost::LoadedShader(const std::string& key,
                                  const std::string& data) {
  std::string prefix = GetShaderPrefixKey();
  if (!key.compare(0, prefix.length(), prefix))
    Send(new GpuMsg_LoadedShader(data));
}

void GpuProcessHost::CreateChannelCache(int32 client_id) {
  TRACE_EVENT0("gpu", "GpuProcessHost::CreateChannelCache");

  scoped_refptr<ShaderDiskCache> cache =
      ShaderCacheFactory::GetInstance()->Get(client_id);
  if (!cache.get())
    return;

  cache->set_host_id(host_id_);

  client_id_to_shader_cache_[client_id] = cache;
}

void GpuProcessHost::OnDestroyChannel(int32 client_id) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnDestroyChannel");
  client_id_to_shader_cache_.erase(client_id);
}

void GpuProcessHost::OnCacheShader(int32 client_id,
                                   const std::string& key,
                                   const std::string& shader) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnCacheShader");
  ClientIdToShaderCacheMap::iterator iter =
      client_id_to_shader_cache_.find(client_id);
  // If the cache doesn't exist then this is an off the record profile.
  if (iter == client_id_to_shader_cache_.end())
    return;
  iter->second->Cache(GetShaderPrefixKey() + ":" + key, shader);
}

}  // namespace content
