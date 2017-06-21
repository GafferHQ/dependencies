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

#include <jni.h>
#include "qjnihelpers_p.h"

Q_CORE_EXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    Q_UNUSED(reserved)

    typedef union {
        JNIEnv *nenv;
        void *venv;
    } _JNIEnv;

    _JNIEnv uenv;
    uenv.venv = Q_NULLPTR;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    JNIEnv *env = uenv.nenv;
    const jint ret = QT_PREPEND_NAMESPACE(QtAndroidPrivate::initJNI(vm, env));
    if (ret != 0)
        return ret;

    return JNI_VERSION_1_6;
}
