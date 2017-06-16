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

#include "qfinalstate_p.h"

#ifndef QT_NO_STATEMACHINE

QT_BEGIN_NAMESPACE

/*!
  \class QFinalState
  \inmodule QtCore

  \brief The QFinalState class provides a final state.

  \since 4.6
  \ingroup statemachine

  A final state is used to communicate that (part of) a QStateMachine has
  finished its work. When a final top-level state is entered, the state
  machine's \l{QStateMachine::finished()}{finished}() signal is emitted. In
  general, when a final substate (a child of a QState) is entered, the parent
  state's \l{QState::finished()}{finished}() signal is emitted.  QFinalState
  is part of \l{The State Machine Framework}.

  To use a final state, you create a QFinalState object and add a transition
  to it from another state. Example:

  \code
  QPushButton button;

  QStateMachine machine;
  QState *s1 = new QState();
  QFinalState *s2 = new QFinalState();
  s1->addTransition(&button, SIGNAL(clicked()), s2);
  machine.addState(s1);
  machine.addState(s2);

  QObject::connect(&machine, SIGNAL(finished()), QApplication::instance(), SLOT(quit()));
  machine.setInitialState(s1);
  machine.start();
  \endcode

  \sa QState::finished()
*/

QFinalStatePrivate::QFinalStatePrivate()
    : QAbstractStatePrivate(FinalState)
{
}

QFinalStatePrivate::~QFinalStatePrivate()
{
    // to prevent vtables being generated in every file that includes the private header
}

/*!
  Constructs a new QFinalState object with the given \a parent state.
*/
QFinalState::QFinalState(QState *parent)
    : QAbstractState(*new QFinalStatePrivate, parent)
{
}

/*!
  \internal
 */
QFinalState::QFinalState(QFinalStatePrivate &dd, QState *parent)
    : QAbstractState(dd, parent)
{
}


/*!
  Destroys this final state.
*/
QFinalState::~QFinalState()
{
}

/*!
  \reimp
*/
void QFinalState::onEntry(QEvent *event)
{
    Q_UNUSED(event);
}

/*!
  \reimp
*/
void QFinalState::onExit(QEvent *event)
{
    Q_UNUSED(event);
}

/*!
  \reimp
*/
bool QFinalState::event(QEvent *e)
{
    return QAbstractState::event(e);
}

QT_END_NAMESPACE

#endif //QT_NO_STATEMACHINE
