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

#include <qglobal.h>
#include "qsystemerror_p.h"
#if !defined(Q_OS_WINCE)
#  include <errno.h>
#  if defined(Q_CC_MSVC)
#    include <crtdbg.h>
#  endif
#else
#  if (_WIN32_WCE >= 0x700)
#    include <errno.h>
#  endif
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

QT_BEGIN_NAMESPACE

#if !defined(Q_OS_WIN) && !defined(QT_NO_THREAD) && !defined(Q_OS_INTEGRITY) && !defined(Q_OS_QNX) && \
    defined(_POSIX_THREAD_SAFE_FUNCTIONS) && _POSIX_VERSION >= 200112L
namespace {
    // There are two incompatible versions of strerror_r:
    // a) the XSI/POSIX.1 version, which returns an int,
    //    indicating success or not
    // b) the GNU version, which returns a char*, which may or may not
    //    be the beginning of the buffer we used
    // The GNU libc manpage for strerror_r says you should use the the XSI
    // version in portable code. However, it's impossible to do that if
    // _GNU_SOURCE is defined so we use C++ overloading to decide what to do
    // depending on the return type
    static inline QString fromstrerror_helper(int, const QByteArray &buf)
    {
        return QString::fromLocal8Bit(buf);
    }
    static inline QString fromstrerror_helper(const char *str, const QByteArray &)
    {
        return QString::fromLocal8Bit(str);
    }
}
#endif

#ifdef Q_OS_WIN
static QString windowsErrorString(int errorCode)
{
    QString ret;
    wchar_t *string = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  errorCode,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPWSTR)&string,
                  0,
                  NULL);
    ret = QString::fromWCharArray(string);
    LocalFree((HLOCAL)string);

    if (ret.isEmpty() && errorCode == ERROR_MOD_NOT_FOUND)
        ret = QString::fromLatin1("The specified module could not be found.");
    return ret;
}
#endif

static QString standardLibraryErrorString(int errorCode)
{
    const char *s = 0;
    QString ret;
    switch (errorCode) {
    case 0:
        break;
    case EACCES:
        s = QT_TRANSLATE_NOOP("QIODevice", "Permission denied");
        break;
    case EMFILE:
        s = QT_TRANSLATE_NOOP("QIODevice", "Too many open files");
        break;
    case ENOENT:
        s = QT_TRANSLATE_NOOP("QIODevice", "No such file or directory");
        break;
    case ENOSPC:
        s = QT_TRANSLATE_NOOP("QIODevice", "No space left on device");
        break;
    default: {
    #ifdef Q_OS_WINCE
        ret = windowsErrorString(errorCode);
    #else
        #if !defined(QT_NO_THREAD) && defined(_POSIX_THREAD_SAFE_FUNCTIONS) && _POSIX_VERSION >= 200112L && !defined(Q_OS_INTEGRITY) && !defined(Q_OS_QNX)
            QByteArray buf(1024, '\0');
            ret = fromstrerror_helper(strerror_r(errorCode, buf.data(), buf.size()), buf);
        #else
            ret = QString::fromLocal8Bit(strerror(errorCode));
        #endif
    #endif
    break; }
    }
    if (s) {
        // ######## this breaks moc build currently
        // ret = QCoreApplication::translate("QIODevice", s);
        ret = QString::fromLatin1(s);
    }
    return ret.trimmed();
}

#ifdef Q_OS_SYMBIAN
static QString symbianErrorString(int errorCode)
{
    switch (errorCode) {
    case KErrNotFound:
        return QLatin1String("not found");
    case KErrCancel:
        return QLatin1String("cancelled");
    case KErrNoMemory:
        return QLatin1String("out of memory");
    case KErrNotSupported:
        return QLatin1String("not supported");
    case KErrBadHandle:
        return QLatin1String("bad handle"); //KERN-EXEC 0 panic is more likely
    case KErrAlreadyExists:
        return QLatin1String("already exists");
    case KErrPathNotFound:
        return QLatin1String("path not found");
    case KErrInUse:
        return QLatin1String("in use");
    case KErrNotReady:
        return QLatin1String("not ready (e.g. FS dismounted, network down)");
    case KErrCorrupt:
        return QLatin1String("corrupt");
    case KErrAccessDenied:
        return QLatin1String("access denied");
    case KErrLocked:
        return QLatin1String("locked");
    case KErrWrite:
        return QLatin1String("incomplete write error");
    case KErrDisMounted:
        return QLatin1String("file system dismounted during operation"); //i.e. a forcible dismount was done while we had files open
    case KErrEof:
        return QLatin1String("end of file");
    case KErrDiskFull:
        return QLatin1String("no space in file system");
    case KErrBadName:
        return QLatin1String("invalid filename");
    case KErrTimedOut:
        return QLatin1String("timed out");
    case KErrBadDescriptor:
        return QLatin1String("bad descriptor (passed address on stack to async call?)");
    case KErrAbort:
        return QLatin1String("aborted");
    case KErrTooBig:
        return QLatin1String("too big"); //e.g. trying to open a >2GB file with 32 bit API
    case KErrBadPower:
        return QLatin1String("insufficient power");
    case KErrDirFull:
        return QLatin1String("no space in directory table");
    case KErrHardwareNotAvailable:
        return QLatin1String("hardware not available");
    case KErrSessionClosed:
        return QLatin1String("session closed");
    case KErrPermissionDenied:
        return QLatin1String("permission denied");
    default:
        return QString(QLatin1String("symbian error %1")).arg(errorCode);
    }    
}
#endif

QString QSystemError::toString()
{
    switch(errorScope) {
    case NativeError:
#if defined (Q_OS_WIN)
        return windowsErrorString(errorCode);
#elif defined (Q_OS_SYMBIAN)
        return symbianErrorString(errorCode);
#else
        //unix: fall through as native and standard library are the same
#endif
    case StandardLibraryError:
        return standardLibraryErrorString(errorCode);
    default:
        qWarning("invalid error scope");
        //fall through
    case NoError:
        return QLatin1String("No error");
    }
}

QT_END_NAMESPACE

