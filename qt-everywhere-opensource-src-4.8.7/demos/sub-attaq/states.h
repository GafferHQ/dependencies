/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef STATES_H
#define STATES_H

//Qt
#include <QtCore/QState>
#include <QtCore/QSignalTransition>
#include <QtCore/QPropertyAnimation>
#include <QtGui/QKeyEventTransition>
#include <QtCore/QSet>

class GraphicsScene;
class Boat;
class SubMarine;
QT_BEGIN_NAMESPACE
class QStateMachine;
QT_END_NAMESPACE

class PlayState : public QState
{
public:
    PlayState(GraphicsScene *scene, QState *parent = 0);
    ~PlayState();

 protected:
    void onEntry(QEvent *);

private :
    GraphicsScene *scene;
    QStateMachine *machine;
    int currentLevel;
    int score;
    QState *parallelChild;

    friend class UpdateScoreState;
    friend class UpdateScoreTransition;
    friend class WinTransition;
    friend class CustomSpaceTransition;
    friend class WinState;
    friend class LostState;
    friend class LevelState;
};

class LevelState : public QState
{
public:
    LevelState(GraphicsScene *scene, PlayState *game, QState *parent = 0);
protected:
    void onEntry(QEvent *);
private :
    void initializeLevel();
    GraphicsScene *scene;
    PlayState *game;
};

class PauseState : public QState
{
public:
    PauseState(GraphicsScene *scene, QState *parent = 0);

protected:
    void onEntry(QEvent *);
    void onExit(QEvent *);
private :
    GraphicsScene *scene;
    Boat *boat;
};

class LostState : public QState
{
public:
    LostState(GraphicsScene *scene, PlayState *game, QState *parent = 0);

protected:
    void onEntry(QEvent *);
    void onExit(QEvent *);
private :
    GraphicsScene *scene;
    PlayState *game;
};

class WinState : public QState
{
public:
    WinState(GraphicsScene *scene, PlayState *game, QState *parent = 0);

protected:
    void onEntry(QEvent *);
    void onExit(QEvent *);
private :
    GraphicsScene *scene;
    PlayState *game;
};

class UpdateScoreState : public QState
{
public:
    UpdateScoreState(PlayState *game, QState *parent);
private:
    QPropertyAnimation *scoreAnimation;
    PlayState *game;
};

//These transtion is used to update the score
class UpdateScoreTransition : public QSignalTransition
{
public:
    UpdateScoreTransition(GraphicsScene *scene, PlayState *game, QAbstractState *target);
protected:
    virtual bool eventTest(QEvent *event);
private:
    PlayState * game;
    GraphicsScene *scene;
};

//These transtion test if we have won the game
class WinTransition : public QSignalTransition
{
public:
    WinTransition(GraphicsScene *scene, PlayState *game, QAbstractState *target);
protected:
    virtual bool eventTest(QEvent *event);
private:
    PlayState * game;
    GraphicsScene *scene;
};

//These transtion is true if one level has been completed and the player want to continue
 class CustomSpaceTransition : public QKeyEventTransition
{
public:
    CustomSpaceTransition(QWidget *widget, PlayState *game, QEvent::Type type, int key);
protected:
    virtual bool eventTest(QEvent *event);
private:
    PlayState *game;
    int key;
};

#endif // STATES_H
