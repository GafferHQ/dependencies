/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "qdeclarativegraphicswidget_p.h"
#include "private/qdeclarativeanchors_p.h"
#include "private/qdeclarativeitem_p.h"
#include "private/qdeclarativeanchors_p_p.h"

QT_BEGIN_NAMESPACE

class QDeclarativeGraphicsWidgetPrivate : public QObjectPrivate {
    Q_DECLARE_PUBLIC(QDeclarativeGraphicsWidget)
public :
    QDeclarativeGraphicsWidgetPrivate() :
        _anchors(0), _anchorLines(0)
    {}
    QDeclarativeItemPrivate::AnchorLines *anchorLines() const;
    QDeclarativeAnchors *_anchors;
    mutable QDeclarativeItemPrivate::AnchorLines *_anchorLines;
};

QDeclarativeGraphicsWidget::QDeclarativeGraphicsWidget(QObject *parent) :
    QObject(*new QDeclarativeGraphicsWidgetPrivate, parent)
{
}
QDeclarativeGraphicsWidget::~QDeclarativeGraphicsWidget()
{
    Q_D(QDeclarativeGraphicsWidget);
    delete d->_anchorLines; d->_anchorLines = 0;
    delete d->_anchors; d->_anchors = 0;
}

QDeclarativeAnchors *QDeclarativeGraphicsWidget::anchors()
{
    Q_D(QDeclarativeGraphicsWidget);
    if (!d->_anchors)
        d->_anchors = new QDeclarativeAnchors(static_cast<QGraphicsObject *>(parent()));
    return d->_anchors;
}

QDeclarativeItemPrivate::AnchorLines *QDeclarativeGraphicsWidgetPrivate::anchorLines() const
{
    Q_Q(const QDeclarativeGraphicsWidget);
    if (!_anchorLines)
        _anchorLines = new QDeclarativeItemPrivate::AnchorLines(static_cast<QGraphicsObject *>(q->parent()));
    return _anchorLines;
}

QDeclarativeAnchorLine QDeclarativeGraphicsWidget::left() const
{
    Q_D(const QDeclarativeGraphicsWidget);
    return d->anchorLines()->left;
}

QDeclarativeAnchorLine QDeclarativeGraphicsWidget::right() const
{
    Q_D(const QDeclarativeGraphicsWidget);
    return d->anchorLines()->right;
}

QDeclarativeAnchorLine QDeclarativeGraphicsWidget::horizontalCenter() const
{
    Q_D(const QDeclarativeGraphicsWidget);
    return d->anchorLines()->hCenter;
}

QDeclarativeAnchorLine QDeclarativeGraphicsWidget::top() const
{
    Q_D(const QDeclarativeGraphicsWidget);
    return d->anchorLines()->top;
}

QDeclarativeAnchorLine QDeclarativeGraphicsWidget::bottom() const
{
    Q_D(const QDeclarativeGraphicsWidget);
    return d->anchorLines()->bottom;
}

QDeclarativeAnchorLine QDeclarativeGraphicsWidget::verticalCenter() const
{
    Q_D(const QDeclarativeGraphicsWidget);
    return d->anchorLines()->vCenter;
}

QT_END_NAMESPACE

#include <moc_qdeclarativegraphicswidget_p.cpp>
