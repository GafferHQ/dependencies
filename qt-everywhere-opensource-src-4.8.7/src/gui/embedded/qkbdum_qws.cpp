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

#include "qkbdum_qws.h"

#if !defined(QT_NO_QWS_KEYBOARD) && !defined(QT_NO_QWS_KBD_UM)

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <qstring.h>
#include <qwindowsystem_qws.h>
#include <qsocketnotifier.h>
#include "qplatformdefs.h"
#include "qvfbhdr.h"

QT_BEGIN_NAMESPACE

class QWSUmKeyboardHandlerPrivate : public QObject
{
    Q_OBJECT

public:
    QWSUmKeyboardHandlerPrivate(const QString&);
    ~QWSUmKeyboardHandlerPrivate();

private slots:
    void readKeyboardData();

private:
    int kbdFD;
    int kbdIdx;
    const int kbdBufferLen;
    unsigned char *kbdBuffer;
    QSocketNotifier *notifier;
};

QWSUmKeyboardHandlerPrivate::QWSUmKeyboardHandlerPrivate(const QString &device)
    : kbdFD(-1), kbdIdx(0), kbdBufferLen(sizeof(QVFbKeyData)*5)
{
    kbdBuffer = new unsigned char [kbdBufferLen];

    if ((kbdFD = QT_OPEN((const char *)device.toLocal8Bit(), O_RDONLY | O_NDELAY, 0)) < 0) {
        qDebug("Cannot open %s (%s)", (const char *)device.toLocal8Bit(),
        strerror(errno));
    } else {
        // Clear pending input
        char buf[2];
        while (QT_READ(kbdFD, buf, 1) > 0) { }

        notifier = new QSocketNotifier(kbdFD, QSocketNotifier::Read, this);
        connect(notifier, SIGNAL(activated(int)),this, SLOT(readKeyboardData()));
    }
}

QWSUmKeyboardHandlerPrivate::~QWSUmKeyboardHandlerPrivate()
{
    if (kbdFD >= 0)
        QT_CLOSE(kbdFD);
    delete [] kbdBuffer;
}


void QWSUmKeyboardHandlerPrivate::readKeyboardData()
{
    int n;
    do {
        n  = QT_READ(kbdFD, kbdBuffer+kbdIdx, kbdBufferLen - kbdIdx);
        if (n > 0)
            kbdIdx += n;
    } while (n > 0);

    int idx = 0;
    while (kbdIdx - idx >= (int)sizeof(QVFbKeyData)) {
        QVFbKeyData *kd = (QVFbKeyData *)(kbdBuffer + idx);
        // Qtopia Key filters must still work.
        QWSServer::processKeyEvent(kd->unicode, kd->keycode, kd->modifiers, kd->press, kd->repeat);
        idx += sizeof(QVFbKeyData);
    }

    int surplus = kbdIdx - idx;
    for (int i = 0; i < surplus; i++)
        kbdBuffer[i] = kbdBuffer[idx+i];
    kbdIdx = surplus;
}

QWSUmKeyboardHandler::QWSUmKeyboardHandler(const QString &device)
    : QWSKeyboardHandler()
{
    d = new QWSUmKeyboardHandlerPrivate(device);
}

QWSUmKeyboardHandler::~QWSUmKeyboardHandler()
{
    delete d;
}

QT_END_NAMESPACE

#include "qkbdum_qws.moc"

#endif // QT_NO_QWS_KEYBOARD && QT_NO_QWS_KBD_UM
