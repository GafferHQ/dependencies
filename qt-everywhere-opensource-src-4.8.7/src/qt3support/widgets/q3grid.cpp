/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#include "q3grid.h"
#include "qlayout.h"
#include "qapplication.h"

QT_BEGIN_NAMESPACE

/*!
    \class Q3Grid
    \brief The Q3Grid widget provides simple geometry management of its children.

    \compat

    The grid places its widgets either in columns or in rows depending
    on its orientation.

    The number of rows \e or columns is defined in the constructor.
    All the grid's children will be placed and sized in accordance
    with their sizeHint() and sizePolicy().

    Use setMargin() to add space around the grid itself, and
    setSpacing() to add space between the widgets.

    \sa Q3VBox Q3HBox QGridLayout
*/

/*!
    \typedef Q3Grid::Direction
    \internal
*/

/*!
    Constructs a grid widget with parent \a parent, called \a name.
    If \a orient is \c Horizontal, \a n specifies the number of
    columns. If \a orient is \c Vertical, \a n specifies the number of
    rows. The widget flags \a f are passed to the Q3Frame constructor.
*/
Q3Grid::Q3Grid(int n, Qt::Orientation orient, QWidget *parent, const char *name,
               Qt::WindowFlags f)
    : Q3Frame(parent, name, f)
{
    int nCols, nRows;
    if (orient == Qt::Horizontal) {
        nCols = n;
        nRows = -1;
    } else {
        nCols = -1;
        nRows = n;
    }
    (new QGridLayout(this, nRows, nCols, 0, 0, name))->setAutoAdd(true);
}



/*!
    Constructs a grid widget with parent \a parent, called \a name.
    \a n specifies the number of columns. The widget flags \a f are
    passed to the Q3Frame constructor.
 */
Q3Grid::Q3Grid(int n, QWidget *parent, const char *name, Qt::WindowFlags f)
    : Q3Frame(parent, name, f)
{
    (new QGridLayout(this, -1, n, 0, 0, name))->setAutoAdd(true);
}


/*!
    Sets the spacing between the child widgets to \a space.
*/

void Q3Grid::setSpacing(int space)
{
    if (layout())
        layout()->setSpacing(space);
}


/*!\reimp
 */
void Q3Grid::frameChanged()
{
    if (layout())
        layout()->setMargin(frameWidth());
}


/*!
  \reimp
*/

QSize Q3Grid::sizeHint() const
{
    QWidget *mThis = (QWidget*)this;
    QApplication::sendPostedEvents(mThis, QEvent::ChildInserted);
    return Q3Frame::sizeHint();
}

QT_END_NAMESPACE
