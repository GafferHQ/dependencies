// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_browser_client.h"

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_main_parts.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_network_delegate.h"
#include "chromecast/browser/cast_quota_permission_context.h"
#include "chromecast/browser/cast_resource_dispatcher_host_delegate.h"
#include "chromecast/browser/geolocation/cast_access_token_store.h"
#include "chromecast/browser/media/cma_message_filter_host.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "chromecast/common/global_descriptors.h"
#include "chromecast/media/cma/backend/media_pipeline_device.h"
#include "chromecast/media/cma/backend/media_pipeline_device_factory.h"
#include "components/crash/app/breakpad_linux.h"
#include "components/crash/browser/crash_handler_host_linux.h"
#include "components/network_hints/browser/network_hints_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "media/audio/audio_manager_factory.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_ANDROID)
#include "components/crash/browser/crash_dump_manager_android.h"
#include "components/external_video_surface/browser/android/external_video_surface_container_impl.h"
#endif  // defined(OS_ANDROID)

namespace chromecast {
namespace shell {

CastContentBrowserClient::CastContentBrowserClient()
    : url_request_context_factory_(new URLRequestContextFactory()) {
}

CastContentBrowserClient::~CastContentBrowserClient() {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::IO,
      FROM_HERE,
      url_request_context_factory_.release());
}

void CastContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line) {
}

std::vector<scoped_refptr<content::BrowserMessageFilter>>
CastContentBrowserClient::GetBrowserMessageFilters() {
  return std::vector<scoped_refptr<content::BrowserMessageFilter>>();
}

scoped_ptr<::media::AudioManagerFactory>
CastContentBrowserClient::CreateAudioManagerFactory() {
  // Return nullptr. The factory will not be set, and the statically linked
  // implementation of AudioManager will be used.
  return scoped_ptr<::media::AudioManagerFactory>();
}

#if !defined(OS_ANDROID)
scoped_ptr<media::MediaPipelineDevice>
CastContentBrowserClient::CreateMediaPipelineDevice(
    const media::MediaPipelineDeviceParams& params) {
  scoped_ptr<media::MediaPipelineDeviceFactory> factory =
      GetMediaPipelineDeviceFactory(params);
  return make_scoped_ptr(new media::MediaPipelineDevice(factory.Pass()));
}
#endif

content::BrowserMainParts* CastContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return new CastBrowserMainParts(parameters,
                                  url_request_context_factory_.get(),
                                  CreateAudioManagerFactory());
}

void CastContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
#if !defined(OS_ANDROID)
  scoped_refptr<media::CmaMessageFilterHost> cma_message_filter(
      new media::CmaMessageFilterHost(
          host->GetID(),
          base::Bind(
              &CastContentBrowserClient::CreateMediaPipelineDevice,
              base::Unretained(this))));
  host->AddFilter(cma_message_filter.get());
#endif  // !defined(OS_ANDROID)

  // Forcibly trigger I/O-thread URLRequestContext initialization before
  // getting HostResolver.
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&net::URLRequestContextGetter::GetURLRequestContext,
                 base::Unretained(
                    url_request_context_factory_->GetSystemGetter())),
      base::Bind(&CastContentBrowserClient::AddNetworkHintsMessageFilter,
                 base::Unretained(this), host->GetID()));

  auto extra_filters = GetBrowserMessageFilters();
  for (auto const& filter : extra_filters) {
    host->AddFilter(filter.get());
  }
}

void CastContentBrowserClient::AddNetworkHintsMessageFilter(
    int render_process_id, net::URLRequestContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host)
    return;

  scoped_refptr<content::BrowserMessageFilter> network_hints_message_filter(
      new network_hints::NetworkHintsMessageFilter(
          url_request_context_factory_->host_resolver()));
  host->AddFilter(network_hints_message_filter.get());
}

net::URLRequestContextGetter* CastContentBrowserClient::CreateRequestContext(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return url_request_context_factory_->CreateMainGetter(
      browser_context,
      protocol_handlers,
      request_interceptors.Pass());
}

bool CastContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  static const char* const kProtocolList[] = {
      url::kBlobScheme,
      url::kFileSystemScheme,
      content::kChromeUIScheme,
      content::kChromeDevToolsScheme,
      url::kDataScheme,
  };

  const std::string& scheme = url.scheme();
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }

  if (scheme == url::kFileScheme) {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableLocalFileAccesses);
  }

  return false;
}

void CastContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  std::string process_type =
      command_line->GetSwitchValueNative(switches::kProcessType);
  base::CommandLine* browser_command_line =
      base::CommandLine::ForCurrentProcess();

  // IsCrashReporterEnabled() is set when InitCrashReporter() is called, and
  // controlled by GetBreakpadClient()->EnableBreakpadForProcess(), therefore
  // it's ok to add switch to every process here.
  if (breakpad::IsCrashReporterEnabled()) {
    command_line->AppendSwitch(switches::kEnableCrashReporter);
  }

  // Renderer process command-line
  if (process_type == switches::kRendererProcess) {
    // Any browser command-line switches that should be propagated to
    // the renderer go here.

    if (browser_command_line->HasSwitch(switches::kEnableCmaMediaPipeline))
      command_line->AppendSwitch(switches::kEnableCmaMediaPipeline);
    if (browser_command_line->HasSwitch(switches::kEnableLegacyHolePunching))
      command_line->AppendSwitch(switches::kEnableLegacyHolePunching);
  }

#if defined(OS_LINUX)
  // Necessary for accelerated 2d canvas.  By default on Linux, Chromium assumes
  // GLES2 contexts can be lost to a power-save mode, which breaks GPU canvas
  // apps.
  if (process_type == switches::kGpuProcess) {
    command_line->AppendSwitch(switches::kGpuNoContextLost);
  }
#endif

  AppendExtraCommandLineSwitches(command_line);
}

content::AccessTokenStore* CastContentBrowserClient::CreateAccessTokenStore() {
  return new CastAccessTokenStore(
      CastBrowserProcess::GetInstance()->browser_context());
}

void CastContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  prefs->allow_scripts_to_close_windows = true;
  // TODO(lcwu): http://crbug.com/391089. This pref is set to true by default
  // because some content providers such as YouTube use plain http requests
  // to retrieve media data chunks while running in a https page. This pref
  // should be disabled once all the content providers are no longer doing that.
  prefs->allow_running_insecure_content = true;
}

void CastContentBrowserClient::ResourceDispatcherHostCreated() {
  CastBrowserProcess::GetInstance()->SetResourceDispatcherHostDelegate(
      make_scoped_ptr(new CastResourceDispatcherHostDelegate));
  content::ResourceDispatcherHost::Get()->SetDelegate(
      CastBrowserProcess::GetInstance()->resource_dispatcher_host_delegate());
}

std::string CastContentBrowserClient::GetApplicationLocale() {
  const std::string locale(base::i18n::GetConfiguredLocale());
  return locale.empty() ? "en-US" : locale;
}

content::QuotaPermissionContext*
CastContentBrowserClient::CreateQuotaPermissionContext() {
  return new CastQuotaPermissionContext();
}

void CastContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_view_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    content::ResourceType resource_type,
    bool overridable,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(bool)>& callback,
    content::CertificateRequestResultType* result) {
  // Allow developers to override certificate errors.
  // Otherwise, any fatal certificate errors will cause an abort.
  *result = content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL;
  return;
}

void CastContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    scoped_ptr<content::ClientCertificateDelegate> delegate) {
  GURL requesting_url("https://" + cert_request_info->host_and_port.ToString());

  if (!requesting_url.is_valid()) {
    LOG(ERROR) << "Invalid URL string: "
               << requesting_url.possibly_invalid_spec();
    delegate->ContinueWithCertificate(nullptr);
    return;
  }

  // In our case there are no relevant certs in the cert_request_info. The cert
  // we need to return (if permitted) is the Cast device cert, which we can
  // access directly through the ClientAuthSigner instance. However, we need to
  // be on the IO thread to determine whether the app is whitelisted to return
  // it, because CastNetworkDelegate is bound to the IO thread.
  // Subsequently, the callback must then itself be performed back here
  // on the UI thread.
  //
  // TODO(davidben): Stop using child ID to identify an app.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CastContentBrowserClient::SelectClientCertificateOnIOThread,
                 base::Unretained(this), requesting_url,
                 web_contents->GetRenderProcessHost()->GetID()),
      base::Bind(&content::ClientCertificateDelegate::ContinueWithCertificate,
                 base::Owned(delegate.release())));
}

net::X509Certificate*
CastContentBrowserClient::SelectClientCertificateOnIOThread(
    GURL requesting_url,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CastNetworkDelegate* network_delegate =
      url_request_context_factory_->app_network_delegate();
  if (network_delegate->IsWhitelisted(requesting_url,
                                      render_process_id, false)) {
    return CastNetworkDelegate::DeviceCert();
  } else {
    LOG(ERROR) << "Invalid host for client certificate request: "
               << requesting_url.host()
               << " with render_process_id: "
               << render_process_id;
    return NULL;
  }
}

bool CastContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    const blink::WebWindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    content::ResourceContext* context,
    int render_process_id,
    int opener_render_view_id,
    int opener_render_frame_id,
    bool* no_javascript_access) {
  *no_javascript_access = true;
  return false;
}

#if defined(OS_ANDROID)
void CastContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::FileDescriptorInfo* mappings,
    std::map<int, base::MemoryMappedFile::Region>* regions) {
  mappings->Share(
      kAndroidPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kAndroidPakDescriptor));
  regions->insert(std::make_pair(
      kAndroidPakDescriptor, base::GlobalDescriptors::GetInstance()->GetRegion(
                                 kAndroidPakDescriptor)));

  if (breakpad::IsCrashReporterEnabled()) {
    base::File minidump_file(
        breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFile(
            child_process_id));
    if (!minidump_file.IsValid()) {
      LOG(ERROR) << "Failed to create file for minidump, crash reporting will "
                 << "be disabled for this process.";
    } else {
      mappings->Transfer(kAndroidMinidumpDescriptor,
                         base::ScopedFD(minidump_file.TakePlatformFile()));
    }
  }
}
#else
void CastContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::FileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) && defined(VIDEO_HOLE)
content::ExternalVideoSurfaceContainer*
CastContentBrowserClient::OverrideCreateExternalVideoSurfaceContainer(
    content::WebContents* web_contents) {
  return external_video_surface::ExternalVideoSurfaceContainerImpl::Create(
      web_contents);
}
#endif  // defined(OS_ANDROID) && defined(VIDEO_HOLE)

#if !defined(OS_ANDROID)
int CastContentBrowserClient::GetCrashSignalFD(
    const base::CommandLine& command_line) {
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess ||
      process_type == switches::kGpuProcess) {
    breakpad::CrashHandlerHostLinux* crash_handler =
        crash_handlers_[process_type];
    if (!crash_handler) {
      crash_handler = CreateCrashHandlerHost(process_type);
      crash_handlers_[process_type] = crash_handler;
    }
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}

breakpad::CrashHandlerHostLinux*
CastContentBrowserClient::CreateCrashHandlerHost(
    const std::string& process_type) {
  // Let cast shell dump to /tmp. Internal minidump generator code can move it
  // to /data/minidumps later, since /data/minidumps is file lock-controlled.
  base::FilePath dumps_path;
  PathService::Get(base::DIR_TEMP, &dumps_path);

  // Alway set "upload" to false to use our own uploader.
  breakpad::CrashHandlerHostLinux* crash_handler =
    new breakpad::CrashHandlerHostLinux(
        process_type, dumps_path, false /* upload */);
  // StartUploaderThread() even though upload is diferred.
  // Breakpad-related memory is freed in the uploader thread.
  crash_handler->StartUploaderThread();
  return crash_handler;
}
#endif  // !defined(OS_ANDROID)

}  // namespace shell
}  // namespace chromecast
