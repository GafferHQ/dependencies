/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDBus module of the Qt Toolkit.
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

#include <QtCore/qglobal.h>
#ifndef QT_BOOTSTRAPPED
#include <QtCore/qlibrary.h>
#endif
#include <QtCore/qmutex.h>

#ifndef QT_NO_DBUS

extern "C" void dbus_shutdown();

QT_BEGIN_NAMESPACE

void (*qdbus_resolve_me(const char *name))();

#if !defined QT_LINKED_LIBDBUS

#ifndef QT_NO_LIBRARY
static QLibrary *qdbus_libdbus = 0;

void qdbus_unloadLibDBus()
{
    if (qdbus_libdbus) {
        if (qEnvironmentVariableIsSet("QDBUS_FORCE_SHUTDOWN"))
            qdbus_libdbus->resolve("dbus_shutdown")();
        qdbus_libdbus->unload();
    }
    delete qdbus_libdbus;
    qdbus_libdbus = 0;
}
#endif

bool qdbus_loadLibDBus()
{
#ifndef QT_NO_LIBRARY
#ifdef QT_BUILD_INTERNAL
    // this is to simulate a library load failure for our autotest suite.
    if (!qEnvironmentVariableIsEmpty("QT_SIMULATE_DBUS_LIBFAIL"))
        return false;
#endif

    static bool triedToLoadLibrary = false;
#ifndef QT_NO_THREAD
    static QBasicMutex mutex;
    QMutexLocker locker(&mutex);
#endif

    QLibrary *&lib = qdbus_libdbus;
    if (triedToLoadLibrary)
        return lib && lib->isLoaded();

    lib = new QLibrary;
    lib->setLoadHints(QLibrary::ExportExternalSymbolsHint); // make libdbus symbols available for apps that need more advanced control over the dbus
    triedToLoadLibrary = true;

    static int majorversions[] = { 3, 2, -1 };
    const QString baseNames[] = {
#ifdef Q_OS_WIN
        QLatin1String("dbus-1"),
#endif
        QLatin1String("libdbus-1")
    };

    lib->unload();
    for (uint i = 0; i < sizeof(majorversions) / sizeof(majorversions[0]); ++i) {
        for (uint j = 0; j < sizeof(baseNames) / sizeof(baseNames[0]); ++j) {
#ifdef Q_OS_WIN
            QString suffix;
            if (majorversions[i] != -1)
                suffix = QString::number(- majorversions[i]); // negative so it prepends the dash
            lib->setFileName(baseNames[j] + suffix);
#else
            lib->setFileNameAndVersion(baseNames[j], majorversions[i]);
#endif
            if (lib->load() && lib->resolve("dbus_connection_open_private"))
                return true;

            lib->unload();
        }
    }

    delete lib;
    lib = 0;
    return false;
#else
    return true;
#endif
}

#ifndef QT_NO_LIBRARY
void (*qdbus_resolve_conditionally(const char *name))()
{
    if (qdbus_loadLibDBus())
        return qdbus_libdbus->resolve(name);
    return 0;
}
#endif

void (*qdbus_resolve_me(const char *name))()
{
#ifndef QT_NO_LIBRARY
    if (!qdbus_loadLibDBus())
        qFatal("Cannot find libdbus-1 in your system to resolve symbol '%s'.", name);

    QFunctionPointer ptr = qdbus_libdbus->resolve(name);
    if (!ptr)
        qFatal("Cannot resolve '%s' in your libdbus-1.", name);

    return ptr;
#else
    Q_UNUSED(name);
    return 0;
#endif
}

#else
static void qdbus_unloadLibDBus()
{
    if (qEnvironmentVariableIsSet("QDBUS_FORCE_SHUTDOWN"))
        dbus_shutdown();
}

#endif // !QT_LINKED_LIBDBUS

#if defined(QT_LINKED_LIBDBUS) || !defined(QT_NO_LIBRARY)
Q_DESTRUCTOR_FUNCTION(qdbus_unloadLibDBus)
#endif

QT_END_NAMESPACE

#endif // QT_NO_DBUS
