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

#include "qabstractnativeeventfilter.h"
#include "qabstracteventdispatcher.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAbstractNativeEventFilter
    \inmodule QtCore
    \since 5.0

    \brief The QAbstractNativeEventFilter class provides an interface for receiving native
    events, such as MSG or XCB event structs.
*/

/*!
    Creates a native event filter.

    By default this doesn't do anything. Remember to install it on the application
    object.
*/
QAbstractNativeEventFilter::QAbstractNativeEventFilter()
{
    Q_UNUSED(d);
}

/*!
    Destroys the native event filter.

    This automatically removes it from the application.
*/
QAbstractNativeEventFilter::~QAbstractNativeEventFilter()
{
    QAbstractEventDispatcher *eventDispatcher = QAbstractEventDispatcher::instance();
    if (eventDispatcher)
        eventDispatcher->removeNativeEventFilter(this);
}

/*!
    \fn bool QAbstractNativeEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result)

    This method is called for every native event.

    \note The filter function here receives native messages,
    for example, MSG or XCB event structs.

    It is called by the QPA platform plugin. On Windows, it is called by
    the event dispatcher.

    The type of event \a eventType is specific to the platform plugin chosen at run-time,
    and can be used to cast \a message to the right type.

    On X11, \a eventType is set to "xcb_generic_event_t", and the \a message can be casted
    to a xcb_generic_event_t pointer.

    On Windows, \a eventType is set to "windows_generic_MSG" for messages sent to toplevel windows,
    and "windows_dispatcher_MSG" for system-wide messages such as messages from a registered hot key.
    In both cases, the \a message can be casted to a MSG pointer.
    The \a result pointer is only used on Windows, and corresponds to the LRESULT pointer.

    On Mac, \a eventType is set to "mac_generic_NSEvent", and the \a message can be casted to an EventRef.

    On Blackberry (not plain QNX) \a eventType is set to "bps_event_t", and the \a message can be casted
    to a bps_event_t pointer.

    In your reimplementation of this function, if you want to filter
    the \a message out, i.e. stop it being handled further, return
    true; otherwise return false.

    Example:
    \snippet code/src_corelib_kernel_qabstractnativeeventfilter.cpp 0
*/

QT_END_NAMESPACE
