/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "timeouttransition.h"

#include <QQmlInfo>
#include <QTimer>
#include <QState>

TimeoutTransition::TimeoutTransition(QState* parent)
    : QSignalTransition((m_timer = new QTimer), SIGNAL(timeout()), parent)
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(1000);
}

TimeoutTransition::~TimeoutTransition()
{
    delete m_timer;
}

int TimeoutTransition::timeout() const
{
    return m_timer->interval();
}

void TimeoutTransition::setTimeout(int timeout)
{
    if (timeout != m_timer->interval()) {
        m_timer->setInterval(timeout);
        emit timeoutChanged();
    }
}

void TimeoutTransition::componentComplete()
{
    QState *state = qobject_cast<QState*>(parent());
    if (!state) {
        qmlInfo(this) << "Parent needs to be a State";
        return;
    }

    connect(state, SIGNAL(entered()), m_timer, SLOT(start()));
    connect(state, SIGNAL(exited()), m_timer, SLOT(stop()));
    if (state->active())
        m_timer->start();
}

/*!
    \qmltype TimeoutTransition
    \inqmlmodule QtQml.StateMachine
    \inherits QSignalTransition
    \ingroup statemachine-qmltypes
    \since 5.4

    \brief The TimeoutTransition type provides a transition based on a timer.

    \l{Timer} type can be combined with SignalTransition to enact more complex
    timeout based transitions.

    TimeoutTransition is part of \l{The Declarative State Machine Framework}.

    \section1 Example Usage

    \snippet qml/statemachine/timeouttransition.qml document

    \clearfloat

    \sa StateMachine, SignalTransition, FinalState, HistoryState
*/

/*!
    \qmlproperty int TimeoutTransition::timeout

    \brief The timeout interval in milliseconds.
*/
