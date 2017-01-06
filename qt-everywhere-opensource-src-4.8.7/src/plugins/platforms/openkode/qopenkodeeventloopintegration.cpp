/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qopenkodeeventloopintegration.h"

#include <QDebug>

#include <KD/kd.h>
#include <KD/ATX_keyboard.h>

QT_BEGIN_NAMESPACE

static const int QT_EVENT_WAKEUP_EVENTLOOP = KD_EVENT_USER + 1;

void kdprocessevent( const KDEvent *event)
{
    switch (event->type) {
    case KD_EVENT_INPUT:
        qDebug() << "KD_EVENT_INPUT";
        break;
    case KD_EVENT_INPUT_POINTER:
        qDebug() << "KD_EVENT_INPUT_POINTER";
        break;
    case KD_EVENT_WINDOW_CLOSE:
        qDebug() << "KD_EVENT_WINDOW_CLOSE";
        break;
    case KD_EVENT_WINDOWPROPERTY_CHANGE:
        qDebug() << "KD_EVENT_WINDOWPROPERTY_CHANGE";
        qDebug() << event->data.windowproperty.pname;
        break;
    case KD_EVENT_WINDOW_FOCUS:
        qDebug() << "KD_EVENT_WINDOW_FOCUS";
        break;
    case KD_EVENT_WINDOW_REDRAW:
        qDebug() << "KD_EVENT_WINDOW_REDRAW";
        break;
    case KD_EVENT_USER:
        qDebug() << "KD_EVENT_USER";
        break;
    case KD_EVENT_INPUT_KEY_ATX:
        qDebug() << "KD_EVENT_INPUT_KEY_ATX";
        break;
    case QT_EVENT_WAKEUP_EVENTLOOP:
        QPlatformEventLoopIntegration::processEvents();
        break;
    default:
        break;
    }

    kdDefaultEvent(event);

}

QOpenKODEEventLoopIntegration::QOpenKODEEventLoopIntegration()
    : m_quit(false)
{
    m_kdThread = kdThreadSelf();
    kdInstallCallback(&kdprocessevent,QT_EVENT_WAKEUP_EVENTLOOP,this);
}

void QOpenKODEEventLoopIntegration::startEventLoop()
{

    while(!m_quit) {
        qint64 msec = nextTimerEvent();
        const KDEvent *event = kdWaitEvent(msec);
        if (event) {
            kdDefaultEvent(event);
            while ((event = kdWaitEvent(0)) != 0) {
                kdDefaultEvent(event);
            }
        }
        QPlatformEventLoopIntegration::processEvents();
    }
}

void QOpenKODEEventLoopIntegration::quitEventLoop()
{
    m_quit = true;
}

void QOpenKODEEventLoopIntegration::qtNeedsToProcessEvents()
{
    KDEvent *event = kdCreateEvent();
    event->type = QT_EVENT_WAKEUP_EVENTLOOP;
    event->userptr = this;
    kdPostThreadEvent(event,m_kdThread);
}

QT_END_NAMESPACE
