/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QT_NO_QWS_MOUSE_QVFB

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include <qvfbhdr.h>
#include <qmousevfb_qws.h>
#include <qwindowsystem_qws.h>
#include <qsocketnotifier.h>
#include <qapplication.h>
#include <qtimer.h>
#include <private/qcore_unix_p.h> // overrides QT_OPEN

QT_BEGIN_NAMESPACE

QVFbMouseHandler::QVFbMouseHandler(const QString &driver, const QString &device)
    : QObject(), QWSMouseHandler(driver, device)
{
    QString mouseDev = device;
    if (device.isEmpty())
        mouseDev = QLatin1String("/dev/vmouse");

    mouseFD = QT_OPEN(mouseDev.toLatin1().constData(), O_RDWR | O_NDELAY);
    if (mouseFD == -1) {
        perror("QVFbMouseHandler::QVFbMouseHandler");
        qWarning("QVFbMouseHander: Unable to open device %s",
                 qPrintable(mouseDev));
        return;
    }

    // Clear pending input
    char buf[2];
    while (QT_READ(mouseFD, buf, 1) > 0) { }

    mouseIdx = 0;

    mouseNotifier = new QSocketNotifier(mouseFD, QSocketNotifier::Read, this);
    connect(mouseNotifier, SIGNAL(activated(int)),this, SLOT(readMouseData()));
}

QVFbMouseHandler::~QVFbMouseHandler()
{
    if (mouseFD >= 0)
        QT_CLOSE(mouseFD);
}

void QVFbMouseHandler::resume()
{
    mouseNotifier->setEnabled(true);
}

void QVFbMouseHandler::suspend()
{
    mouseNotifier->setEnabled(false);
}

void QVFbMouseHandler::readMouseData()
{
    int n;
    do {
        n = QT_READ(mouseFD, mouseBuf+mouseIdx, mouseBufSize-mouseIdx);
        if (n > 0)
            mouseIdx += n;
    } while (n > 0);

    int idx = 0;
    static const int packetsize = sizeof(QPoint) + 2*sizeof(int);
    while (mouseIdx-idx >= packetsize) {
        uchar *mb = mouseBuf+idx;
        QPoint mousePos = *reinterpret_cast<QPoint *>(mb);
        mb += sizeof(QPoint);
        int bstate = *reinterpret_cast<int *>(mb);
        mb += sizeof(int);
        int wheel = *reinterpret_cast<int *>(mb);
//        limitToScreen(mousePos);
        mouseChanged(mousePos, bstate, wheel);
        idx += packetsize;
    }

    int surplus = mouseIdx - idx;
    for (int i = 0; i < surplus; i++)
        mouseBuf[i] = mouseBuf[idx+i];
    mouseIdx = surplus;
}

QT_END_NAMESPACE

#endif // QT_NO_QWS_MOUSE_QVFB
