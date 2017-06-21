/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qkeyevent.h"

QT_BEGIN_NAMESPACE

namespace Qt3DInput {

//Qt6: Move this into a QtQmlGui module and merge it with QQuickKeyEvent

/*!
    \class Qt3DInput::QKeyEvent
    \inmodule Qt3DInput
    \since 5.5
*/

/*!
    \qmltype KeyEvent
    \inqmlmodule Qt3D.Input
    \instantiates Qt3DInput::QKeyEvent
    \since 5.5

    The KeyEvent QML type cannot be directly created. Objects of this type
    are used as signal parameters in KeyboardInput.
*/

QKeyEvent::QKeyEvent(QEvent::Type type, int key, Qt::KeyboardModifiers modifiers, const QString &text, bool autorep, ushort count)
    : QObject()
    , m_event(type, key, modifiers, text, autorep, count)
{
    m_event.setAccepted(false);
}

QKeyEvent::QKeyEvent(const QT_PREPEND_NAMESPACE(QKeyEvent) &ke)
    : QObject()
    , m_event(ke)
{
    m_event.setAccepted(false);
}

/*!
    \qmlproperty int Qt3D.Input::KeyEvent::key
    \readonly

    This property holds the code of the key that was pressed or released.

    See \l [CPP] {Qt::Key}{Qt.Key} for the list of keyboard codes.

    \sa {QtQuick::KeyEvent::key}{KeyEvent.key}
*/

/*!
    \qmlproperty string Qt3D.Input::KeyEvent::text
    \readonly

    This property holds the Unicode text that the key generated. The text
    returned can be an empty string in cases where modifier keys, such as
    Shift, Control, Alt, and Meta, are being pressed or released. In such
    cases \l key will contain a valid value.
*/

/*!
    \qmlproperty int Qt3D.Input::KeyEvent::modifiers
    \readonly

    This property holds the keyboard modifier flags that existed immediately
    before the event occurred.

    \sa {QtQuick::KeyEvent::modifiers}{KeyEvent.modifiers}
*/

/*!
    \qmlproperty bool Qt3D.Input::KeyEvent::isAutoRepeat
    \readonly

    Holds whether this event comes from an auto-repeating key.
*/

/*!
    \qmlproperty int Qt3D.Input::KeyEvent::count
    \readonly

    Holds the number of keys involved in this event. If \l text is not empty,
    this is simply the length of the string.
*/

/*!
    \qmlproperty quint32 Qt3D.Input::KeyEvent::nativeScanCodei
    \readonly

    This property contains the native scan code of the key that was pressed.
    It is passed through from QKeyEvent unchanged.

    \sa QKeyEvent::nativeScanCode()
*/

/*!
    \qmlproperty bool Qt3D.Input::KeyEvent::accepted

    Setting \e accepted to \c true prevents the key event from being propagated
    to the item's parent.

    Generally, if the item acts on the key event then it should be accepted so
    that ancestor items do not also respond to the same event.
*/

/*!
    \qmlmethod bool Qt3D.Input::KeyEvent::matches(StandardKey key)

    Returns \c true if the key event matches the given standard key; otherwise
    returns \c false.

    \sa QKeySequence::StandardKey
*/

} // namespace Qt3DInput

QT_END_NAMESPACE
