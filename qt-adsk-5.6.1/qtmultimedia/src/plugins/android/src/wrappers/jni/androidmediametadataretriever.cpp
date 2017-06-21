/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "androidmediametadataretriever.h"

#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/private/qjni_p.h>
#include <QtCore/QUrl>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

static bool exceptionCheckAndClear(JNIEnv *env)
{
    if (Q_UNLIKELY(env->ExceptionCheck())) {
#ifdef QT_DEBUG
        env->ExceptionDescribe();
#endif // QT_DEBUG
        env->ExceptionClear();
        return true;
    }

    return false;
}

AndroidMediaMetadataRetriever::AndroidMediaMetadataRetriever()
{
    m_metadataRetriever = QJNIObjectPrivate("android/media/MediaMetadataRetriever");
}

AndroidMediaMetadataRetriever::~AndroidMediaMetadataRetriever()
{
    release();
}

QString AndroidMediaMetadataRetriever::extractMetadata(MetadataKey key)
{
    QString value;

    QJNIObjectPrivate metadata = m_metadataRetriever.callObjectMethod("extractMetadata",
                                                                      "(I)Ljava/lang/String;",
                                                                      jint(key));
    if (metadata.isValid())
        value = metadata.toString();

    return value;
}

void AndroidMediaMetadataRetriever::release()
{
    if (!m_metadataRetriever.isValid())
        return;

    m_metadataRetriever.callMethod<void>("release");
}

bool AndroidMediaMetadataRetriever::setDataSource(const QUrl &url)
{
    if (!m_metadataRetriever.isValid())
        return false;

    QJNIEnvironmentPrivate env;

    if (url.isLocalFile()) { // also includes qrc files (copied to a temp file by QMediaPlayer)
        QJNIObjectPrivate string = QJNIObjectPrivate::fromString(url.path());
        QJNIObjectPrivate fileInputStream("java/io/FileInputStream",
                                          "(Ljava/lang/String;)V",
                                          string.object());

        if (exceptionCheckAndClear(env))
            return false;

        QJNIObjectPrivate fd = fileInputStream.callObjectMethod("getFD",
                                                                "()Ljava/io/FileDescriptor;");
        if (exceptionCheckAndClear(env)) {
            fileInputStream.callMethod<void>("close");
            exceptionCheckAndClear(env);
            return false;
        }

        m_metadataRetriever.callMethod<void>("setDataSource",
                                             "(Ljava/io/FileDescriptor;)V",
                                             fd.object());

        bool ok = !exceptionCheckAndClear(env);

        fileInputStream.callMethod<void>("close");
        exceptionCheckAndClear(env);

        if (!ok)
            return false;
    } else if (url.scheme() == QLatin1String("assets")) {
        QJNIObjectPrivate string = QJNIObjectPrivate::fromString(url.path().mid(1)); // remove first '/'
        QJNIObjectPrivate activity(QtAndroidPrivate::activity());
        QJNIObjectPrivate assetManager = activity.callObjectMethod("getAssets",
                                                                   "()Landroid/content/res/AssetManager;");
        QJNIObjectPrivate assetFd = assetManager.callObjectMethod("openFd",
                                                                  "(Ljava/lang/String;)Landroid/content/res/AssetFileDescriptor;",
                                                                  string.object());
        if (exceptionCheckAndClear(env))
            return false;

        QJNIObjectPrivate fd = assetFd.callObjectMethod("getFileDescriptor",
                                                        "()Ljava/io/FileDescriptor;");
        if (exceptionCheckAndClear(env)) {
            assetFd.callMethod<void>("close");
            exceptionCheckAndClear(env);
            return false;
        }

        m_metadataRetriever.callMethod<void>("setDataSource",
                                             "(Ljava/io/FileDescriptor;JJ)V",
                                             fd.object(),
                                             assetFd.callMethod<jlong>("getStartOffset"),
                                             assetFd.callMethod<jlong>("getLength"));

        bool ok = !exceptionCheckAndClear(env);

        assetFd.callMethod<void>("close");
        exceptionCheckAndClear(env);

        if (!ok)
            return false;
    } else if (QtAndroidPrivate::androidSdkVersion() >= 14) {
        // On API levels >= 14, only setDataSource(String, Map<String, String>) accepts remote media
        QJNIObjectPrivate string = QJNIObjectPrivate::fromString(url.toString(QUrl::FullyEncoded));
        QJNIObjectPrivate hash("java/util/HashMap");

        m_metadataRetriever.callMethod<void>("setDataSource",
                                             "(Ljava/lang/String;Ljava/util/Map;)V",
                                             string.object(),
                                             hash.object());
        if (exceptionCheckAndClear(env))
            return false;
    } else {
        // While on API levels < 14, only setDataSource(Context, Uri) is available and works for
        // remote media...
        QJNIObjectPrivate string = QJNIObjectPrivate::fromString(url.toString(QUrl::FullyEncoded));
        QJNIObjectPrivate uri = m_metadataRetriever.callStaticObjectMethod("android/net/Uri",
                                                                           "parse",
                                                                           "(Ljava/lang/String;)Landroid/net/Uri;",
                                                                           string.object());
        if (exceptionCheckAndClear(env))
            return false;

        m_metadataRetriever.callMethod<void>("setDataSource",
                                             "(Landroid/content/Context;Landroid/net/Uri;)V",
                                             QtAndroidPrivate::activity(),
                                             uri.object());
        if (exceptionCheckAndClear(env))
            return false;
    }

    return true;
}

QT_END_NAMESPACE
