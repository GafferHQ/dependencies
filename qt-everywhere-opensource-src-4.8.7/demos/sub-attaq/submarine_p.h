/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#ifndef SUBMARINE_P_H
#define SUBMARINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

//Own
#include "animationmanager.h"
#include "submarine.h"
#include "qanimationstate.h"

//Qt
#include <QtCore/QPropertyAnimation>
#include <QtGui/QGraphicsScene>

//This state is describing when the boat is moving right
class MovementState : public QAnimationState
{
Q_OBJECT
public:
    MovementState(SubMarine *submarine, QState *parent = 0) : QAnimationState(parent)
    {
        movementAnimation = new QPropertyAnimation(submarine, "pos");
        connect(movementAnimation,SIGNAL(valueChanged(const QVariant &)),this,SLOT(onAnimationMovementValueChanged(const QVariant &)));
        setAnimation(movementAnimation);
        AnimationManager::self()->registerAnimation(movementAnimation);
        this->submarine = submarine;
    }

protected slots:
    void onAnimationMovementValueChanged(const QVariant &)
    {
        if (qrand() % 200 + 1 == 3)
            submarine->launchTorpedo(qrand() % 3 + 1);
    }

protected:
    void onEntry(QEvent *e)
    {
        if (submarine->currentDirection() == SubMarine::Left) {
            movementAnimation->setEndValue(QPointF(0,submarine->y()));
            movementAnimation->setDuration(submarine->x()/submarine->currentSpeed()*12);
        }
        else /*if (submarine->currentDirection() == SubMarine::Right)*/ {
            movementAnimation->setEndValue(QPointF(submarine->scene()->width()-submarine->size().width(),submarine->y()));
            movementAnimation->setDuration((submarine->scene()->width()-submarine->size().width()-submarine->x())/submarine->currentSpeed()*12);
        }
        QAnimationState::onEntry(e);
    }

private:
    SubMarine *submarine;
    QPropertyAnimation *movementAnimation;
};

//This state is describing when the boat is moving right
class ReturnState : public QAnimationState
{
public:
    ReturnState(SubMarine *submarine, QState *parent = 0) : QAnimationState(parent)
    {
        returnAnimation = new QPropertyAnimation(submarine->rotation(), "angle");
        returnAnimation->setDuration(500);
        AnimationManager::self()->registerAnimation(returnAnimation);
        setAnimation(returnAnimation);
        this->submarine = submarine;
    }

protected:
    void onEntry(QEvent *e)
    {
        returnAnimation->stop();
        returnAnimation->setEndValue(submarine->currentDirection() == SubMarine::Right ? 360. : 180.);
        QAnimationState::onEntry(e);
    }

    void onExit(QEvent *e)
    {
        submarine->currentDirection() == SubMarine::Right ? submarine->setCurrentDirection(SubMarine::Left) : submarine->setCurrentDirection(SubMarine::Right);
        QAnimationState::onExit(e);
    }

private:
    SubMarine *submarine;
    QPropertyAnimation *returnAnimation;
};

#endif // SUBMARINE_P_H
