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

#include "qqmlpropertyvaluesource.h"

#include "qqml.h"

QT_BEGIN_NAMESPACE

/*!
    \class QQmlPropertyValueSource
    \brief The QQmlPropertyValueSource class is an interface for property value sources such as animations and bindings.
    \inmodule QtQml

    See \l{Property Value Sources} for information on writing custom property
    value sources.
 */

/*!
    Constructs a QQmlPropertyValueSource.
*/
QQmlPropertyValueSource::QQmlPropertyValueSource()
{
}

/*!
    Destroys the value source.
*/
QQmlPropertyValueSource::~QQmlPropertyValueSource()
{
}

/*!
    \fn void QQmlPropertyValueSource::setTarget(const QQmlProperty &property)
    Set the target \a property for the value source.  This method will
    be called by the QML engine when assigning a value source.
*/

QT_END_NAMESPACE
