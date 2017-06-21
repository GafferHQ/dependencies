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

#ifndef RESOURCE_DISPATCHER_HOST_DELEGATE_QT_H
#define RESOURCE_DISPATCHER_HOST_DELEGATE_QT_H

#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"

#include "web_contents_adapter_client.h"

namespace QtWebEngineCore {

class AuthenticationDialogController;

class ResourceDispatcherHostLoginDelegateQt : public content::ResourceDispatcherHostLoginDelegate {
public:
    ResourceDispatcherHostLoginDelegateQt(net::AuthChallengeInfo *authInfo, net::URLRequest *request);
    ~ResourceDispatcherHostLoginDelegateQt();

    // ResourceDispatcherHostLoginDelegate implementation
    virtual void OnRequestCancelled();

    QUrl url() const;
    QString realm() const;
    QString host() const;
    bool isProxy() const;

    void sendAuthToRequester(bool success, const QString &user, const QString &password);

private:
    void triggerDialog();
    void destroy();

    QUrl m_url;
    QString m_realm;
    bool m_isProxy;
    QString m_host;

    int m_renderProcessId;
    int m_renderFrameId;

    net::AuthChallengeInfo *m_authInfo;

    // The request that wants login data.
    // Must only be accessed on the IO thread.
    net::URLRequest *m_request;

    // This member is used to keep authentication dialog controller alive until
    // authorization is sent or cancelled.
    QSharedPointer<AuthenticationDialogController> m_dialogController;
};

class ResourceDispatcherHostDelegateQt : public content::ResourceDispatcherHostDelegate {
public:
    virtual bool HandleExternalProtocol(const GURL& url, int child_id, int route_id,
                                        bool is_main_frame, ui::PageTransition page_transition, bool has_user_gesture) Q_DECL_OVERRIDE;

    virtual content::ResourceDispatcherHostLoginDelegate* CreateLoginDelegate(net::AuthChallengeInfo *authInfo, net::URLRequest *request) Q_DECL_OVERRIDE;
};

} // namespace QtWebEngineCore

#endif // RESOURCE_DISPATCHER_HOST_DELEGATE_QT_H
