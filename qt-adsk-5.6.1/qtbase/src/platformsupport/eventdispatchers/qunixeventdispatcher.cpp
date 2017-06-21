/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "qplatformdefs.h"
#include "qcoreapplication.h"
#include "qunixeventdispatcher_qpa_p.h"
#include "private/qguiapplication_p.h"

#include <qpa/qwindowsysteminterface.h>
#include <QtCore/QElapsedTimer>
#include <QtCore/QAtomicInt>
#include <QtCore/QSemaphore>

#include <QtCore/QDebug>

#include <errno.h>

QT_BEGIN_NAMESPACE

QT_USE_NAMESPACE


QUnixEventDispatcherQPA::QUnixEventDispatcherQPA(QObject *parent)
    : QEventDispatcherUNIX(parent)
{ }

QUnixEventDispatcherQPA::~QUnixEventDispatcherQPA()
{ }

bool QUnixEventDispatcherQPA::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    const bool didSendEvents = QEventDispatcherUNIX::processEvents(flags);
    return QWindowSystemInterface::sendWindowSystemEvents(flags) || didSendEvents;
}

bool QUnixEventDispatcherQPA::hasPendingEvents()
{
    extern uint qGlobalPostedEventsCount(); // from qapplication.cpp
    return qGlobalPostedEventsCount() || QWindowSystemInterface::windowSystemEventsQueued();
}

void QUnixEventDispatcherQPA::flush()
{
    if(qApp)
        qApp->sendPostedEvents();
}

QT_END_NAMESPACE
