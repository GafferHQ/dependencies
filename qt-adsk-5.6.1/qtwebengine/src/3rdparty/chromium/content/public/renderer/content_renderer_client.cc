// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

#include "content/public/renderer/media_stream_renderer_factory.h"
#include "media/base/renderer_factory.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerClient.h"
#include "third_party/WebKit/public/web/WebPluginPlaceholder.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return nullptr;
}

SkBitmap* ContentRendererClient::GetSadWebViewBitmap() {
  return nullptr;
}

scoped_ptr<blink::WebPluginPlaceholder>
ContentRendererClient::CreatePluginPlaceholder(
    RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) {
  return nullptr;
}

bool ContentRendererClient::OverrideCreatePlugin(
    RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  return false;
}

blink::WebPlugin* ContentRendererClient::CreatePluginReplacement(
    RenderFrame* render_frame,
    const base::FilePath& plugin_path) {
  return nullptr;
}

bool ContentRendererClient::HasErrorPage(int http_status_code,
                                         std::string* error_domain) {
  return false;
}

bool ContentRendererClient::ShouldSuppressErrorPage(RenderFrame* render_frame,
                                                    const GURL& url) {
  return false;
}

void ContentRendererClient::DeferMediaLoad(RenderFrame* render_frame,
                                           const base::Closure& closure) {
  closure.Run();
}

blink::WebMediaStreamCenter*
ContentRendererClient::OverrideCreateWebMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client) {
  return nullptr;
}

blink::WebRTCPeerConnectionHandler*
ContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client) {
  return nullptr;
}

blink::WebMIDIAccessor*
ContentRendererClient::OverrideCreateMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  return nullptr;
}

blink::WebAudioDevice*
ContentRendererClient::OverrideCreateAudioDevice(
    double sample_rate) {
  return nullptr;
}

blink::WebClipboard* ContentRendererClient::OverrideWebClipboard() {
  return nullptr;
}

blink::WebThemeEngine* ContentRendererClient::OverrideThemeEngine() {
  return nullptr;
}

blink::WebSpeechSynthesizer* ContentRendererClient::OverrideSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return nullptr;
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowPopup() {
  return false;
}

#ifdef OS_ANDROID
bool ContentRendererClient::HandleNavigation(
    RenderFrame* render_frame,
    DocumentState* document_state,
    int opener_id,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  return false;
}
#endif

bool ContentRendererClient::ShouldFork(blink::WebLocalFrame* frame,
                                       const GURL& url,
                                       const std::string& http_method,
                                       bool is_initial_navigation,
                                       bool is_server_redirect,
                                       bool* send_referrer) {
  return false;
}

bool ContentRendererClient::WillSendRequest(
    blink::WebFrame* frame,
    ui::PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  return false;
}

unsigned long long ContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool ContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

blink::WebPrescientNetworking*
ContentRendererClient::GetPrescientNetworking() {
  return nullptr;
}

bool ContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderFrame* render_frame,
    blink::WebPageVisibilityState* override_state) {
  return false;
}

const void* ContentRendererClient::CreatePPAPIInterface(
    const std::string& interface_name) {
  return nullptr;
}

bool ContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  return false;
}

bool ContentRendererClient::AllowPepperMediaStreamAPI(const GURL& url) {
  return false;
}

void ContentRendererClient::AddKeySystems(
    std::vector<media::KeySystemInfo>* key_systems) {
}

scoped_ptr<media::RendererFactory>
ContentRendererClient::CreateMediaRendererFactory(
    RenderFrame* render_frame,
    const scoped_refptr<media::GpuVideoAcceleratorFactories>& gpu_factories,
    const scoped_refptr<media::MediaLog>& media_log) {
  return nullptr;
}

scoped_ptr<MediaStreamRendererFactory>
ContentRendererClient::CreateMediaStreamRendererFactory() {
  return nullptr;
}

bool ContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) const {
  return false;
}

bool ContentRendererClient::ShouldGatherSiteIsolationStats() const {
  return true;
}

blink::WebWorkerContentSettingsClientProxy*
ContentRendererClient::CreateWorkerContentSettingsClientProxy(
    RenderFrame* render_frame, blink::WebFrame* frame) {
  return nullptr;
}

bool ContentRendererClient::IsPluginAllowedToUseCameraDeviceAPI(
    const GURL& url) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToUseCompositorAPI(const GURL& url) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
  return false;
}

BrowserPluginDelegate* ContentRendererClient::CreateBrowserPluginDelegate(
    RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
  return nullptr;
}

std::string ContentRendererClient::GetUserAgentOverrideForURL(const GURL& url) {
  return std::string();
}

scoped_ptr<blink::WebAppBannerClient>
ContentRendererClient::CreateAppBannerClient(RenderFrame* render_frame) {
  return nullptr;
}

}  // namespace content
