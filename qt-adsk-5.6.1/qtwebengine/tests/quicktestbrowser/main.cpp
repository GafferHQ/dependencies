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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "utils.h"

#ifndef QT_NO_WIDGETS
#include <QtWidgets/QApplication>
typedef QApplication Application;
#else
#include <QtGui/QGuiApplication>
typedef QGuiApplication Application;
#endif
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlComponent>
#include <QtWebEngine/qtwebengineglobal.h>
#include <QtWebEngine/QQuickWebEngineProfile>
#include <QtWebEngineCore/qwebenginecookiestore.h>

static QUrl startupUrl()
{
    QUrl ret;
    QStringList args(qApp->arguments());
    args.takeFirst();
    Q_FOREACH (const QString& arg, args) {
        if (arg.startsWith(QLatin1Char('-')))
             continue;
        ret = Utils::fromUserInput(arg);
        if (ret.isValid())
            return ret;
    }
    return QUrl(QStringLiteral("http://qt.io/"));
}

int main(int argc, char **argv)
{
    Application app(argc, argv);

    // Enable dev tools by default for the test browser
    if (qgetenv("QTWEBENGINE_REMOTE_DEBUGGING").isNull())
        qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "1337");
    QtWebEngine::initialize();

    QQmlApplicationEngine appEngine;
    Utils utils;
    appEngine.rootContext()->setContextProperty("utils", &utils);
    appEngine.load(QUrl("qrc:/ApplicationRoot.qml"));
    QObject *rootObject = appEngine.rootObjects().first();

    QQuickWebEngineProfile *profile = new QQuickWebEngineProfile(rootObject);

    const QMetaObject *rootMeta = rootObject->metaObject();
    int index = rootMeta->indexOfProperty("testProfile");
    Q_ASSERT(index != -1);
    QMetaProperty profileProperty = rootMeta->property(index);
    profileProperty.write(rootObject, qVariantFromValue(profile));

    QMetaObject::invokeMethod(rootObject, "load", Q_ARG(QVariant, startupUrl()));

    return app.exec();
}
