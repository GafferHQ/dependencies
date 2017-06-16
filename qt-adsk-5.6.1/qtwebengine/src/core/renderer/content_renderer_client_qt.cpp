/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "renderer/content_renderer_client_qt.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/localized_error.h"
#include "components/error_page/common/error_page_params.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "content/public/common/web_preferences.h"

#include "renderer/web_channel_ipc_transport.h"
#include "renderer/render_frame_observer_qt.h"
#include "renderer/render_view_observer_qt.h"
#include "renderer/user_script_controller.h"

#include "grit/renderer_resources.h"

namespace QtWebEngineCore {

static const char kHttpErrorDomain[] = "http";
static const char kQrcSchemeQt[] = "qrc";

class RenderProcessObserverQt : public content::RenderProcessObserver {
public:
    void WebKitInitialized() override
    {
        // Can only be done after blink is initialized.
        blink::WebString qrcScheme(base::ASCIIToUTF16(kQrcSchemeQt));
        // mark qrc as a secure scheme (avoids deprecation warnings)
        blink::WebSecurityPolicy::registerURLSchemeAsSecure(qrcScheme);
    }
};

ContentRendererClientQt::ContentRendererClientQt()
{
}

ContentRendererClientQt::~ContentRendererClientQt()
{
}

void ContentRendererClientQt::RenderThreadStarted()
{
    content::RenderThread *renderThread = content::RenderThread::Get();
    renderThread->RegisterExtension(WebChannelIPCTransport::getV8Extension());
    m_visitedLinkSlave.reset(new visitedlink::VisitedLinkSlave);
    m_webCacheObserver.reset(new web_cache::WebCacheRenderProcessObserver());
    m_renderProcessObserver.reset(new RenderProcessObserverQt());
    renderThread->AddObserver(m_visitedLinkSlave.data());
    renderThread->AddObserver(m_webCacheObserver.data());
    renderThread->AddObserver(UserScriptController::instance());
    renderThread->AddObserver(m_renderProcessObserver.data());
}

void ContentRendererClientQt::RenderViewCreated(content::RenderView* render_view)
{
    // RenderViewObservers destroy themselves with their RenderView.
    new RenderViewObserverQt(render_view, m_webCacheObserver.data());
    new WebChannelIPCTransport(render_view);
    UserScriptController::instance()->renderViewCreated(render_view);
}

void ContentRendererClientQt::RenderFrameCreated(content::RenderFrame* render_frame)
{
    new QtWebEngineCore::RenderFrameObserverQt(render_frame);
}

bool ContentRendererClientQt::HasErrorPage(int httpStatusCode, std::string *errorDomain)
{
    // Use an internal error page, if we have one for the status code.
    if (!LocalizedError::HasStrings(LocalizedError::kHttpErrorDomain, httpStatusCode)) {
        return false;
    }

    *errorDomain = LocalizedError::kHttpErrorDomain;
    return true;
}

bool ContentRendererClientQt::ShouldSuppressErrorPage(content::RenderFrame *frame, const GURL &)
{
    return !(frame->GetWebkitPreferences().enable_error_page);
}

// To tap into the chromium localized strings. Ripped from the chrome layer (highly simplified).
void ContentRendererClientQt::GetNavigationErrorStrings(content::RenderView* renderView, blink::WebFrame *frame, const blink::WebURLRequest &failedRequest, const blink::WebURLError &error, std::string *errorHtml, base::string16 *errorDescription)
{
    Q_UNUSED(frame)
    const bool isPost = base::EqualsASCII(failedRequest.httpMethod(), "POST");

    if (errorHtml) {
        // Use a local error page.
        int resourceId;
        base::DictionaryValue errorStrings;

        const std::string locale = content::RenderThread::Get()->GetLocale();
        // TODO(elproxy): We could potentially get better diagnostics here by first calling
        // NetErrorHelper::GetErrorStringsForDnsProbe, but that one is harder to untangle.
        LocalizedError::GetStrings(error.reason, error.domain.utf8(), error.unreachableURL, isPost
                                   , error.staleCopyInCache && !isPost, locale, renderView->GetAcceptLanguages()
                                   , scoped_ptr<error_page::ErrorPageParams>(), &errorStrings);
        resourceId = IDR_NET_ERROR_HTML;


        const base::StringPiece template_html(ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resourceId));
        if (template_html.empty())
            NOTREACHED() << "unable to load template. ID: " << resourceId;
        else // "t" is the id of the templates root node.
            *errorHtml = webui::GetTemplatesHtml(template_html, &errorStrings, "t");
    }

    if (errorDescription)
        *errorDescription = LocalizedError::GetErrorDetails(error, isPost);
}

unsigned long long ContentRendererClientQt::VisitedLinkHash(const char *canonicalUrl, size_t length)
{
    return m_visitedLinkSlave->ComputeURLFingerprint(canonicalUrl, length);
}

bool ContentRendererClientQt::IsLinkVisited(unsigned long long linkHash)
{
    return m_visitedLinkSlave->IsVisited(linkHash);
}

} // namespace
