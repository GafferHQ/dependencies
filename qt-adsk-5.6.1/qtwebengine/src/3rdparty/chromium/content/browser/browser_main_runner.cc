// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_main_runner.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/profiler/scoped_profile.h"
#include "base/profiler/scoped_tracker.h"
#include "base/trace_event/trace_event.h"
#include "base/tracked_objects.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_shutdown_profile_dumper.h"
#include "content/browser/notification_service_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "ui/base/ime/input_method_initializer.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "net/cert/sha256_legacy_support_win.h"
#include "sandbox/win/src/sidestep/preamble_patcher.h"
#include "ui/base/win/scoped_ole_initializer.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/win/direct_write.h"
#endif

bool g_exited_main_message_loop = false;

namespace content {

#if defined(OS_WIN)
namespace {

// Pointer to the original CryptVerifyCertificateSignatureEx function.
net::sha256_interception::CryptVerifyCertificateSignatureExFunc
    g_real_crypt_verify_signature_stub = NULL;

// Stub function that is called whenever the Crypt32 function
// CryptVerifyCertificateSignatureEx is called. It just defers to net to perform
// the actual verification.
BOOL WINAPI CryptVerifyCertificateSignatureExStub(
    HCRYPTPROV_LEGACY provider,
    DWORD encoding_type,
    DWORD subject_type,
    void* subject_data,
    DWORD issuer_type,
    void* issuer_data,
    DWORD flags,
    void* extra) {
  return net::sha256_interception::CryptVerifyCertificateSignatureExHook(
      g_real_crypt_verify_signature_stub, provider, encoding_type, subject_type,
      subject_data, issuer_type, issuer_data, flags, extra);
}

// If necessary, install an interception
void InstallSha256LegacyHooks() {
#if defined(_WIN64)
  // Interception on x64 is not supported.
  return;
#else
  if (base::win::MaybeHasSHA256Support())
    return;

  net::sha256_interception::CryptVerifyCertificateSignatureExFunc
      cert_verify_signature_ptr = reinterpret_cast<
          net::sha256_interception::CryptVerifyCertificateSignatureExFunc>(
              ::GetProcAddress(::GetModuleHandle(L"crypt32.dll"),
                               "CryptVerifyCertificateSignatureEx"));
  CHECK(cert_verify_signature_ptr);

  DWORD old_protect = 0;
  if (!::VirtualProtect(cert_verify_signature_ptr, 5, PAGE_EXECUTE_READWRITE,
                        &old_protect)) {
    return;
  }

  g_real_crypt_verify_signature_stub =
      reinterpret_cast<
          net::sha256_interception::CryptVerifyCertificateSignatureExFunc>(
              VirtualAllocEx(::GetCurrentProcess(), NULL,
                             sidestep::kMaxPreambleStubSize, MEM_COMMIT,
                             PAGE_EXECUTE_READWRITE));
  if (g_real_crypt_verify_signature_stub == NULL) {
    CHECK(::VirtualProtect(cert_verify_signature_ptr, 5, old_protect,
                           &old_protect));
    return;
  }

  sidestep::SideStepError patch_result =
      sidestep::PreamblePatcher::Patch(
          cert_verify_signature_ptr, CryptVerifyCertificateSignatureExStub,
          g_real_crypt_verify_signature_stub, sidestep::kMaxPreambleStubSize);
  if (patch_result != sidestep::SIDESTEP_SUCCESS) {
    CHECK(::VirtualFreeEx(::GetCurrentProcess(),
                          g_real_crypt_verify_signature_stub, 0,
                          MEM_RELEASE));
    CHECK(::VirtualProtect(cert_verify_signature_ptr, 5, old_protect,
                           &old_protect));
    return;
  }

  DWORD dummy = 0;
  CHECK(::VirtualProtect(cert_verify_signature_ptr, 5, old_protect, &dummy));
  CHECK(::VirtualProtect(g_real_crypt_verify_signature_stub,
                         sidestep::kMaxPreambleStubSize, old_protect,
                         &old_protect));
#endif  // _WIN64
}

}  // namespace

#endif  // OS_WIN

class BrowserMainRunnerImpl : public BrowserMainRunner {
 public:
  BrowserMainRunnerImpl()
      : initialization_started_(false), is_shutdown_(false) {}

  ~BrowserMainRunnerImpl() override {
    if (initialization_started_ && !is_shutdown_)
      Shutdown();
  }

  int Initialize(const MainFunctionParams& parameters) override {
    // TODO(vadimt, yiyaoliu): Remove all tracked_objects references below once
    // crbug.com/453640 is fixed.
    tracked_objects::ThreadData::InitializeThreadContext("CrBrowserMain");
    TRACK_SCOPED_REGION(
        "Startup", "BrowserMainRunnerImpl::Initialize");
    TRACE_EVENT0("startup", "BrowserMainRunnerImpl::Initialize");

    // On Android we normally initialize the browser in a series of UI thread
    // tasks. While this is happening a second request can come from the OS or
    // another application to start the browser. If this happens then we must
    // not run these parts of initialization twice.
    if (!initialization_started_) {
      initialization_started_ = true;

#if !defined(OS_IOS)
      if (parameters.command_line.HasSwitch(switches::kWaitForDebugger))
        base::debug::WaitForDebugger(60, true);
#endif

#if defined(OS_WIN)
      if (base::win::GetVersion() < base::win::VERSION_VISTA) {
        // When "Extend support of advanced text services to all programs"
        // (a.k.a. Cicero Unaware Application Support; CUAS) is enabled on
        // Windows XP and handwriting modules shipped with Office 2003 are
        // installed, "penjpn.dll" and "skchui.dll" will be loaded and then
        // crash unless a user installs Office 2003 SP3. To prevent these
        // modules from being loaded, disable TSF entirely. crbug.com/160914.
        // TODO(yukawa): Add a high-level wrapper for this instead of calling
        // Win32 API here directly.
        ImmDisableTextFrameService(static_cast<DWORD>(-1));
      }
      InstallSha256LegacyHooks();
#endif  // OS_WIN

      base::StatisticsRecorder::Initialize();

      notification_service_.reset(new NotificationServiceImpl);

#if defined(OS_WIN)
      // Ole must be initialized before starting message pump, so that TSF
      // (Text Services Framework) module can interact with the message pump
      // on Windows 8 Metro mode.
      ole_initializer_.reset(new ui::ScopedOleInitializer);
      // Enable DirectWrite font rendering if needed.
      gfx::win::MaybeInitializeDirectWrite();
#endif  // OS_WIN

      main_loop_.reset(new BrowserMainLoop(parameters));

      main_loop_->Init();

      main_loop_->EarlyInitialization();

      // Must happen before we try to use a message loop or display any UI.
      if (!main_loop_->InitializeToolkit())
        return 1;

      main_loop_->PreMainMessageLoopStart();
      main_loop_->MainMessageLoopStart();
      main_loop_->PostMainMessageLoopStart();

// WARNING: If we get a WM_ENDSESSION, objects created on the stack here
// are NOT deleted. If you need something to run during WM_ENDSESSION add it
// to browser_shutdown::Shutdown or BrowserProcess::EndSession.

      ui::InitializeInputMethod();
    }
    main_loop_->CreateStartupTasks();
    int result_code = main_loop_->GetResultCode();
    if (result_code > 0)
      return result_code;

    // Return -1 to indicate no early termination.
    return -1;
  }

  int Run() override {
    DCHECK(initialization_started_);
    DCHECK(!is_shutdown_);
    main_loop_->RunMainMessageLoopParts();
    return main_loop_->GetResultCode();
  }

  void Shutdown() override {
    DCHECK(initialization_started_);
    DCHECK(!is_shutdown_);
#ifdef LEAK_SANITIZER
    // Invoke leak detection now, to avoid dealing with shutdown-only leaks.
    // Normally this will have already happened in
    // BroserProcessImpl::ReleaseModule(), so this call has no effect. This is
    // only for processes which do not instantiate a BrowserProcess.
    // If leaks are found, the process will exit here.
    __lsan_do_leak_check();
#endif
    // If startup tracing has not been finished yet, replace it's dumper
    // with special version, which would save trace file on exit (i.e.
    // startup tracing becomes a version of shutdown tracing).
    scoped_ptr<BrowserShutdownProfileDumper> startup_profiler;
    if (main_loop_->is_tracing_startup()) {
      main_loop_->StopStartupTracingTimer();
      if (main_loop_->startup_trace_file() !=
          base::FilePath().AppendASCII("none")) {
        startup_profiler.reset(
            new BrowserShutdownProfileDumper(main_loop_->startup_trace_file()));
      }
    }

    // The shutdown tracing got enabled in AttemptUserExit earlier, but someone
    // needs to write the result to disc. For that a dumper needs to get created
    // which will dump the traces to disc when it gets destroyed.
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    scoped_ptr<BrowserShutdownProfileDumper> shutdown_profiler;
    if (command_line.HasSwitch(switches::kTraceShutdown)) {
      shutdown_profiler.reset(new BrowserShutdownProfileDumper(
          BrowserShutdownProfileDumper::GetShutdownProfileFileName()));
    }

    {
      // The trace event has to stay between profiler creation and destruction.
      TRACE_EVENT0("shutdown", "BrowserMainRunner");
      g_exited_main_message_loop = true;

      main_loop_->ShutdownThreadsAndCleanUp();

      ui::ShutdownInputMethod();
  #if defined(OS_WIN)
      ole_initializer_.reset(NULL);
  #endif
  #if defined(OS_ANDROID)
      // Forcefully terminates the RunLoop inside MessagePumpForUI, ensuring
      // proper shutdown for content_browsertests. Shutdown() is not used by
      // the actual browser.
      if (base::MessageLoop::current()->is_running())
        base::MessageLoop::current()->QuitNow();
  #endif
      main_loop_.reset(NULL);

      notification_service_.reset(NULL);

      is_shutdown_ = true;
    }
  }

 protected:
  // True if we have started to initialize the runner.
  bool initialization_started_;

  // True if the runner has been shut down.
  bool is_shutdown_;

  scoped_ptr<NotificationServiceImpl> notification_service_;
  scoped_ptr<BrowserMainLoop> main_loop_;
#if defined(OS_WIN)
  scoped_ptr<ui::ScopedOleInitializer> ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserMainRunnerImpl);
};

// static
BrowserMainRunner* BrowserMainRunner::Create() {
  return new BrowserMainRunnerImpl();
}

}  // namespace content
