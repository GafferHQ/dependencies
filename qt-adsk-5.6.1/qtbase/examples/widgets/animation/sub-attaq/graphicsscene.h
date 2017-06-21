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

#ifndef __GRAPHICSSCENE__H__
#define __GRAPHICSSCENE__H__

//Qt
#include <QtWidgets/QGraphicsScene>
#include <QtCore/QSet>
#include <QtCore/QState>


class Boat;
class SubMarine;
class Torpedo;
class Bomb;
class PixmapItem;
class ProgressItem;
class TextInformationItem;
QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

class GraphicsScene : public QGraphicsScene
{
Q_OBJECT
public:
    enum Mode {
        Big = 0,
        Small
    };

    struct SubmarineDescription {
        int type;
        int points;
        QString name;
    };

    struct LevelDescription {
        int id;
        QString name;
        QList<QPair<int,int> > submarines;
    };

    GraphicsScene(int x, int y, int width, int height, Mode mode = Big);
    qreal sealLevel() const;
    void setupScene(QAction *newAction, QAction *quitAction);
    void addItem(Bomb *bomb);
    void addItem(Torpedo *torpedo);
    void addItem(SubMarine *submarine);
    void addItem(QGraphicsItem *item);
    void clearScene();

signals:
    void subMarineDestroyed(int);
    void allSubMarineDestroyed(int);

private slots:
    void onBombExecutionFinished();
    void onTorpedoExecutionFinished();
    void onSubMarineExecutionFinished();

private:
    Mode mode;
    ProgressItem *progressItem;
    TextInformationItem *textInformationItem;
    Boat *boat;
    QSet<SubMarine *> submarines;
    QSet<Bomb *> bombs;
    QSet<Torpedo *> torpedos;
    QVector<SubmarineDescription> submarinesData;
    QHash<int, LevelDescription> levelsData;

    friend class PauseState;
    friend class PlayState;
    friend class LevelState;
    friend class LostState;
    friend class WinState;
    friend class WinTransition;
    friend class UpdateScoreTransition;
};

#endif //__GRAPHICSSCENE__H__

