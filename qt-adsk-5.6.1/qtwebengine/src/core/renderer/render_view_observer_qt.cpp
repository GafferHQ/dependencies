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

#include "renderer/render_view_observer_qt.h"

#include "common/qt_messages.h"

#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

RenderViewObserverQt::RenderViewObserverQt(
        content::RenderView* render_view,
        web_cache::WebCacheRenderProcessObserver* web_cache_render_process_observer)
    : content::RenderViewObserver(render_view)
    , m_web_cache_render_process_observer(web_cache_render_process_observer)
{
}

void RenderViewObserverQt::onFetchDocumentMarkup(quint64 requestId)
{
    Send(new RenderViewObserverHostQt_DidFetchDocumentMarkup(
        routing_id(),
        requestId,
        render_view()->GetWebView()->mainFrame()->contentAsMarkup()));
}

void RenderViewObserverQt::onFetchDocumentInnerText(quint64 requestId)
{
    Send(new RenderViewObserverHostQt_DidFetchDocumentInnerText(
        routing_id(),
        requestId,
        render_view()->GetWebView()->mainFrame()->contentAsText(std::numeric_limits<std::size_t>::max())));
}

void RenderViewObserverQt::onSetBackgroundColor(quint32 color)
{
    render_view()->GetWebView()->setBaseBackgroundColor(color);
}

void RenderViewObserverQt::OnFirstVisuallyNonEmptyLayout()
{
    Send(new RenderViewObserverHostQt_DidFirstVisuallyNonEmptyLayout(routing_id()));
}

bool RenderViewObserverQt::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(RenderViewObserverQt, message)
        IPC_MESSAGE_HANDLER(RenderViewObserverQt_FetchDocumentMarkup, onFetchDocumentMarkup)
        IPC_MESSAGE_HANDLER(RenderViewObserverQt_FetchDocumentInnerText, onFetchDocumentInnerText)
        IPC_MESSAGE_HANDLER(RenderViewObserverQt_SetBackgroundColor, onSetBackgroundColor)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
}

void RenderViewObserverQt::Navigate(const GURL &)
{
    if (m_web_cache_render_process_observer)
        m_web_cache_render_process_observer->ExecutePendingClearCache();
}
