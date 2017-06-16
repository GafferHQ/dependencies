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
#ifndef CONTENT_RENDERER_CLIENT_QT_H
#define CONTENT_RENDERER_CLIENT_QT_H

#include "content/public/renderer/content_renderer_client.h"

#include <QtGlobal>
#include <QScopedPointer>

namespace content {
class RenderProcessObserver;
}

namespace visitedlink {
class VisitedLinkSlave;
}

namespace web_cache {
class WebCacheRenderProcessObserver;
}

namespace QtWebEngineCore {

class ContentRendererClientQt : public content::ContentRendererClient {
public:
    ContentRendererClientQt();
    ~ContentRendererClientQt();
    virtual void RenderThreadStarted() Q_DECL_OVERRIDE;
    virtual void RenderViewCreated(content::RenderView *render_view) Q_DECL_OVERRIDE;
    virtual void RenderFrameCreated(content::RenderFrame* render_frame) Q_DECL_OVERRIDE;
    virtual bool ShouldSuppressErrorPage(content::RenderFrame *, const GURL &) Q_DECL_OVERRIDE;
    virtual bool HasErrorPage(int httpStatusCode, std::string *errorDomain) Q_DECL_OVERRIDE;
    virtual void GetNavigationErrorStrings(content::RenderView* renderView, blink::WebFrame* frame, const blink::WebURLRequest& failedRequest
            , const blink::WebURLError& error, std::string* errorHtml, base::string16* errorDescription) Q_DECL_OVERRIDE;

    virtual unsigned long long VisitedLinkHash(const char *canonicalUrl, size_t length) Q_DECL_OVERRIDE;
    virtual bool IsLinkVisited(unsigned long long linkHash) Q_DECL_OVERRIDE;

private:
    QScopedPointer<visitedlink::VisitedLinkSlave> m_visitedLinkSlave;
    QScopedPointer<web_cache::WebCacheRenderProcessObserver> m_webCacheObserver;
    QScopedPointer<content::RenderProcessObserver> m_renderProcessObserver;
};

} // namespace

#endif // CONTENT_RENDERER_CLIENT_QT_H
