// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/debug_urls.h"

#if defined(SYZYASAN)
#include <windows.h>
#endif

#include <vector>

#include "base/command_line.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/profiler.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/switches.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "url/gurl.h"

#if defined(ENABLE_PLUGINS)
#include "content/browser/ppapi_plugin_process_host.h"
#endif

namespace content {

namespace {

// Define the Asan debug URLs.
const char kAsanCrashDomain[] = "crash";
const char kAsanHeapOverflow[] = "/browser-heap-overflow";
const char kAsanHeapUnderflow[] = "/browser-heap-underflow";
const char kAsanUseAfterFree[] = "/browser-use-after-free";
#if defined(SYZYASAN)
const char kAsanCorruptHeapBlock[] = "/browser-corrupt-heap-block";
const char kAsanCorruptHeap[] = "/browser-corrupt-heap";
#endif

#if defined(KASKO)
// Define the Kasko debug URLs.
const char kKaskoCrashDomain[] = "kasko";
const char kKaskoSendReport[] = "/send-report";
#endif

void HandlePpapiFlashDebugURL(const GURL& url) {
#if defined(ENABLE_PLUGINS)
  bool crash = url == GURL(kChromeUIPpapiFlashCrashURL);

  std::vector<PpapiPluginProcessHost*> hosts;
  PpapiPluginProcessHost::FindByName(
      base::UTF8ToUTF16(kFlashPluginName), &hosts);
  for (std::vector<PpapiPluginProcessHost*>::iterator iter = hosts.begin();
       iter != hosts.end(); ++iter) {
    if (crash)
      (*iter)->Send(new PpapiMsg_Crash());
    else
      (*iter)->Send(new PpapiMsg_Hang());
  }
#endif
}

bool IsKaskoDebugURL(const GURL& url) {
#if defined(KASKO)
  return (url.is_valid() && url.SchemeIs(kChromeUIScheme) &&
          url.DomainIs(kKaskoCrashDomain, sizeof(kKaskoCrashDomain) - 1) &&
          url.path() == kKaskoSendReport);
#else
  return false;
#endif
}

void HandleKaskoDebugURL() {
#if defined(KASKO)
  // Signature of an enhanced crash reporting function.
  typedef void(__cdecl * ReportCrashWithProtobufPtr)(EXCEPTION_POINTERS*,
                                                     const char*);

  HMODULE exe_hmodule = ::GetModuleHandle(NULL);
  ReportCrashWithProtobufPtr report_crash_with_protobuf =
      reinterpret_cast<ReportCrashWithProtobufPtr>(
          ::GetProcAddress(exe_hmodule, "ReportCrashWithProtobuf"));
  if (report_crash_with_protobuf)
    report_crash_with_protobuf(NULL, "Invoked from debug url.");
  else
    NOTREACHED();
#else
  NOTIMPLEMENTED();
#endif
}

bool IsAsanDebugURL(const GURL& url) {
#if defined(SYZYASAN)
  if (!base::debug::IsBinaryInstrumented())
    return false;
#endif

  if (!(url.is_valid() && url.SchemeIs(kChromeUIScheme) &&
        url.DomainIs(kAsanCrashDomain, sizeof(kAsanCrashDomain) - 1) &&
        url.has_path())) {
    return false;
  }

  if (url.path() == kAsanHeapOverflow || url.path() == kAsanHeapUnderflow ||
      url.path() == kAsanUseAfterFree) {
    return true;
  }

#if defined(SYZYASAN)
  if (url.path() == kAsanCorruptHeapBlock || url.path() == kAsanCorruptHeap)
    return true;
#endif

  return false;
}

bool HandleAsanDebugURL(const GURL& url) {
#if defined(SYZYASAN)
  if (!base::debug::IsBinaryInstrumented())
    return false;

  if (url.path() == kAsanCorruptHeapBlock) {
    base::debug::AsanCorruptHeapBlock();
    return true;
  } else if (url.path() == kAsanCorruptHeap) {
    base::debug::AsanCorruptHeap();
    return true;
  }
#endif

#if defined(ADDRESS_SANITIZER) || defined(SYZYASAN)
  if (url.path() == kAsanHeapOverflow) {
    base::debug::AsanHeapOverflow();
  } else if (url.path() == kAsanHeapUnderflow) {
    base::debug::AsanHeapUnderflow();
  } else if (url.path() == kAsanUseAfterFree) {
    base::debug::AsanHeapUseAfterFree();
  } else {
    return false;
  }
#endif

  return true;
}


}  // namespace

class ScopedAllowWaitForDebugURL {
 private:
  base::ThreadRestrictions::ScopedAllowWait wait;
};

bool HandleDebugURL(const GURL& url, ui::PageTransition transition) {
  // Ensure that the user explicitly navigated to this URL, unless
  // kEnableGpuBenchmarking is enabled by Telemetry.
  bool is_telemetry_navigation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableGpuBenchmarking) &&
      (transition & ui::PAGE_TRANSITION_TYPED);

  if (!(transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) &&
      !is_telemetry_navigation)
    return false;

  if (IsAsanDebugURL(url))
    return HandleAsanDebugURL(url);

  if (IsKaskoDebugURL(url)) {
    HandleKaskoDebugURL();
    return true;
  }

  if (url == GURL(kChromeUIBrowserCrashURL)) {
    // Induce an intentional crash in the browser process.
    CHECK(false);
    return true;
  }

  if (url == GURL(kChromeUIBrowserUIHang)) {
    ScopedAllowWaitForDebugURL allow_wait;
    base::WaitableEvent(false, false).Wait();
    return true;
  }

  if (url == GURL(kChromeUIGpuCleanURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateRemoveAllContext();
    return true;
  }

  if (url == GURL(kChromeUIGpuCrashURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateCrash();
    return true;
  }

  if (url == GURL(kChromeUIGpuHangURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateHang();
    return true;
  }

  if (url == GURL(kChromeUIPpapiFlashCrashURL) ||
      url == GURL(kChromeUIPpapiFlashHangURL)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&HandlePpapiFlashDebugURL, url));
    return true;
  }

  return false;
}

bool IsRendererDebugURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs(url::kJavaScriptScheme))
    return true;

  return url == GURL(kChromeUICrashURL) ||
         url == GURL(kChromeUIDumpURL) ||
         url == GURL(kChromeUIKillURL) ||
         url == GURL(kChromeUIHangURL) ||
         url == GURL(kChromeUIShorthangURL);
}

}  // namespace content
