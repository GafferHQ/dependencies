/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qnamepool_p.h"
#include "qxmlnamepool.h"

QT_BEGIN_NAMESPACE

/*!
  \class QXmlNamePool
  \brief The QXmlNamePool class is a table of shared strings referenced by instances of QXmlName.
  \reentrant
  \since 4.4
  \ingroup xml-tools

  QXmlNamePool is used to optimize operations on instances of
  QXmlName. An instance of QXmlName represents an XML name in
  a way that allows the XML name to be compared and passed around
  efficiently. The efficiency is achieved by storing the strings
  comprising the XML names in an instance of QXmlNamePool, where
  they are mapped to binary identifiers, which are then packed
  into a key which is stored in the QXmlName.

  This means that each instance of QXmlName is tied to the name
  pool it was created with, and that name pool should be kept in
  scope and used to create all instances of QXmlName that might
  be compared. Note also that the name pool is required if you
  must reconstitute the QXmlName as text, or if you must access
  any of its component strings, so although instances of
  QXmlName can be compared without reference to a name pool, the
  name pool must be kept in scope if the name's strings must be
  accessed later.

  \sa QXmlName
  \sa QXmlQuery::namePool()
 */

/*!
  Constructs an empty name pool.
 */
QXmlNamePool::QXmlNamePool() : d(new QPatternist::NamePool())
{
}

/*!
  Constructs a copy of the \a other name pool.
 */
QXmlNamePool::QXmlNamePool(const QXmlNamePool &other) : d(other.d)
{
}

/*!
  Destroys the name pool. Instances of QXmlName constructed
  with this name pool can still be compared after this destructor
  is called, but their text strings cannot be accessed.
 */
QXmlNamePool::~QXmlNamePool()
{
}

QXmlNamePool::QXmlNamePool(QPatternist::NamePool *namePool) : d(QExplicitlySharedDataPointer<QPatternist::NamePool>(namePool))
{
}

/*!
  Assigns the \a other name pool to this one.
 */
QXmlNamePool &QXmlNamePool::operator=(const QXmlNamePool &other)
{
    d = other.d;
    return *this;
}

QT_END_NAMESPACE

