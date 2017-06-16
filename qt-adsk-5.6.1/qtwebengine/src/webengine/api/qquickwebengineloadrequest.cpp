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

#include <qquickwebengineloadrequest_p.h>

QT_BEGIN_NAMESPACE

class QQuickWebEngineLoadRequestPrivate {
public:
    QQuickWebEngineLoadRequestPrivate(const QUrl& url, QQuickWebEngineView::LoadStatus status, const QString& errorString, int errorCode, QQuickWebEngineView::ErrorDomain errorDomain)
        : url(url)
        , status(status)
        , errorString(errorString)
        , errorCode(errorCode)
        , errorDomain(errorDomain)
    {
    }

    QUrl url;
    QQuickWebEngineView::LoadStatus status;
    QString errorString;
    int errorCode;
    QQuickWebEngineView::ErrorDomain errorDomain;
};

/*!
    \qmltype WebEngineLoadRequest
    \instantiates QQuickWebEngineLoadRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.0

    \brief A utility type for the WebEngineView::loadingChanged signal.

    Contains information about a request for loading a web page, such as the URL and
    current loading status (started, succeeded, failed).

    \sa WebEngineView::loadingChanged
*/
QQuickWebEngineLoadRequest::QQuickWebEngineLoadRequest(const QUrl& url, QQuickWebEngineView::LoadStatus status, const QString& errorString, int errorCode, QQuickWebEngineView::ErrorDomain errorDomain, QObject* parent)
    : QObject(parent)
    , d_ptr(new QQuickWebEngineLoadRequestPrivate(url, status, errorString, errorCode, errorDomain))
{
}

QQuickWebEngineLoadRequest::~QQuickWebEngineLoadRequest()
{
}

/*!
    \qmlproperty url WebEngineLoadRequest::url
    \brief Holds the URL of the load request.
 */
QUrl QQuickWebEngineLoadRequest::url() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->url;
}

/*!
    \qmlproperty enumeration WebEngineLoadRequest::status

    This enumeration represents the load status of a web page load request:

    \value WebEngineView::LoadStartedStatus Page is currently loading.
    \value WebEngineView::LoadSucceededStatus Page has been loaded with success.
    \value WebEngineView::LoadFailedStatus Page could not be loaded.

    \sa WebEngineView::loadingChanged
*/
QQuickWebEngineView::LoadStatus QQuickWebEngineLoadRequest::status() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->status;
}

/*!
    \qmlproperty string WebEngineLoadRequest::errorString
    \brief Holds the error message.
*/
QString QQuickWebEngineLoadRequest::errorString() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->errorString;
}

QQuickWebEngineView::ErrorDomain QQuickWebEngineLoadRequest::errorDomain() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->errorDomain;
}

/*!
    \qmlproperty int WebEngineLoadRequest::errorCode
    \brief Holds the error code.
*/
int QQuickWebEngineLoadRequest::errorCode() const
{
    Q_D(const QQuickWebEngineLoadRequest);
    return d->errorCode;
}

QT_END_NAMESPACE
