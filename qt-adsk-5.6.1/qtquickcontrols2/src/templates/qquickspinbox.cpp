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

#include "qquickspinbox_p.h"
#include "qquickcontrol_p_p.h"

#include <QtGui/qguiapplication.h>
#include <QtGui/qstylehints.h>

#include <QtQml/qqmlinfo.h>
#include <QtQml/private/qqmllocale_p.h>
#include <QtQml/private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

// copied from qabstractbutton.cpp
static const int AUTO_REPEAT_DELAY = 300;
static const int AUTO_REPEAT_INTERVAL = 100;

/*!
    \qmltype SpinBox
    \inherits Control
    \instantiates QQuickSpinBox
    \inqmlmodule Qt.labs.controls
    \ingroup input
    \brief A spinbox control.

    \image qtlabscontrols-spinbox.png

    SpinBox allows the user to choose an integer value by clicking the up
    or down indicator buttons, by pressing up or down on the keyboard, or
    by entering a text value in the input field.

    By default, SpinBox provides discrete values in the range of \c [0-99]
    with a \l stepSize of \c 1.

    \snippet qtlabscontrols-spinbox.qml 1

    \section2 Custom Values

    \image qtlabscontrols-spinbox-textual.png

    Even though SpinBox works on integer values, it can be customized to
    accept arbitrary input values. The following snippet demonstrates how
    \l validator, \l textFromValue and \l valueFromText can be used to
    customize the default behavior.

    \snippet qtlabscontrols-spinbox-textual.qml 1

    \labs

    \sa Tumbler, {Customizing SpinBox}
*/

class QQuickSpinBoxPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickSpinBox)

public:
    QQuickSpinBoxPrivate() : from(0), to(99), value(0), stepSize(1),
        delayTimer(0), repeatTimer(0), up(Q_NULLPTR), down(Q_NULLPTR), validator(Q_NULLPTR) { }

    int boundValue(int value) const;
    void updateValue();

    int effectiveStepSize() const;

    void startRepeatDelay();
    void startPressRepeat();
    void stopPressRepeat();

    bool handleMousePressEvent(QQuickItem *child, QMouseEvent *event);
    bool handleMouseMoveEvent(QQuickItem *child, QMouseEvent *event);
    bool handleMouseReleaseEvent(QQuickItem *child, QMouseEvent *event);
    bool handleMouseUngrabEvent(QQuickItem *child);

    int from;
    int to;
    int value;
    int stepSize;
    int delayTimer;
    int repeatTimer;
    QQuickSpinButton *up;
    QQuickSpinButton *down;
    QValidator *validator;
    mutable QJSValue textFromValue;
    mutable QJSValue valueFromText;
};

int QQuickSpinBoxPrivate::boundValue(int value) const
{
    return from > to ? qBound(to, value, from) : qBound(from, value, to);
}

void QQuickSpinBoxPrivate::updateValue()
{
    Q_Q(QQuickSpinBox);
    if (contentItem) {
        QVariant text = contentItem->property("text");
        if (text.isValid()) {
            QQmlEngine *engine = qmlEngine(q);
            if (engine) {
                QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(engine);
                QJSValue loc(v4, QQmlLocale::wrap(v4, locale));
                QJSValue val = q->valueFromText().call(QJSValueList() << text.toString() << loc);
                q->setValue(val.toInt());
            }
        }
    }
}

int QQuickSpinBoxPrivate::effectiveStepSize() const
{
    return from > to ? -1 * stepSize : stepSize;
}

void QQuickSpinBoxPrivate::startRepeatDelay()
{
    Q_Q(QQuickSpinBox);
    stopPressRepeat();
    delayTimer = q->startTimer(AUTO_REPEAT_DELAY);
}

void QQuickSpinBoxPrivate::startPressRepeat()
{
    Q_Q(QQuickSpinBox);
    stopPressRepeat();
    repeatTimer = q->startTimer(AUTO_REPEAT_INTERVAL);
}

void QQuickSpinBoxPrivate::stopPressRepeat()
{
    Q_Q(QQuickSpinBox);
    if (delayTimer > 0) {
        q->killTimer(delayTimer);
        delayTimer = 0;
    }
    if (repeatTimer > 0) {
        q->killTimer(repeatTimer);
        repeatTimer = 0;
    }
}

bool QQuickSpinBoxPrivate::handleMousePressEvent(QQuickItem *child, QMouseEvent *event)
{
    Q_Q(QQuickSpinBox);
    QQuickItem *ui = up->indicator();
    QQuickItem *di = down->indicator();
    up->setPressed(ui && ui->contains(ui->mapFromItem(child, event->pos())));
    down->setPressed(di && di->contains(di->mapFromItem(child, event->pos())));

    bool pressed = up->isPressed() || down->isPressed();
    q->setAccessibleProperty("pressed", pressed);
    if (pressed)
        startRepeatDelay();
    return pressed;
}

bool QQuickSpinBoxPrivate::handleMouseMoveEvent(QQuickItem *child, QMouseEvent *event)
{
    Q_Q(QQuickSpinBox);
    QQuickItem *ui = up->indicator();
    QQuickItem *di = down->indicator();
    up->setPressed(ui && ui->contains(ui->mapFromItem(child, event->pos())));
    down->setPressed(di && di->contains(di->mapFromItem(child, event->pos())));

    bool pressed = up->isPressed() || down->isPressed();
    q->setAccessibleProperty("pressed", pressed);
    stopPressRepeat();
    return pressed;
}

bool QQuickSpinBoxPrivate::handleMouseReleaseEvent(QQuickItem *child, QMouseEvent *event)
{
    Q_Q(QQuickSpinBox);
    QQuickItem *ui = up->indicator();
    QQuickItem *di = down->indicator();
    bool wasPressed = up->isPressed() || down->isPressed();
    if (up->isPressed()) {
        up->setPressed(false);
        if (repeatTimer <= 0 && ui && ui->contains(ui->mapFromItem(child, event->pos())))
            q->increase();
    } else if (down->isPressed()) {
        down->setPressed(false);
        if (repeatTimer <= 0 && di && di->contains(di->mapFromItem(child, event->pos())))
            q->decrease();
    }

    q->setAccessibleProperty("pressed", false);
    stopPressRepeat();
    return wasPressed;
}

bool QQuickSpinBoxPrivate::handleMouseUngrabEvent(QQuickItem *)
{
    Q_Q(QQuickSpinBox);
    up->setPressed(false);
    down->setPressed(false);

    q->setAccessibleProperty("pressed", false);
    stopPressRepeat();
    return false;
}

QQuickSpinBox::QQuickSpinBox(QQuickItem *parent) :
    QQuickControl(*(new QQuickSpinBoxPrivate), parent)
{
    Q_D(QQuickSpinBox);
    d->up = new QQuickSpinButton(this);
    d->down = new QQuickSpinButton(this);

    setFlag(ItemIsFocusScope);
    setFiltersChildMouseEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

/*!
    \qmlproperty int Qt.labs.controls::SpinBox::from

    This property holds the starting value for the range. The default value is \c 0.

    \sa to, value
*/
int QQuickSpinBox::from() const
{
    Q_D(const QQuickSpinBox);
    return d->from;
}

void QQuickSpinBox::setFrom(int from)
{
    Q_D(QQuickSpinBox);
    if (d->from != from) {
        d->from = from;
        emit fromChanged();
        if (isComponentComplete())
            setValue(d->value);
    }
}

/*!
    \qmlproperty int Qt.labs.controls::SpinBox::to

    This property holds the end value for the range. The default value is \c 99.

    \sa from, value
*/
int QQuickSpinBox::to() const
{
    Q_D(const QQuickSpinBox);
    return d->to;
}

void QQuickSpinBox::setTo(int to)
{
    Q_D(QQuickSpinBox);
    if (d->to != to) {
        d->to = to;
        emit toChanged();
        if (isComponentComplete())
            setValue(d->value);
    }
}

/*!
    \qmlproperty int Qt.labs.controls::SpinBox::value

    This property holds the value in the range \c from - \c to. The default value is \c 0.
*/
int QQuickSpinBox::value() const
{
    Q_D(const QQuickSpinBox);
    return d->value;
}

void QQuickSpinBox::setValue(int value)
{
    Q_D(QQuickSpinBox);
    if (isComponentComplete())
        value = d->boundValue(value);

    if (d->value != value) {
        d->value = value;
        emit valueChanged();
    }
}

/*!
    \qmlproperty int Qt.labs.controls::SpinBox::stepSize

    This property holds the step size. The default value is \c 1.

    \sa increase(), decrease()
*/
int QQuickSpinBox::stepSize() const
{
    Q_D(const QQuickSpinBox);
    return d->stepSize;
}

void QQuickSpinBox::setStepSize(int step)
{
    Q_D(QQuickSpinBox);
    if (d->stepSize != step) {
        d->stepSize = step;
        emit stepSizeChanged();
    }
}

/*!
    \qmlproperty Validator Qt.labs.controls::SpinBox::validator

    This property holds the input text validator. By default, SpinBox uses
    \l IntValidator to accept input of integer numbers.

    \snippet SpinBox.qml validator

    \sa textFromValue, valueFromText, {Control::locale}{locale}
*/
QValidator *QQuickSpinBox::validator() const
{
    Q_D(const QQuickSpinBox);
    return d->validator;
}

void QQuickSpinBox::setValidator(QValidator *validator)
{
    Q_D(QQuickSpinBox);
    if (d->validator != validator) {
        d->validator = validator;
        emit validatorChanged();
    }
}

/*!
    \qmlproperty function Qt.labs.controls::SpinBox::textFromValue

    This property holds a callback function that is called whenever
    an integer value needs to be converted to display text.

    The callback function signature is \c {string function(value, locale)}.
    The function can have one or two arguments, where the first argument
    is the value to be converted, and the optional second argument is the
    locale that should be used for the conversion, if applicable.

    The default implementation does the conversion using \l {QtQml::Locale}{Number.toLocaleString()}:

    \code
    textFromValue: function(value, locale) { return Number(value).toLocaleString(locale, 'f', 0); }
    \endcode

    \sa valueFromText, validator, {Control::locale}{locale}
*/
QJSValue QQuickSpinBox::textFromValue() const
{
    Q_D(const QQuickSpinBox);
    if (!d->textFromValue.isCallable()) {
        QQmlEngine *engine = qmlEngine(this);
        if (engine)
            d->textFromValue = engine->evaluate(QStringLiteral("function(value, locale) { return Number(value).toLocaleString(locale, 'f', 0); }"));
    }
    return d->textFromValue;
}

void QQuickSpinBox::setTextFromValue(const QJSValue &callback)
{
    Q_D(QQuickSpinBox);
    if (!callback.isCallable()) {
        qmlInfo(this) << "textFromValue must be a callable function";
        return;
    }
    d->textFromValue = callback;
    emit textFromValueChanged();
}

/*!
    \qmlproperty function Qt.labs.controls::SpinBox::valueFromText

    This property holds a callback function that is called whenever
    input text needs to be converted to an integer value.

    The callback function signature is \c {int function(text, locale)}.
    The function can have one or two arguments, where the first argument
    is the text to be converted, and the optional second argument is the
    locale that should be used for the conversion, if applicable.

    The default implementation does the conversion using \l {QtQml::Locale}{Number.fromLocaleString()}:

    \code
    valueFromText: function(text, locale) { return Number.fromLocaleString(locale, text); }
    \endcode

    \sa textFromValue, validator, {Control::locale}{locale}
*/
QJSValue QQuickSpinBox::valueFromText() const
{
    Q_D(const QQuickSpinBox);
    if (!d->valueFromText.isCallable()) {
        QQmlEngine *engine = qmlEngine(this);
        if (engine)
            d->valueFromText = engine->evaluate(QStringLiteral("function(text, locale) { return Number.fromLocaleString(locale, text); }"));
    }
    return d->valueFromText;
}

void QQuickSpinBox::setValueFromText(const QJSValue &callback)
{
    Q_D(QQuickSpinBox);
    if (!callback.isCallable()) {
        qmlInfo(this) << "valueFromText must be a callable function";
        return;
    }
    d->valueFromText = callback;
    emit valueFromTextChanged();
}

/*!
    \qmlpropertygroup Qt.labs.controls::SpinBox::up
    \qmlproperty bool Qt.labs.controls::SpinBox::up.pressed
    \qmlproperty Item Qt.labs.controls::SpinBox::up.indicator

    These properties hold the up indicator item and whether it is pressed.

    \sa increase()
*/
QQuickSpinButton *QQuickSpinBox::up() const
{
    Q_D(const QQuickSpinBox);
    return d->up;
}

/*!
    \qmlpropertygroup Qt.labs.controls::SpinBox::down
    \qmlproperty bool Qt.labs.controls::SpinBox::down.pressed
    \qmlproperty Item Qt.labs.controls::SpinBox::down.indicator

    These properties hold the down indicator item and whether it is pressed.

    \sa decrease()
*/
QQuickSpinButton *QQuickSpinBox::down() const
{
    Q_D(const QQuickSpinBox);
    return d->down;
}

/*!
    \qmlmethod void Qt.labs.controls::SpinBox::increase()

    Increases the value by \l stepSize.

    \sa stepSize
*/
void QQuickSpinBox::increase()
{
    Q_D(QQuickSpinBox);
    setValue(d->value + d->effectiveStepSize());
}

/*!
    \qmlmethod void Qt.labs.controls::SpinBox::decrease()

    Decreases the value by \l stepSize.

    \sa stepSize
*/
void QQuickSpinBox::decrease()
{
    Q_D(QQuickSpinBox);
    setValue(d->value - d->effectiveStepSize());
}

void QQuickSpinBox::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickSpinBox);
    QQuickControl::keyPressEvent(event);

    switch (event->key()) {
    case Qt::Key_Up:
        increase();
        d->up->setPressed(true);
        event->accept();
        break;

    case Qt::Key_Down:
        decrease();
        d->down->setPressed(true);
        event->accept();
        break;

    default:
        break;
    }

    setAccessibleProperty("pressed", d->up->isPressed() || d->down->isPressed());
}

void QQuickSpinBox::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(QQuickSpinBox);
    QQuickControl::keyReleaseEvent(event);

    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        d->updateValue();

    d->up->setPressed(false);
    d->down->setPressed(false);
    setAccessibleProperty("pressed", false);
}

bool QQuickSpinBox::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_D(QQuickSpinBox);
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

void QQuickSpinBox::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickSpinBox);
    QQuickControl::mousePressEvent(event);
    d->handleMousePressEvent(this, event);
}

void QQuickSpinBox::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickSpinBox);
    QQuickControl::mouseMoveEvent(event);
    d->handleMouseMoveEvent(this, event);
}

void QQuickSpinBox::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickSpinBox);
    QQuickControl::mouseReleaseEvent(event);
    d->handleMouseReleaseEvent(this, event);
}

void QQuickSpinBox::mouseUngrabEvent()
{
    Q_D(QQuickSpinBox);
    QQuickControl::mouseUngrabEvent();
    d->handleMouseUngrabEvent(this);
}

void QQuickSpinBox::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickSpinBox);
    QQuickControl::timerEvent(event);
    if (event->timerId() == d->delayTimer) {
        d->startPressRepeat();
    } else if (event->timerId() == d->repeatTimer) {
        if (d->up->isPressed())
            increase();
        else if (d->down->isPressed())
            decrease();
    }
}

void QQuickSpinBox::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(QQuickSpinBox);
    QQuickControl::itemChange(change, value);
    if (change == ItemActiveFocusHasChanged && !value.boolValue)
        d->updateValue();
}

void QQuickSpinBox::contentItemChange(QQuickItem *newItem, QQuickItem *oldItem)
{
    Q_UNUSED(oldItem);
    if (newItem)
        newItem->setActiveFocusOnTab(true);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickSpinBox::accessibleRole() const
{
    return QAccessible::SpinBox;
}
#endif

class QQuickSpinButtonPrivate : public QObjectPrivate
{
public:
    QQuickSpinButtonPrivate() : pressed(false), indicator(Q_NULLPTR) { }
    bool pressed;
    QQuickItem *indicator;
};

QQuickSpinButton::QQuickSpinButton(QQuickSpinBox *parent) :
    QObject(*(new QQuickSpinButtonPrivate), parent)
{
}

bool QQuickSpinButton::isPressed() const
{
    Q_D(const QQuickSpinButton);
    return d->pressed;
}

void QQuickSpinButton::setPressed(bool pressed)
{
    Q_D(QQuickSpinButton);
    if (d->pressed != pressed) {
        d->pressed = pressed;
        emit pressedChanged();
    }
}

QQuickItem *QQuickSpinButton::indicator() const
{
    Q_D(const QQuickSpinButton);
    return d->indicator;
}

void QQuickSpinButton::setIndicator(QQuickItem *indicator)
{
    Q_D(QQuickSpinButton);
    if (d->indicator != indicator) {
        delete d->indicator;
        d->indicator = indicator;
        if (indicator) {
            if (!indicator->parentItem())
                indicator->setParentItem(static_cast<QQuickItem *>(parent()));
            indicator->setAcceptedMouseButtons(Qt::LeftButton);
        }
        emit indicatorChanged();
    }
}

QT_END_NAMESPACE
