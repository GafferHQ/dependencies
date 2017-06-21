/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef ABSTRACTTOOL_H
#define ABSTRACTTOOL_H

#include <QtCore/QObject>

QT_BEGIN_NAMESPACE
class QMouseEvent;
class QKeyEvent;
class QWheelEvent;
class QTouchEvent;

namespace QmlJSDebugger {

class AbstractViewInspector;

class AbstractTool : public QObject
{
    Q_OBJECT

public:
    explicit AbstractTool(AbstractViewInspector *inspector);

    AbstractViewInspector *inspector() const { return m_inspector; }

    virtual void enable(bool enable) = 0;

    virtual void leaveEvent(QEvent *event) = 0;

    virtual void mousePressEvent(QMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QMouseEvent *event) = 0;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) = 0;

    virtual void hoverMoveEvent(QMouseEvent *event) = 0;
#ifndef QT_NO_WHEELEVENT
    virtual void wheelEvent(QWheelEvent *event) = 0;
#endif

    virtual void keyPressEvent(QKeyEvent *event) = 0;
    virtual void keyReleaseEvent(QKeyEvent *keyEvent) = 0;

    virtual void touchEvent(QTouchEvent *) {}

private:
    AbstractViewInspector *m_inspector;
};

} // namespace QmlJSDebugger

QT_END_NAMESPACE

#endif // ABSTRACTTOOL_H
