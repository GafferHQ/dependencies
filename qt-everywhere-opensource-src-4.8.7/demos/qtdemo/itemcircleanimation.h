/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#ifndef ITEM_CIRCLE_ANIMATION_H
#define ITEM_CIRCLE_ANIMATION_H

#include <QtCore>
#include <QObject>
#include <QtGui>
#include <QTimeLine>
#include <QList>
#include "demoitem.h"
#include "letteritem.h"
#include "guideline.h"
#include "guidecircle.h"

typedef QList<LetterItem *> LetterList;
class TickerEffect;

class ItemCircleAnimation : public QObject, public DemoItem
{
public:
    ItemCircleAnimation(QGraphicsScene *scene = 0, QGraphicsItem *parent = 0);
    virtual ~ItemCircleAnimation();

    // overridden methods:
    QRectF boundingRect() const;
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = 0);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void wheelEvent(QGraphicsSceneWheelEvent *event);
    void animationStarted(int id = 0);
    void animationStopped(int id = 0);
    void prepare();
    void tick();
    void switchToNextEffect();
    void useGuideQt();
    void useGuideTt();
    void pause(bool on);

    bool tickOnPaint;
    bool paused;
    bool doIntroTransitions;

private:
    void setupLetters();
    void createLetter(char c);
    void setupGuides();
    void useGuide(Guide *guide, int firstLetter, int lastLetter);
    void swapModel();
    void setTickerScale(float s);

    int showCount;
    float scale;
    QPointF mouseMoveLastPosition;
    int letterCount;
    LetterList *letterList;
    Guide *qtGuide1;
    Guide *qtGuide2;
    Guide *qtGuide3;
    Guide *currGuide;
    TickerEffect *effect;
    QTime tickTimer;
};

#endif // ITEM_CIRCLE_ANIMATION_H



