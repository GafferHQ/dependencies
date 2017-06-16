/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgenericunixservices_p.h"

#include <QtCore/QStandardPaths>
#include <QtCore/QProcess>
#include <QtCore/QUrl>
#include <QtCore/QDebug>

#include <stdlib.h>

QT_BEGIN_NAMESPACE

enum { debug = 0 };

static inline QByteArray detectDesktopEnvironment()
{
    const QByteArray xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    if (!xdgCurrentDesktop.isEmpty())
        return xdgCurrentDesktop.toUpper(); // KDE, GNOME, UNITY, LXDE, MATE, XFCE...

    // Classic fallbacks
    if (!qEnvironmentVariableIsEmpty("KDE_FULL_SESSION"))
        return QByteArrayLiteral("KDE");
    if (!qEnvironmentVariableIsEmpty("GNOME_DESKTOP_SESSION_ID"))
        return QByteArrayLiteral("GNOME");

    // Fallback to checking $DESKTOP_SESSION (unreliable)
    const QByteArray desktopSession = qgetenv("DESKTOP_SESSION");
    if (desktopSession == "gnome")
        return QByteArrayLiteral("GNOME");
    if (desktopSession == "xfce")
        return QByteArrayLiteral("XFCE");

    return QByteArrayLiteral("UNKNOWN");
}

static inline bool checkExecutable(const QString &candidate, QString *result)
{
    *result = QStandardPaths::findExecutable(candidate);
    return !result->isEmpty();
}

static inline bool detectWebBrowser(const QByteArray &desktop,
                                    bool checkBrowserVariable,
                                    QString *browser)
{
    const char *browsers[] = {"google-chrome", "firefox", "mozilla", "opera"};

    browser->clear();
    if (checkExecutable(QStringLiteral("xdg-open"), browser))
        return true;

    if (checkBrowserVariable) {
        QByteArray browserVariable = qgetenv("DEFAULT_BROWSER");
        if (browserVariable.isEmpty())
            browserVariable = qgetenv("BROWSER");
        if (!browserVariable.isEmpty() && checkExecutable(QString::fromLocal8Bit(browserVariable), browser))
            return true;
    }

    if (desktop == QByteArray("KDE")) {
        // Konqueror launcher
        if (checkExecutable(QStringLiteral("kfmclient"), browser)) {
            browser->append(QStringLiteral(" exec"));
            return true;
        }
    } else if (desktop == QByteArray("GNOME")) {
        if (checkExecutable(QStringLiteral("gnome-open"), browser))
            return true;
    }

    for (size_t i = 0; i < sizeof(browsers)/sizeof(char *); ++i)
        if (checkExecutable(QLatin1String(browsers[i]), browser))
            return true;
    return false;
}

static inline bool launch(const QString &launcher, const QUrl &url)
{
    const QString command = launcher + QLatin1Char(' ') + QLatin1String(url.toEncoded());
    if (debug)
        qDebug("Launching %s", qPrintable(command));
#if defined(QT_NO_PROCESS)
    const bool ok = ::system(qPrintable(command + QStringLiteral(" &")));
#else
    const bool ok = QProcess::startDetached(command);
#endif
    if (!ok)
        qWarning("Launch failed (%s)", qPrintable(command));
    return ok;
}

QByteArray QGenericUnixServices::desktopEnvironment() const
{
    static const QByteArray result = detectDesktopEnvironment();
    return result;
}

bool QGenericUnixServices::openUrl(const QUrl &url)
{
    if (url.scheme() == QLatin1String("mailto"))
        return openDocument(url);

    if (m_webBrowser.isEmpty() && !detectWebBrowser(desktopEnvironment(), true, &m_webBrowser)) {
        qWarning("Unable to detect a web browser to launch '%s'", qPrintable(url.toString()));
        return false;
    }
    return launch(m_webBrowser, url);
}

bool QGenericUnixServices::openDocument(const QUrl &url)
{
    if (m_documentLauncher.isEmpty() && !detectWebBrowser(desktopEnvironment(), false, &m_documentLauncher)) {
        qWarning("Unable to detect a launcher for '%s'", qPrintable(url.toString()));
        return false;
    }
    return launch(m_documentLauncher, url);
}

QT_END_NAMESPACE
