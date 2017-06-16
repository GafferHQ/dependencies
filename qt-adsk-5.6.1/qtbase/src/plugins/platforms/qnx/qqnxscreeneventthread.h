/***************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QQNXSCREENEVENTTHREAD_H
#define QQNXSCREENEVENTTHREAD_H

#include <QtCore/QThread>
#include <QtCore/QMutex>

#include <screen/screen.h>

QT_BEGIN_NAMESPACE

class QQnxScreenEventHandler;

typedef QVarLengthArray<screen_event_t, 64> QQnxScreenEventArray;

class QQnxScreenEventThread : public QThread
{
    Q_OBJECT

public:
    QQnxScreenEventThread(screen_context_t context, QQnxScreenEventHandler *screenEventHandler);
    ~QQnxScreenEventThread();

    static void injectKeyboardEvent(int flags, int sym, int mod, int scan, int cap);

    QQnxScreenEventArray *lock();
    void unlock();

protected:
    void run();

Q_SIGNALS:
    void eventPending();

private:
    void shutdown();

    screen_context_t m_screenContext;
    QMutex m_mutex;
    QQnxScreenEventArray m_events;
    QQnxScreenEventHandler *m_screenEventHandler;
    bool m_quit;
};

QT_END_NAMESPACE

#endif // QQNXSCREENEVENTTHREAD_H
