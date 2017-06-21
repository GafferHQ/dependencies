/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qbluetoothhostinfo.h"
#include "qbluetoothhostinfo_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothHostInfo
    \inmodule QtBluetooth
    \brief The QBluetoothHostInfo class encapsulates the details of a local
    QBluetooth device.

    \since 5.2

    This class holds the name and address of a local Bluetooth device.
*/

/*!
    Constructs a null QBluetoothHostInfo object.
*/
QBluetoothHostInfo::QBluetoothHostInfo() :
    d_ptr(new QBluetoothHostInfoPrivate)
{
}

/*!
    Constructs a new QBluetoothHostInfo which is a copy of \a other.
*/
QBluetoothHostInfo::QBluetoothHostInfo(const QBluetoothHostInfo &other) :
    d_ptr(new QBluetoothHostInfoPrivate)
{
    Q_D(QBluetoothHostInfo);

    d->m_address = other.d_func()->m_address;
    d->m_name = other.d_func()->m_name;
}

/*!
    Destroys the QBluetoothHostInfo.
*/
QBluetoothHostInfo::~QBluetoothHostInfo()
{
    delete d_ptr;
}

/*!
    Assigns \a other to this QBluetoothHostInfo instance.
*/
QBluetoothHostInfo &QBluetoothHostInfo::operator=(const QBluetoothHostInfo &other)
{
    Q_D(QBluetoothHostInfo);

    d->m_address = other.d_func()->m_address;
    d->m_name = other.d_func()->m_name;

    return *this;
}

/*!
    \since 5.5

    Returns true if \a other is equal to this QBluetoothHostInfo, otherwise false.
*/
bool QBluetoothHostInfo::operator==(const QBluetoothHostInfo &other) const
{
    if (d_ptr == other.d_ptr)
        return true;

    return d_ptr->m_address == other.d_ptr->m_address && d_ptr->m_name == other.d_ptr->m_name;
}

/*!
    \since 5.5

    Returns true if \a other is not equal to this QBluetoothHostInfo, otherwise false.
*/
bool QBluetoothHostInfo::operator!=(const QBluetoothHostInfo &other) const
{
    return !operator==(other);
}

/*!
    Returns the Bluetooth address as a QBluetoothAddress.
*/
QBluetoothAddress QBluetoothHostInfo::address() const
{
    Q_D(const QBluetoothHostInfo);
    return d->m_address;
}

/*!
    Sets the Bluetooth \a address for this Bluetooth host info object.
*/
void QBluetoothHostInfo::setAddress(const QBluetoothAddress &address)
{
    Q_D(QBluetoothHostInfo);
    d->m_address = address;
}

/*!
    Returns the user visible name of the host info object.
*/
QString QBluetoothHostInfo::name() const
{
    Q_D(const QBluetoothHostInfo);
    return d->m_name;
}

/*!
    Sets the \a name of the host info object.
*/
void QBluetoothHostInfo::setName(const QString &name)
{
    Q_D(QBluetoothHostInfo);
    d->m_name = name;
}

QT_END_NAMESPACE
