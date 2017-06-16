/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Templates module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickswitch_p.h"
#include "qquickabstractbutton_p_p.h"

#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuick/private/qquickevents_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Switch
    \inherits AbstractButton
    \instantiates QQuickSwitch
    \inqmlmodule Qt.labs.controls
    \ingroup qtlabscontrols-buttons
    \brief A switch control.

    \image qtlabscontrols-switch.gif

    Switch is an option button that can be dragged or toggled on (checked) or
    off (unchecked). Switches are typically used to select between two states.

    \table
    \row \li \image qtlabscontrols-switch-normal.png
         \li A switch in its normal state.
    \row \li \image qtlabscontrols-switch-checked.png
         \li A switch that is checked.
    \row \li \image qtlabscontrols-switch-focused.png
         \li A switch that has active focus.
    \row \li \image qtlabscontrols-switch-disabled.png
         \li A switch that is disabled.
    \endtable

    \code
    ColumnLayout {
        Switch {
            text: qsTr("Wi-Fi")
        }
        Switch {
            text: qsTr("Bluetooth")
        }
    }
    \endcode

    \labs

    \sa {Customizing Switch}, {Button Controls}
*/

class QQuickSwitchPrivate : public QQuickAbstractButtonPrivate
{
    Q_DECLARE_PUBLIC(QQuickSwitch)

public:
    QQuickSwitchPrivate() : position(0) { }

    void updatePosition();

    bool handleMousePressEvent(QQuickItem *child, QMouseEvent *event);
    bool handleMouseMoveEvent(QQuickItem *child, QMouseEvent *event);
    bool handleMouseReleaseEvent(QQuickItem *child, QMouseEvent *event);
    bool handleMouseUngrabEvent(QQuickItem *child);

    qreal position;
    QPoint pressPoint;
};

void QQuickSwitchPrivate::updatePosition()
{
    Q_Q(QQuickSwitch);
    q->setPosition(checked ? 1.0 : 0.0);
}

bool QQuickSwitchPrivate::handleMousePressEvent(QQuickItem *child, QMouseEvent *event)
{
    Q_Q(QQuickSwitch);
    Q_UNUSED(child);
    pressPoint = event->pos();
    q->setPressed(true);
    event->accept();
    return true;
}

bool QQuickSwitchPrivate::handleMouseMoveEvent(QQuickItem *child, QMouseEvent *event)
{
    Q_Q(QQuickSwitch);
    if (!child->keepMouseGrab())
        child->setKeepMouseGrab(QQuickWindowPrivate::dragOverThreshold(event->pos().x() - pressPoint.x(), Qt::XAxis, event));
    if (child->keepMouseGrab()) {
        q->setPosition(q->positionAt(event->pos()));
        event->accept();
    }
    return true;
}

bool QQuickSwitchPrivate::handleMouseReleaseEvent(QQuickItem *child, QMouseEvent *event)
{
    Q_Q(QQuickSwitch);
    pressPoint = QPoint();
    q->setPressed(false);
    if (child->keepMouseGrab()) {
        q->setChecked(position > 0.5);
        q->setPosition(checked ? 1.0 : 0.0);
        child->setKeepMouseGrab(false);
        event->accept();
    } else {
        emit q->clicked();
        event->accept();
        q->toggle();
    }
    return true;
}

bool QQuickSwitchPrivate::handleMouseUngrabEvent(QQuickItem *child)
{
    Q_Q(QQuickSwitch);
    Q_UNUSED(child);
    pressPoint = QPoint();
    q->setChecked(position > 0.5);
    q->setPosition(checked ? 1.0 : 0.0);
    q->setPressed(false);
    return true;
}

QQuickSwitch::QQuickSwitch(QQuickItem *parent) :
    QQuickAbstractButton(*(new QQuickSwitchPrivate), parent)
{
    setCheckable(true);
    setFiltersChildMouseEvents(true);
    QObjectPrivate::connect(this, &QQuickAbstractButton::checkedChanged, d_func(), &QQuickSwitchPrivate::updatePosition);
}

/*!
    \qmlproperty real Qt.labs.controls::Switch::position
    \readonly

    This property holds the logical position of the thumb indicator.

    The position is defined as a percentage of the control's size, scaled to
    \c 0.0 - \c 1.0. The position can be used for example to determine whether
    the thumb has been dragged past the halfway. For visualizing a thumb
    indicator, the right-to-left aware \l visualPosition should be used instead.

    \sa visualPosition
*/
qreal QQuickSwitch::position() const
{
    Q_D(const QQuickSwitch);
    return d->position;
}

void QQuickSwitch::setPosition(qreal position)
{
    Q_D(QQuickSwitch);
    position = qBound<qreal>(0.0, position, 1.0);
    if (d->position != position) {
        d->position = position;
        emit positionChanged();
        emit visualPositionChanged();
    }
}

/*!
    \qmlproperty real Qt.labs.controls::Switch::visualPosition
    \readonly

    This property holds the visual position of the thumb indicator.

    The position is defined as a percentage of the control's size, scaled to
    \c 0.0 - \c 1.0. When the control is \l {Control::mirrored}{mirrored}, the
    value is equal to \c {1.0 - position}. This makes the value suitable for
    visualizing the thumb indicator taking right-to-left support into account.
    In order to for example determine whether the thumb has been dragged past
    the halfway, the logical \l position should be used instead.

    \sa position
*/
qreal QQuickSwitch::visualPosition() const
{
    Q_D(const QQuickSwitch);
    if (isMirrored())
        return 1.0 - d->position;
    return d->position;
}

void QQuickSwitch::mirrorChange()
{
    QQuickAbstractButton::mirrorChange();
    emit visualPositionChanged();
}

bool QQuickSwitch::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_D(QQuickSwitch);
    if (child == indicator()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            return d->handleMousePressEvent(child, static_cast<QMouseEvent *>(event));
        case QEvent::MouseMove:
            return d->handleMouseMoveEvent(child, static_cast<QMouseEvent *>(event));
        case QEvent::MouseButtonRelease:
            return d->handleMouseReleaseEvent(child, static_cast<QMouseEvent *>(event));
        case QEvent::UngrabMouse:
            return d->handleMouseUngrabEvent(child);
        default:
            return false;
        }
    }
    return false;
}

qreal QQuickSwitch::positionAt(const QPoint &point) const
{
    qreal pos = point.x() / indicator()->width();
    if (isMirrored())
        return 1.0 - pos;
    return pos;
}

QT_END_NAMESPACE
