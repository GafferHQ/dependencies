/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#include "qstandardpaths.h"

#ifndef QT_NO_STANDARDPATHS

#include <QtCore/private/qjni_p.h>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qmap.h>
#include <QDir>

QT_BEGIN_NAMESPACE

typedef QMap<QString, QString> AndroidDirCache;
Q_GLOBAL_STATIC(AndroidDirCache, androidDirCache)

static QString testDir()
{
    return QStandardPaths::isTestModeEnabled() ? QLatin1String("/qttest")
                                               : QLatin1String("");
}

static QJNIObjectPrivate applicationContext()
{
    static QJNIObjectPrivate appCtx;
    if (appCtx.isValid())
        return appCtx;

    QJNIObjectPrivate activity(QtAndroidPrivate::activity());
    if (!activity.isValid())
        return appCtx;

    appCtx = activity.callObjectMethod("getApplicationContext",
                                       "()Landroid/content/Context;");
    return appCtx;
}

static inline QString getAbsolutePath(const QJNIObjectPrivate &file)
{
    QJNIObjectPrivate path = file.callObjectMethod("getAbsolutePath",
                                                   "()Ljava/lang/String;");
    if (!path.isValid())
        return QString();

    return path.toString();
}

/*
 * The root of the external storage
 *
 */
static QString getExternalStorageDirectory()
{
    QString &path = (*androidDirCache)[QStringLiteral("EXT_ROOT")];
    if (!path.isEmpty())
        return path;

    QJNIObjectPrivate file = QJNIObjectPrivate::callStaticObjectMethod("android/os/Environment",
                                                                       "getExternalStorageDirectory",
                                                                       "()Ljava/io/File;");
    if (!file.isValid())
        return QString();

    return (path = getAbsolutePath(file));
}

/*
 * Locations where applications can place user files (public).
 * E.g., /storage/Music
 */
static QString getExternalStoragePublicDirectory(const char *directoryField)
{
    QString &path = (*androidDirCache)[QLatin1String(directoryField)];
    if (!path.isEmpty())
        return path;

    QJNIObjectPrivate dirField = QJNIObjectPrivate::getStaticObjectField("android/os/Environment",
                                                                         directoryField,
                                                                         "Ljava/lang/String;");
    if (!dirField.isValid())
        return QString();

    QJNIObjectPrivate file = QJNIObjectPrivate::callStaticObjectMethod("android/os/Environment",
                                                                       "getExternalStoragePublicDirectory",
                                                                       "(Ljava/lang/String;)Ljava/io/File;",
                                                                       dirField.object());
    if (!file.isValid())
        return QString();

    return (path = getAbsolutePath(file));
}

/*
 * Locations where applications can place persistent files it owns.
 * E.g., /storage/org.app/Music
 */
static QString getExternalFilesDir(const char *directoryField = 0)
{
    QString &path = (*androidDirCache)[QString(QLatin1String("APPNAME_%1")).arg(QLatin1String(directoryField))];
    if (!path.isEmpty())
        return path;

    QJNIObjectPrivate activity(QtAndroidPrivate::activity());
    if (!activity.isValid())
        return QString();

    QJNIObjectPrivate appCtx = applicationContext();
    if (!appCtx.isValid())
        return QString();

    QJNIObjectPrivate dirField = QJNIObjectPrivate::fromString(QLatin1String(""));
    if (directoryField) {
        dirField = QJNIObjectPrivate::getStaticObjectField("android/os/Environment",
                                                           directoryField,
                                                           "Ljava/lang/String;");
        if (!dirField.isValid())
            return QString();
    }

    QJNIObjectPrivate file = appCtx.callObjectMethod("getExternalFilesDir",
                                                     "(Ljava/lang/String;)Ljava/io/File;",
                                                     dirField.object());

    if (!file.isValid())
        return QString();

    return (path = getAbsolutePath(file));
}

/*
 * Directory where applications can store cache files it owns (public).
 * E.g., /storage/org.app/
 */
static QString getExternalCacheDir()
{
    QString &path = (*androidDirCache)[QStringLiteral("APPNAME_CACHE")];
    if (!path.isEmpty())
        return path;

    QJNIObjectPrivate appCtx = applicationContext();
    if (!appCtx.isValid())
        return QString();

    QJNIObjectPrivate file = appCtx.callObjectMethod("getExternalCacheDir",
                                                     "()Ljava/io/File;");

    if (!file.isValid())
        return QString();

    return (path = getAbsolutePath(file));
}

/*
 * Directory where applications can store cache files it owns (private).
 */
static QString getCacheDir()
{
    QString &path = (*androidDirCache)[QStringLiteral("APPROOT_CACHE")];
    if (!path.isEmpty())
        return path;

    QJNIObjectPrivate appCtx = applicationContext();
    if (!appCtx.isValid())
        return QString();

    QJNIObjectPrivate file = appCtx.callObjectMethod("getCacheDir",
                                                     "()Ljava/io/File;");
    if (!file.isValid())
        return QString();

    return (path = getAbsolutePath(file));
}

/*
 * Directory where applications can store files it owns (private).
 * (Same location as $HOME)
 */
static QString getFilesDir()
{
    QString &path = (*androidDirCache)[QStringLiteral("APPROOT_FILES")];
    if (!path.isEmpty())
        return path;

    return (path  = QDir::homePath());
}

QString QStandardPaths::writableLocation(StandardLocation type)
{
    switch (type) {
    case QStandardPaths::MusicLocation:
        return getExternalStoragePublicDirectory("DIRECTORY_MUSIC");
    case QStandardPaths::MoviesLocation:
        return getExternalStoragePublicDirectory("DIRECTORY_MOVIES");
    case QStandardPaths::PicturesLocation:
        return getExternalStoragePublicDirectory("DIRECTORY_PICTURES");
    case QStandardPaths::DocumentsLocation:
        if (QtAndroidPrivate::androidSdkVersion() > 18)
            return getExternalStoragePublicDirectory("DIRECTORY_DOCUMENTS");
        else
            return getExternalStorageDirectory() + QLatin1String("/Documents");
    case QStandardPaths::DownloadLocation:
        return getExternalStoragePublicDirectory("DIRECTORY_DOWNLOADS");
    case QStandardPaths::GenericConfigLocation:
    case QStandardPaths::ConfigLocation:
    case QStandardPaths::AppConfigLocation:
        return getFilesDir() + testDir() + QLatin1String("/settings");
    case QStandardPaths::GenericDataLocation:
        return getExternalStorageDirectory() + testDir();
    case QStandardPaths::AppDataLocation:
    case QStandardPaths::AppLocalDataLocation:
        return getFilesDir() + testDir();
    case QStandardPaths::GenericCacheLocation:
    case QStandardPaths::RuntimeLocation:
    case QStandardPaths::TempLocation:
    case QStandardPaths::CacheLocation:
        return getCacheDir() + testDir();
    case QStandardPaths::DesktopLocation:
    case QStandardPaths::HomeLocation:
        return getFilesDir();
    case QStandardPaths::ApplicationsLocation:
    case QStandardPaths::FontsLocation:
    default:
        break;
    }

    return QString();
}

QStringList QStandardPaths::standardLocations(StandardLocation type)
{
    if (type == MusicLocation) {
        return QStringList() << writableLocation(type)
                             << getExternalFilesDir("DIRECTORY_MUSIC")
                             << getExternalStoragePublicDirectory("DIRECTORY_PODCASTS")
                             << getExternalFilesDir("DIRECTORY_PODCASTS")
                             << getExternalStoragePublicDirectory("DIRECTORY_NOTIFICATIONS")
                             << getExternalFilesDir("DIRECTORY_NOTIFICATIONS")
                             << getExternalStoragePublicDirectory("DIRECTORY_ALARMS")
                             << getExternalFilesDir("DIRECTORY_ALARMS");
    }

    if (type == MoviesLocation) {
        return QStringList() << writableLocation(type)
                             << getExternalFilesDir("DIRECTORY_MOVIES");
    }

    if (type == PicturesLocation) {
        return QStringList()  << writableLocation(type)
                              << getExternalFilesDir("DIRECTORY_PICTURES");
    }

    if (type == DocumentsLocation) {
        if (QtAndroidPrivate::androidSdkVersion() > 18) {
            return QStringList() << writableLocation(type)
                                 << getExternalFilesDir("DIRECTORY_DOCUMENTS");
        } else {
            return QStringList() << writableLocation(type)
                                 << getExternalFilesDir() + QLatin1String("/Documents");
        }
    }

    if (type == DownloadLocation) {
        return QStringList() << writableLocation(type)
                             << getExternalFilesDir("DIRECTORY_DOWNLOADS");
    }

    if (type == AppDataLocation || type == AppLocalDataLocation) {
        return QStringList() << writableLocation(type)
                             << getExternalFilesDir();
    }

    if (type == CacheLocation) {
        return QStringList() << writableLocation(type)
                             << getExternalCacheDir();
    }

    if (type == FontsLocation) {
        QString &fontLocation = (*androidDirCache)[QStringLiteral("FONT_LOCATION")];
        if (!fontLocation.isEmpty())
            return QStringList(fontLocation);

        const QByteArray ba = qgetenv("QT_ANDROID_FONT_LOCATION");
        if (!ba.isEmpty())
            return QStringList((fontLocation = QDir::cleanPath(QString::fromLocal8Bit(ba))));

        return QStringList((fontLocation = QLatin1String("/system/fonts")));
    }

    return QStringList(writableLocation(type));
}

QT_END_NAMESPACE

#endif // QT_NO_STANDARDPATHS
