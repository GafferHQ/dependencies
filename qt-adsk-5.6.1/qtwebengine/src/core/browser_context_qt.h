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

#ifndef BROWSER_CONTEXT_QT_H
#define BROWSER_CONTEXT_QT_H

#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request_context.h"

#include <QtCore/qcompilerdetection.h> // Needed for Q_DECL_OVERRIDE

namespace QtWebEngineCore {

class BrowserContextAdapter;
class PermissionManagerQt;
class SSLHostStateDelegateQt;
class URLRequestContextGetterQt;

class BrowserContextQt : public content::BrowserContext
{
public:
    explicit BrowserContextQt(BrowserContextAdapter *);

    virtual ~BrowserContextQt();

    virtual base::FilePath GetPath() const Q_DECL_OVERRIDE;
    base::FilePath GetCachePath() const;
    virtual bool IsOffTheRecord() const Q_DECL_OVERRIDE;

    virtual net::URLRequestContextGetter *GetRequestContext() Q_DECL_OVERRIDE;
    virtual net::URLRequestContextGetter *GetRequestContextForRenderProcess(int) Q_DECL_OVERRIDE;
    virtual net::URLRequestContextGetter *GetMediaRequestContext() Q_DECL_OVERRIDE;
    virtual net::URLRequestContextGetter *GetMediaRequestContextForRenderProcess(int) Q_DECL_OVERRIDE;
    virtual net::URLRequestContextGetter *GetMediaRequestContextForStoragePartition(const base::FilePath&, bool) Q_DECL_OVERRIDE;
    virtual content::ResourceContext *GetResourceContext() Q_DECL_OVERRIDE;
    virtual content::DownloadManagerDelegate *GetDownloadManagerDelegate() Q_DECL_OVERRIDE;
    virtual content::BrowserPluginGuestManager* GetGuestManager() Q_DECL_OVERRIDE;
    virtual storage::SpecialStoragePolicy *GetSpecialStoragePolicy() Q_DECL_OVERRIDE;
    virtual content::PushMessagingService* GetPushMessagingService() Q_DECL_OVERRIDE;
    virtual content::SSLHostStateDelegate* GetSSLHostStateDelegate() Q_DECL_OVERRIDE;
    net::URLRequestContextGetter *CreateRequestContext(content::ProtocolHandlerMap *protocol_handlers, content::URLRequestInterceptorScopedVector request_interceptors);
    virtual scoped_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(const base::FilePath& partition_path) Q_DECL_OVERRIDE;
    virtual content::PermissionManager *GetPermissionManager() Q_DECL_OVERRIDE;

    BrowserContextAdapter *adapter() { return m_adapter; }

private:
    friend class ContentBrowserClientQt;
    friend class WebContentsAdapter;
    scoped_ptr<content::ResourceContext> resourceContext;
    scoped_refptr<URLRequestContextGetterQt> url_request_getter_;
    scoped_ptr<PermissionManagerQt> permissionManager;
    scoped_ptr<SSLHostStateDelegateQt> sslHostStateDelegate;
    BrowserContextAdapter *m_adapter;
    friend class BrowserContextAdapter;

    DISALLOW_COPY_AND_ASSIGN(BrowserContextQt);
};

} // namespace QtWebEngineCore

#endif // BROWSER_CONTEXT_QT_H
