/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#ifndef BOAT_P_H
#define BOAT_P_H

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
#include "bomb.h"
#include "graphicsscene.h"

// Qt
#include <QtWidgets/QKeyEventTransition>

static const int MAX_BOMB = 5;


//These transtion test if we have to stop the boat (i.e current speed is 1)
class KeyStopTransition : public QKeyEventTransition
{
public:
    KeyStopTransition(Boat *b, QEvent::Type t, int k)
    : QKeyEventTransition(b, t, k), boat(b)
    {
    }
protected:
    virtual bool eventTest(QEvent *event) Q_DECL_OVERRIDE
    {
        if (!QKeyEventTransition::eventTest(event))
            return false;
        return (boat->currentSpeed() == 1);
    }
private:
    Boat * boat;
};

//These transtion test if we have to move the boat (i.e current speed was 0 or another value)
 class KeyMoveTransition : public QKeyEventTransition
{
public:
    KeyMoveTransition(Boat *b, QEvent::Type t, int k)
    : QKeyEventTransition(b, t, k), boat(b), key(k)
    {
    }
protected:
    virtual bool eventTest(QEvent *event) Q_DECL_OVERRIDE
    {
        if (!QKeyEventTransition::eventTest(event))
            return false;
        return (boat->currentSpeed() >= 0);
    }
    void onTransition(QEvent *) Q_DECL_OVERRIDE
    {
        //We decrease the speed if needed
        if (key == Qt::Key_Left && boat->currentDirection() == Boat::Right)
            boat->setCurrentSpeed(boat->currentSpeed() - 1);
        else if (key == Qt::Key_Right && boat->currentDirection() == Boat::Left)
            boat->setCurrentSpeed(boat->currentSpeed() - 1);
        else if (boat->currentSpeed() < 3)
            boat->setCurrentSpeed(boat->currentSpeed() + 1);
        boat->updateBoatMovement();
    }
private:
    Boat * boat;
    int key;
};

//This transition trigger the bombs launch
 class KeyLaunchTransition : public QKeyEventTransition
{
public:
    KeyLaunchTransition(Boat *boat, QEvent::Type type, int key)
    : QKeyEventTransition(boat, type, key), boat(boat)
    {
    }
protected:
    virtual bool eventTest(QEvent *event) Q_DECL_OVERRIDE
    {
        if (!QKeyEventTransition::eventTest(event))
            return false;
        //We have enough bomb?
        return (boat->bombsLaunched() < MAX_BOMB);
    }
private:
    Boat * boat;
};

//This state is describing when the boat is moving right
class MoveStateRight : public QState
{
public:
    explicit MoveStateRight(Boat *boat,QState *parent = 0) : QState(parent), boat(boat)
    {
    }
protected:
    void onEntry(QEvent *) Q_DECL_OVERRIDE
    {
        boat->setCurrentDirection(Boat::Right);
        boat->updateBoatMovement();
    }
private:
    Boat * boat;
};

 //This state is describing when the boat is moving left
class MoveStateLeft : public QState
{
public:
    explicit MoveStateLeft(Boat *boat,QState *parent = 0) : QState(parent), boat(boat)
    {
    }
protected:
    void onEntry(QEvent *) Q_DECL_OVERRIDE
    {
        boat->setCurrentDirection(Boat::Left);
        boat->updateBoatMovement();
    }
private:
    Boat * boat;
};

//This state is describing when the boat is in a stand by position
class StopState : public QState
{
public:
    explicit StopState(Boat *boat,QState *parent = 0) : QState(parent), boat(boat)
    {
    }
protected:
    void onEntry(QEvent *) Q_DECL_OVERRIDE
    {
        boat->setCurrentSpeed(0);
        boat->setCurrentDirection(Boat::None);
        boat->updateBoatMovement();
    }
private:
    Boat * boat;
};

//This state is describing the launch of the torpedo on the right
class LaunchStateRight : public QState
{
public:
    explicit LaunchStateRight(Boat *boat,QState *parent = 0) : QState(parent), boat(boat)
    {
    }
protected:
    void onEntry(QEvent *) Q_DECL_OVERRIDE
    {
        Bomb *b = new Bomb();
        b->setPos(boat->x()+boat->size().width(),boat->y());
        GraphicsScene *scene = static_cast<GraphicsScene *>(boat->scene());
        scene->addItem(b);
        b->launch(Bomb::Right);
        boat->setBombsLaunched(boat->bombsLaunched() + 1);
    }
private:
    Boat * boat;
};

//This state is describing the launch of the torpedo on the left
class LaunchStateLeft : public QState
{
public:
    explicit LaunchStateLeft(Boat *boat,QState *parent = 0) : QState(parent), boat(boat)
    {
    }
protected:
    void onEntry(QEvent *) Q_DECL_OVERRIDE
    {
        Bomb *b = new Bomb();
        b->setPos(boat->x() - b->size().width(), boat->y());
        GraphicsScene *scene = static_cast<GraphicsScene *>(boat->scene());
        scene->addItem(b);
        b->launch(Bomb::Left);
        boat->setBombsLaunched(boat->bombsLaunched() + 1);
    }
private:
    Boat * boat;
};

#endif // BOAT_P_H
