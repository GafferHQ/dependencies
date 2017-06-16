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

#include "qquickdial_p.h"

#include <QtCore/qmath.h>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtLabsTemplates/private/qquickcontrol_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Dial
    \inherits Control
    \instantiates QQuickDial
    \inqmlmodule Qt.labs.controls
    \ingroup qtlabscontrols-input
    \brief A circular dial that is rotated to set a value.

    The Dial is similar to a traditional dial knob that is found on devices
    such as stereos or industrial equipment. It allows the user to specify a
    value within a range.

    The value of the dial is set with the \l value property. The range is
    set with the \l from and \l to properties.

    The dial can be manipulated with a keyboard. It supports the following
    actions:

    \table
    \header \li \b {Action} \li \b {Key}
    \row \li Decrease \l value by \l stepSize \li \c Qt.Key_Left
    \row \li Decrease \l value by \l stepSize \li \c Qt.Key_Down
    \row \li Set \l value to \l from \li \c Qt.Key_Home
    \row \li Increase \l value by \l stepSize \li \c Qt.Key_Right
    \row \li Increase \l value by \l stepSize \li \c Qt.Key_Up
    \row \li Set \l value to \l to \li \c Qt.Key_End
    \endtable

    \labs

    \sa {Customizing Dial}, {Input Controls}
*/

static const qreal startAngle = -140;
static const qreal endAngle = 140;

class QQuickDialPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickDial)

public:
    QQuickDialPrivate() :
        from(0),
        to(1),
        value(0),
        position(0),
        angle(startAngle),
        stepSize(0),
        pressed(false),
        snapMode(QQuickDial::NoSnap),
        handle(Q_NULLPTR)
    {
    }

    qreal valueAt(qreal position) const;
    qreal snapPosition(qreal position) const;
    qreal positionAt(const QPoint &point) const;
    void setPosition(qreal position);
    void updatePosition();

    qreal from;
    qreal to;
    qreal value;
    qreal position;
    qreal angle;
    qreal stepSize;
    bool pressed;
    QPoint pressPoint;
    QQuickDial::SnapMode snapMode;
    QQuickItem *handle;
};

qreal QQuickDialPrivate::valueAt(qreal position) const
{
    return from + (to - from) * position;
}

qreal QQuickDialPrivate::snapPosition(qreal position) const
{
    if (qFuzzyIsNull(stepSize))
        return position;
    return qRound(position / stepSize) * stepSize;
}

qreal QQuickDialPrivate::positionAt(const QPoint &point) const
{
    qreal yy = height / 2.0 - point.y();
    qreal xx = point.x() - width / 2.0;
    qreal angle = (xx || yy) ? atan2(yy, xx) : 0;

    if (angle < M_PI / -2)
        angle = angle + M_PI * 2;

    qreal normalizedAngle = (M_PI * 4 / 3 - angle) / (M_PI * 10 / 6);
    return normalizedAngle;
}

void QQuickDialPrivate::setPosition(qreal pos)
{
    Q_Q(QQuickDial);
    pos = qBound<qreal>(0.0, pos, 1.0);
    if (!qFuzzyCompare(position, pos)) {
        position = pos;

        angle = startAngle + position * qAbs(endAngle - startAngle);

        emit q->positionChanged();
        emit q->angleChanged();
    }
}

void QQuickDialPrivate::updatePosition()
{
    qreal pos = 0;
    if (!qFuzzyCompare(from, to))
        pos = (value - from) / (to - from);
    setPosition(pos);
}

QQuickDial::QQuickDial(QQuickItem *parent) :
    QQuickControl(*(new QQuickDialPrivate), parent)
{
    setActiveFocusOnTab(true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

/*!
    \qmlproperty real Qt.labs.controls::Dial::from

    This property holds the starting value for the range. The default value is \c 0.0.

    \sa to, value
*/
qreal QQuickDial::from() const
{
    Q_D(const QQuickDial);
    return d->from;
}

void QQuickDial::setFrom(qreal from)
{
    Q_D(QQuickDial);
    if (!qFuzzyCompare(d->from, from)) {
        d->from = from;
        emit fromChanged();
        if (isComponentComplete()) {
            setValue(d->value);
            d->updatePosition();
        }
    }
}

/*!
    \qmlproperty real Qt.labs.controls::Dial::to

    This property holds the end value for the range. The default value is
    \c 1.0.

    \sa from, value
*/
qreal QQuickDial::to() const
{
    Q_D(const QQuickDial);
    return d->to;
}

void QQuickDial::setTo(qreal to)
{
    Q_D(QQuickDial);
    if (!qFuzzyCompare(d->to, to)) {
        d->to = to;
        emit toChanged();
        if (isComponentComplete()) {
            setValue(d->value);
            d->updatePosition();
        }
    }
}

/*!
    \qmlproperty real Qt.labs.controls::Dial::value

    This property holds the value in the range \c from - \c to. The default
    value is \c 0.0.

    Unlike the \l position property, the \c value is not updated while the
    handle is dragged. The value is updated after the value has been chosen
    and the dial has been released.

    \sa position
*/
qreal QQuickDial::value() const
{
    Q_D(const QQuickDial);
    return d->value;
}

void QQuickDial::setValue(qreal value)
{
    Q_D(QQuickDial);
    if (isComponentComplete())
        value = d->from > d->to ? qBound(d->to, value, d->from) : qBound(d->from, value, d->to);

    if (!qFuzzyCompare(d->value, value)) {
        d->value = value;
        d->updatePosition();
        emit valueChanged();
    }
}

/*!
    \qmlproperty real Qt.labs.controls::Dial::position
    \readonly

    This property holds the logical position of the handle.

    The position is defined as a percentage of the control's angle range (the
    range within which the handle can be moved) scaled to \c {0.0 - 1.0}.
    Unlike the \l value property, the \c position is continuously updated while
    the handle is dragged.

    \sa value, angle
*/
qreal QQuickDial::position() const
{
    Q_D(const QQuickDial);
    return d->position;
}

/*!
    \qmlproperty real Qt.labs.controls::Dial::angle
    \readonly

    This property holds the angle of the handle.

    Like the \l position property, angle is continuously updated while the
    handle is dragged.

    \sa position
*/
qreal QQuickDial::angle() const
{
    Q_D(const QQuickDial);
    return d->angle;
}

/*!
    \qmlproperty real Qt.labs.controls::Dial::stepSize

    This property holds the step size. The default value is \c 0.0.

    \sa snapMode, increase(), decrease()
*/
qreal QQuickDial::stepSize() const
{
    Q_D(const QQuickDial);
    return d->stepSize;
}

void QQuickDial::setStepSize(qreal step)
{
    Q_D(QQuickDial);
    if (!qFuzzyCompare(d->stepSize, step)) {
        d->stepSize = step;
        emit stepSizeChanged();
    }
}

/*!
    \qmlproperty enumeration Qt.labs.controls::Dial::snapMode

    This property holds the snap mode.

    The snap mode works with the \l stepSize to allow the handle to snap to
    certain points along the dial.

    Possible values:
    \value Dial.NoSnap The dial does not snap (default).
    \value Dial.SnapAlways The dial snaps while the handle is dragged.
    \value Dial.SnapOnRelease The dial does not snap while being dragged, but only after the handle is released.

    \sa stepSize
*/
QQuickDial::SnapMode QQuickDial::snapMode() const
{
    Q_D(const QQuickDial);
    return d->snapMode;
}

void QQuickDial::setSnapMode(SnapMode mode)
{
    Q_D(QQuickDial);
    if (d->snapMode != mode) {
        d->snapMode = mode;
        emit snapModeChanged();
    }
}

/*!
    \qmlproperty bool Qt.labs.controls::Dial::pressed

    This property holds whether the dial is pressed.

    The dial will be pressed when either the mouse is pressed over it, or a key
    such as \c Qt.Key_Left is held down. If you'd prefer not to have the dial
    be pressed upon key presses (due to styling reasons, for example), you can
    use the \l {Keys}{Keys attached property}:

    \code
    Dial {
        Keys.onLeftPressed: {}
    }
    \endcode

    This will result in pressed only being \c true upon mouse presses.
*/
bool QQuickDial::isPressed() const
{
    Q_D(const QQuickDial);
    return d->pressed;
}

void QQuickDial::setPressed(bool pressed)
{
    Q_D(QQuickDial);
    if (d->pressed != pressed) {
        d->pressed = pressed;
        setAccessibleProperty("pressed", pressed);
        emit pressedChanged();
    }
}

/*!
    \qmlmethod void Qt.labs.controls::Dial::increase()

    Increases the value by \l stepSize, or \c 0.1 if stepSize is not defined.

    \sa stepSize
*/
void QQuickDial::increase()
{
    Q_D(QQuickDial);
    qreal step = qFuzzyIsNull(d->stepSize) ? 0.1 : d->stepSize;
    setValue(d->value + step);
}

/*!
    \qmlmethod void Qt.labs.controls::Dial::decrease()

    Decreases the value by \l stepSize, or \c 0.1 if stepSize is not defined.

    \sa stepSize
*/
void QQuickDial::decrease()
{
    Q_D(QQuickDial);
    qreal step = qFuzzyIsNull(d->stepSize) ? 0.1 : d->stepSize;
    setValue(d->value - step);
}

/*!
    \qmlproperty component Qt.labs.controls::Dial::handle

    This property holds the handle of the dial.

    The handle acts as a visual indicator of the position of the dial.

    \sa {Customizing Dial}
*/
QQuickItem *QQuickDial::handle() const
{
    Q_D(const QQuickDial);
    return d->handle;
}

void QQuickDial::setHandle(QQuickItem *handle)
{
    Q_D(QQuickDial);
    if (handle != d->handle) {
        d->handle = handle;
        if (d->handle && !d->handle->parentItem())
            d->handle->setParentItem(this);
        emit handleChanged();
    }
}

void QQuickDial::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickDial);
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Down:
        setPressed(true);
        if (isMirrored())
            increase();
        else
            decrease();
        break;

    case Qt::Key_Right:
    case Qt::Key_Up:
        setPressed(true);
        if (isMirrored())
            decrease();
        else
            increase();
        break;

    case Qt::Key_Home:
        setPressed(true);
        setValue(isMirrored() ? d->to : d->from);
        break;

    case Qt::Key_End:
        setPressed(true);
        setValue(isMirrored() ? d->from : d->to);
        break;

    default:
        event->ignore();
        QQuickControl::keyPressEvent(event);
        break;
    }
}

void QQuickDial::keyReleaseEvent(QKeyEvent *event)
{
    QQuickControl::keyReleaseEvent(event);
    setPressed(false);
}

void QQuickDial::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickDial);
    QQuickControl::mousePressEvent(event);
    d->pressPoint = event->pos();
    setPressed(true);
}

void QQuickDial::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickDial);
    QQuickControl::mouseMoveEvent(event);
    if (!keepMouseGrab()) {
        bool overXDragThreshold = QQuickWindowPrivate::dragOverThreshold(event->pos().x() - d->pressPoint.x(), Qt::XAxis, event);
        setKeepMouseGrab(overXDragThreshold);

        if (!overXDragThreshold) {
            bool overYDragThreshold = QQuickWindowPrivate::dragOverThreshold(event->pos().y() - d->pressPoint.y(), Qt::YAxis, event);
            setKeepMouseGrab(overYDragThreshold);
        }
    }
    if (keepMouseGrab()) {
        qreal pos = d->positionAt(event->pos());
        if (d->snapMode == SnapAlways)
            pos = d->snapPosition(pos);
        d->setPosition(pos);
    }
}

void QQuickDial::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickDial);
    QQuickControl::mouseReleaseEvent(event);
    d->pressPoint = QPoint();
    if (keepMouseGrab()) {
        qreal pos = d->positionAt(event->pos());
        if (d->snapMode != NoSnap)
            pos = d->snapPosition(pos);
        setValue(d->valueAt(pos));
        setKeepMouseGrab(false);
    }
    setPressed(false);
}

void QQuickDial::mouseUngrabEvent()
{
    Q_D(QQuickDial);
    QQuickControl::mouseUngrabEvent();
    d->pressPoint = QPoint();
    setPressed(false);
}

void QQuickDial::mirrorChange()
{
    QQuickControl::mirrorChange();
    emit angleChanged();
}

void QQuickDial::componentComplete()
{
    Q_D(QQuickDial);
    QQuickControl::componentComplete();
    setValue(d->value);
    d->updatePosition();
}

#ifndef QT_NO_ACCESSIBILITY
void QQuickDial::accessibilityActiveChanged(bool active)
{
    QQuickControl::accessibilityActiveChanged(active);

    Q_D(QQuickDial);
    if (active)
        setAccessibleProperty("pressed", d->pressed);
}

QAccessible::Role QQuickDial::accessibleRole() const
{
    return QAccessible::Dial;
}
#endif

QT_END_NAMESPACE
