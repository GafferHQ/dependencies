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

#include "qandroidjnienvironment.h"
#include <QtCore/private/qjni_p.h>
#include <QtCore/private/qjnihelpers_p.h>
#include <QtCore/qthreadstorage.h>

QT_BEGIN_NAMESPACE

/*!
    \class QAndroidJniEnvironment
    \inmodule QtAndroidExtras
    \brief The QAndroidJniEnvironment provides access to the JNI Environment.
    \since 5.2
*/

/*!
    \fn QAndroidJniEnvironment::QAndroidJniEnvironment()

    Constructs a new QAndroidJniEnvironment object and attach the current thread to the Java VM.

    \snippet code/src_androidextras_qandroidjnienvironment.cpp Create QAndroidJniEnvironment
*/

/*!
    \fn QAndroidJniEnvironment::~QAndroidJniEnvironment()

    Detaches the current thread from the Java VM and destroys the QAndroidJniEnvironment object.
*/

/*!
    \fn JavaVM *QAndroidJniEnvironment::javaVM()

    Returns the Java VM interface.
*/

/*!
    \fn JNIEnv *QAndroidJniEnvironment::operator->()

    Provides access to the QAndroidJniEnvironment's JNIEnv pointer.
*/

/*!
    \fn QAndroidJniEnvironment::operator JNIEnv *() const

    Returns the JNI Environment pointer.
 */


QAndroidJniEnvironment::QAndroidJniEnvironment()
    : d(new QJNIEnvironmentPrivate)
{
}

QAndroidJniEnvironment::~QAndroidJniEnvironment()
{
}

JavaVM *QAndroidJniEnvironment::javaVM()
{
    return QtAndroidPrivate::javaVM();
}

JNIEnv *QAndroidJniEnvironment::operator->()
{
    return d->jniEnv;
}

QAndroidJniEnvironment::operator JNIEnv*() const
{
    return d->jniEnv;
}

QT_END_NAMESPACE
