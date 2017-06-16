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

#include <QtQml/qqmlextensionplugin.h>
#include <QtWebEngine/QQuickWebEngineProfile>

#include "qquickwebenginecertificateerror_p.h"
#include "qquickwebenginedownloaditem_p.h"
#include "qquickwebenginehistory_p.h"
#include "qquickwebengineloadrequest_p.h"
#include "qquickwebenginenavigationrequest_p.h"
#include "qquickwebenginenewviewrequest_p.h"
#include "qquickwebenginesettings_p.h"
#include "qquickwebenginesingleton_p.h"
#include "qquickwebengineview_p.h"
#include "qtwebengineversion.h"

QT_BEGIN_NAMESPACE

static QObject *webEngineSingletonProvider(QQmlEngine *, QJSEngine *)
{
    return new QQuickWebEngineSingleton;
}

class QtWebEnginePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")
public:
    virtual void registerTypes(const char *uri) Q_DECL_OVERRIDE
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtWebEngine"));

        qmlRegisterType<QQuickWebEngineView>(uri, 1, 0, "WebEngineView");
        qmlRegisterUncreatableType<QQuickWebEngineLoadRequest>(uri, 1, 0, "WebEngineLoadRequest", tr("Cannot create separate instance of WebEngineLoadRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineNavigationRequest>(uri, 1, 0, "WebEngineNavigationRequest", tr("Cannot create separate instance of WebEngineNavigationRequest"));

        qmlRegisterType<QQuickWebEngineView, 1>(uri, 1, 1, "WebEngineView");
        qmlRegisterType<QQuickWebEngineView, 2>(uri, 1, 2, "WebEngineView");
        qmlRegisterType<QQuickWebEngineProfile>(uri, 1, 1, "WebEngineProfile");
        qmlRegisterType<QQuickWebEngineProfile, 1>(uri, 1, 2, "WebEngineProfile");
        qmlRegisterType<QQuickWebEngineScript>(uri, 1, 1, "WebEngineScript");
        qmlRegisterUncreatableType<QQuickWebEngineCertificateError>(uri, 1, 1, "WebEngineCertificateError", tr("Cannot create separate instance of WebEngineCertificateError"));
        qmlRegisterUncreatableType<QQuickWebEngineDownloadItem>(uri, 1, 1, "WebEngineDownloadItem",
            tr("Cannot create a separate instance of WebEngineDownloadItem"));
        qmlRegisterUncreatableType<QQuickWebEngineDownloadItem, 1>(uri, 1, 2, "WebEngineDownloadItem",
            tr("Cannot create a separate instance of WebEngineDownloadItem"));
        qmlRegisterUncreatableType<QQuickWebEngineNewViewRequest>(uri, 1, 1, "WebEngineNewViewRequest", tr("Cannot create separate instance of WebEngineNewViewRequest"));
        qmlRegisterUncreatableType<QQuickWebEngineSettings>(uri, 1, 1, "WebEngineSettings", tr("Cannot create a separate instance of WebEngineSettings"));
        // FIXME(QTBUG-40043): qmlRegisterUncreatableType<QQuickWebEngineSettings, 1>(uri, 1, 2, "WebEngineSettings", tr("Cannot create a separate instance of WebEngineSettings"));
        qmlRegisterSingletonType<QQuickWebEngineSingleton>(uri, 1, 1, "WebEngine", webEngineSingletonProvider);
        qmlRegisterUncreatableType<QQuickWebEngineHistory>(uri, 1, 1, "NavigationHistory",
            tr("Cannot create a separate instance of NavigationHistory"));
        qmlRegisterUncreatableType<QQuickWebEngineHistoryListModel>(uri, 1, 1, "NavigationHistoryListModel",
            tr("Cannot create a separate instance of NavigationHistory"));
        qmlRegisterUncreatableType<QQuickWebEngineFullScreenRequest>(uri, 1, 1, "FullScreenRequest",
            tr("Cannot create a separate instance of FullScreenRequest"));

        // For now (1.x import), the latest revision matches the minor version of the import.
        qmlRegisterRevision<QQuickWebEngineView, LATEST_WEBENGINEVIEW_REVISION>(uri, 1, LATEST_WEBENGINEVIEW_REVISION);
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
