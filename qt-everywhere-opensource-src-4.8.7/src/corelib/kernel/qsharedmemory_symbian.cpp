/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsharedmemory.h"
#include "qsharedmemory_p.h"

#include "qcore_symbian_p.h"
#include <qdebug.h>

#ifndef QT_NO_SHAREDMEMORY

#define QSHAREDMEMORY_DEBUG

QT_BEGIN_NAMESPACE

QSharedMemoryPrivate::QSharedMemoryPrivate()
    : QObjectPrivate(), memory(0), size(0), error(QSharedMemory::NoError),
#ifndef QT_NO_SYSTEMSEMAPHORE
      systemSemaphore(QString()), lockedByMe(false)
#endif
{
}

void QSharedMemoryPrivate::setErrorString(const QString &function, TInt errorCode)
{
    if (errorCode == KErrNone)
        return;

    switch (errorCode) {
    case KErrAlreadyExists:
        error = QSharedMemory::AlreadyExists;
        errorString = QSharedMemory::tr("%1: already exists").arg(function);
    break;
    case KErrNotFound:
        error = QSharedMemory::NotFound;
        errorString = QSharedMemory::tr("%1: doesn't exists").arg(function);
        break;
    case KErrArgument:
        error = QSharedMemory::InvalidSize;
        errorString = QSharedMemory::tr("%1: invalid size").arg(function);
        break;
    case KErrNoMemory:
        error = QSharedMemory::OutOfResources;
        errorString = QSharedMemory::tr("%1: out of resources").arg(function);
        break;
    case KErrPermissionDenied:
        error = QSharedMemory::PermissionDenied;
        errorString = QSharedMemory::tr("%1: permission denied").arg(function);
        break;
    default:
        errorString = QSharedMemory::tr("%1: unknown error %2").arg(function).arg(errorCode);
        error = QSharedMemory::UnknownError;
#if defined QSHAREDMEMORY_DEBUG
        qDebug() << errorString << "key" << key;
#endif
        break;
    }
}

key_t QSharedMemoryPrivate::handle()
{
    // don't allow making handles on empty keys
    if (nativeKey.isEmpty()) {
        error = QSharedMemory::KeyError;
        errorString = QSharedMemory::tr("%1: key is empty").arg(QLatin1String("QSharedMemory::handle"));
        return 0;
    }

    // Not really cost effective to check here if shared memory is attachable, as it requires
    // exactly the same call as attaching, so always assume handle is valid and return failure
    // from attach.
    return 1;
}

void QSharedMemoryPrivate::cleanHandle()
{
    chunk.Close();
}

bool QSharedMemoryPrivate::create(int size)
{
    if (!handle())
        return false;

    TPtrC ptr(qt_QString2TPtrC(nativeKey));

    TInt err = chunk.CreateGlobal(ptr, size, size);

    if (err != KErrNone) {
        setErrorString(QLatin1String("QSharedMemory::create"), err);
        return false;
    }

    // Zero out the created chunk
    Mem::FillZ(chunk.Base(), chunk.Size());

    return true;
}

bool QSharedMemoryPrivate::attach(QSharedMemory::AccessMode /* mode */)
{
    // Grab a pointer to the memory block
    if (!chunk.Handle()) {
        if (!handle())
            return false;

        TPtrC ptr(qt_QString2TPtrC(nativeKey));

        TInt err = KErrNoMemory;

        err = chunk.OpenGlobal(ptr, false);

        if (err != KErrNone) {
            setErrorString(QLatin1String("QSharedMemory::attach"), err);
            return false;
        }
    }

    size = chunk.Size();
    memory = chunk.Base();

    return true;
}

bool QSharedMemoryPrivate::detach()
{
    chunk.Close();

    memory = 0;
    size = 0;

    return true;
}

QT_END_NAMESPACE

#endif //QT_NO_SHAREDMEMORY
