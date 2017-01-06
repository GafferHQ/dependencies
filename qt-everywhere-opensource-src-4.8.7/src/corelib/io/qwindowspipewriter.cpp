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

#include "qwindowspipewriter_p.h"
#include <string.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_THREAD

QWindowsPipeWriter::QWindowsPipeWriter(HANDLE pipe, QObject * parent)
    : QThread(parent),
      writePipe(INVALID_HANDLE_VALUE),
      quitNow(false),
      hasWritten(false)
{
#if !defined(Q_OS_WINCE) || (_WIN32_WCE >= 0x600)
    DuplicateHandle(GetCurrentProcess(), pipe, GetCurrentProcess(),
                         &writePipe, 0, FALSE, DUPLICATE_SAME_ACCESS);
#else
    Q_UNUSED(pipe);
    writePipe = GetCurrentProcess();
#endif
}

QWindowsPipeWriter::~QWindowsPipeWriter()
{
    lock.lock();
    quitNow = true;
    waitCondition.wakeOne();
    lock.unlock();
    if (!wait(30000))
        terminate();
#if !defined(Q_OS_WINCE) || (_WIN32_WCE >= 0x600)
    CloseHandle(writePipe);
#endif
}

bool QWindowsPipeWriter::waitForWrite(int msecs)
{
    QMutexLocker locker(&lock);
    bool hadWritten = hasWritten;
    hasWritten = false;
    if (hadWritten)
        return true;
    if (!waitCondition.wait(&lock, msecs))
        return false;
    hadWritten = hasWritten;
    hasWritten = false;
    return hadWritten;
}

qint64 QWindowsPipeWriter::write(const char *ptr, qint64 maxlen)
{
    if (!isRunning())
        return -1;

    QMutexLocker locker(&lock);
    data.append(QByteArray(ptr, maxlen));
    waitCondition.wakeOne();
    return maxlen;
}

void QWindowsPipeWriter::run()
{
    OVERLAPPED overl;
    memset(&overl, 0, sizeof overl);
    overl.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    forever {
        lock.lock();
        while(data.isEmpty() && (!quitNow)) {
            waitCondition.wakeOne();
            waitCondition.wait(&lock);
        }

        if (quitNow) {
            lock.unlock();
            quitNow = false;
	    break;
        }

        QByteArray copy = data;

        lock.unlock();

        const char *ptrData = copy.data();
        qint64 maxlen = copy.size();
        qint64 totalWritten = 0;
        overl.Offset = 0;
        overl.OffsetHigh = 0;
        while ((!quitNow) && totalWritten < maxlen) {
            DWORD written = 0;
            if (!WriteFile(writePipe, ptrData + totalWritten,
                           maxlen - totalWritten, &written, &overl)) {

                if (GetLastError() == 0xE8/*NT_STATUS_INVALID_USER_BUFFER*/) {
                    // give the os a rest
                    msleep(100);
                    continue;
                }
#ifndef Q_OS_WINCE
                if (GetLastError() == ERROR_IO_PENDING) {
                  if (!GetOverlappedResult(writePipe, &overl, &written, TRUE)) {
                      CloseHandle(overl.hEvent);
                      return;
                  }
                } else {
                    CloseHandle(overl.hEvent);
                    return;
                }
#else
                return;
#endif
            }
            totalWritten += written;
#if defined QPIPEWRITER_DEBUG
            qDebug("QWindowsPipeWriter::run() wrote %d %d/%d bytes",
			    written, int(totalWritten), int(maxlen));
#endif
            lock.lock();
            data.remove(0, written);
            hasWritten = true;
            lock.unlock();
        }
        emit bytesWritten(totalWritten);
        emit canWrite();
    }
    CloseHandle(overl.hEvent);
}

#endif //QT_NO_THREAD

QT_END_NAMESPACE
