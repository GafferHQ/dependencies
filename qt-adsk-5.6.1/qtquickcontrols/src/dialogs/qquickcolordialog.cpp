/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
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

#include "qquickcolordialog_p.h"
#include <QQuickItem>
#include <private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype AbstractColorDialog
    \instantiates QQuickColorDialog
    \inqmlmodule QtQuick.Dialogs
    \ingroup qtquick-visual
    \brief API wrapper for QML file dialog implementations
    \since 5.1
    \internal

    AbstractColorDialog provides only the API for implementing a color dialog.
    The implementation (e.g. a Window or preferably an Item, in case it is
    shown on a device that doesn't support multiple windows) can be provided as
    \l implementation, which is the default property (the only allowed child
    element).
*/

/*!
    \qmlsignal QtQuick::Dialogs::AbstractColorDialog::accepted

    This signal is emitted by \l accept().

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal QtQuick::Dialogs::AbstractColorDialog::rejected

    This signal is emitted by \l reject().

    The corresponding handler is \c onRejected.
*/

/*!
    \class QQuickColorDialog
    \inmodule QtQuick.Dialogs
    \internal

    The QQuickColorDialog class is a concrete subclass of
    \l QQuickAbstractColorDialog, but it is abstract from the QML perspective
    because it needs to enclose a graphical implementation. It exists in order
    to provide accessors and helper functions which the QML implementation will
    need.

    \since 5.1
*/

/*!
    Constructs a file dialog wrapper with parent window \a parent.
*/
QQuickColorDialog::QQuickColorDialog(QObject *parent)
    : QQuickAbstractColorDialog(parent)
{
}


/*!
    Destroys the file dialog wrapper.
*/
QQuickColorDialog::~QQuickColorDialog()
{
}

/*!
    \qmlproperty bool AbstractColorDialog::visible

    This property holds whether the dialog is visible. By default this is false.
*/

/*!
    \qmlproperty QObject AbstractColorDialog::implementation

    The QML object which implements the actual file dialog. Should be either a
    \l Window or an \l Item.
*/

void QQuickColorDialog::accept()
{
    setColor(m_currentColor);
    QQuickAbstractColorDialog::accept();
}

QT_END_NAMESPACE
