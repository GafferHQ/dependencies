/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "sensoritem.h"
#include <QtCore/QDebug>
#include <QtSensors>

QT_BEGIN_NAMESPACE

/*
    \class QPropertyInfo
    \brief The QPropertyInfo type provides an easy access for reading and writing the property values.
*/

/*
    Construct a QPropertyInfo object with parent \a parent
*/
QPropertyInfo::QPropertyInfo(QObject* parent)
    : QObject(parent)
    , _index(0)
    , _isWriteable(false)
    , _name("")
    , _typeName("")
    , _value("")
{}

/*
    Construct a QPropertyInfo object with parent \a parent, property name \a name, property index \a index,
    property write access \a writeable, property type \a typeName and property value \a value
*/
QPropertyInfo::QPropertyInfo(const QString& name, int index, bool writeable, const QString& typeName, const QString& value, QObject* parent)
    : QObject(parent)
    , _index(index)
    , _isWriteable(writeable)
    , _name(name)
    , _typeName(typeName)
    , _value(value)
{}

/*
    \property QPropertyInfo::name
    Returns the name of the property
*/
QString QPropertyInfo::name()
{
    return _name;
}

/*
    \property QPropertyInfo::typeName
    Returns the type of the property
*/
QString QPropertyInfo::typeName()
{
    return _typeName;
}

/*
    \property QPropertyInfo::value
    Returns the current value of the property
*/
QString QPropertyInfo::value()
{
    return _value;
}

/*
    \fn void QPropertyInfo::valueChanged()
    Signal that notifies the client if the property value was changed.
*/

/*
    \fn QPropertyInfo::setValue(const QString& value)
    Sets the value \a value of the property
*/
void QPropertyInfo::setValue(const QString& value)
{
    if (value != _value){
        _value = value;
        emit valueChanged();
    }
}

/*
    \fn QPropertyInfo::index()
    Returns the meta-data index of the property
*/
int QPropertyInfo::index()
{
    return _index;
}

/*
    \property QPropertyInfo::isWriteable
    Returns true if the property is writeable false if property is read only
*/
bool QPropertyInfo::isWriteable()
{
    return _isWriteable;
}

QT_END_NAMESPACE
