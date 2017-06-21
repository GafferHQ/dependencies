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

#ifndef CERTIFICATE_ERROR_CONTROLLER_P_H
#define CERTIFICATE_ERROR_CONTROLLER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "content/public/browser/content_browser_client.h"

#include "certificate_error_controller.h"

QT_BEGIN_NAMESPACE

class CertificateErrorControllerPrivate {
public:
    CertificateErrorControllerPrivate(int cert_error, const net::SSLInfo& ssl_info, const GURL& request_url, content::ResourceType resource_type, bool overridable, bool strict_enforcement, const base::Callback<void(bool)>& callback);

    void accept(bool accepted);

    CertificateErrorController::CertificateError certError;
    const QUrl requestUrl;
    QDateTime validStart;
    QDateTime validExpiry;
    CertificateErrorController::ResourceType resourceType;
    bool overridable;
    bool strictEnforcement;
    const base::Callback<void(bool)> callback;
};

QT_END_NAMESPACE

#endif // CERTIFICATE_ERROR_CONTROLLER_P_H
