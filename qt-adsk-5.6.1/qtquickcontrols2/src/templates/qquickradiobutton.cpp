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

#include "qquickradiobutton_p.h"
#include "qquickcontrol_p_p.h"

#include <QtGui/qpa/qplatformtheme.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype RadioButton
    \inherits AbstractButton
    \instantiates QQuickRadioButton
    \inqmlmodule Qt.labs.controls
    \ingroup qtlabscontrols-buttons
    \brief A radio button control.

    RadioButton presents an option button that can be toggled on (checked) or
    off (unchecked). Radio buttons are typically used to select one option
    from a set of options.

    \table
    \row \li \image qtlabscontrols-radiobutton-normal.png
         \li A radio button in its normal state.
    \row \li \image qtlabscontrols-radiobutton-checked.png
         \li A radio button that is checked.
    \row \li \image qtlabscontrols-radiobutton-focused.png
         \li A radio button that has active focus.
    \row \li \image qtlabscontrols-radiobutton-disabled.png
         \li A radio button that is disabled.
    \endtable

    Radio buttons are \l {AbstractButton::autoExclusive}{auto-exclusive}
    by default. Only one button can be checked at any time amongst radio
    buttons that belong to the same parent item; checking another button
    automatically unchecks the previously checked one.

    \code
    ColumnLayout {
        RadioButton {
            checked: true
            text: qsTr("First")
        }
        RadioButton {
            text: qsTr("Second")
        }
        RadioButton {
            text: qsTr("Third")
        }
    }
    \endcode

    \labs

    \sa ButtonGroup, {Customizing RadioButton}, {Button Controls}
*/

QQuickRadioButton::QQuickRadioButton(QQuickItem *parent) :
    QQuickAbstractButton(parent)
{
    setCheckable(true);
    setAutoExclusive(true);
}

QFont QQuickRadioButton::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::RadioButtonFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickRadioButton::accessibleRole() const
{
    return QAccessible::RadioButton;
}
#endif

QT_END_NAMESPACE
