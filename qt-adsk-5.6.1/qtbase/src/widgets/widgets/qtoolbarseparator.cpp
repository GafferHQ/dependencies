/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#include "qtoolbarseparator_p.h"

#ifndef QT_NO_TOOLBAR

#include <qstyle.h>
#include <qstyleoption.h>
#include <qtoolbar.h>
#include <qpainter.h>

QT_BEGIN_NAMESPACE

void QToolBarSeparator::initStyleOption(QStyleOption *option) const
{
    option->initFrom(this);
    if (orientation() == Qt::Horizontal)
        option->state |= QStyle::State_Horizontal;
}

QToolBarSeparator::QToolBarSeparator(QToolBar *parent)
    : QWidget(parent), orient(parent->orientation())
{ setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum); }

void QToolBarSeparator::setOrientation(Qt::Orientation orientation)
{
    orient = orientation;
    update();
}

Qt::Orientation QToolBarSeparator::orientation() const
{ return orient; }

QSize QToolBarSeparator::sizeHint() const
{
    QStyleOption opt;
    initStyleOption(&opt);
    const int extent = style()->pixelMetric(QStyle::PM_ToolBarSeparatorExtent, &opt, parentWidget());
    return QSize(extent, extent);
}

void QToolBarSeparator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QStyleOption opt;
    initStyleOption(&opt);
    style()->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator, &opt, &p, parentWidget());
}

QT_END_NAMESPACE

#include "moc_qtoolbarseparator_p.cpp"

#endif // QT_NO_TOOLBAR
