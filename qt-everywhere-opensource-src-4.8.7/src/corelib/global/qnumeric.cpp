/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#include "qnumeric.h"
#include "qnumeric_p.h"

QT_BEGIN_NAMESPACE

/*!
    Returns true if the double \a {d} is equivalent to infinity.
*/
Q_CORE_EXPORT bool qIsInf(double d) { return qt_is_inf(d); }

/*!
    Returns true if the double \a {d} is not a number (NaN).
*/
Q_CORE_EXPORT bool qIsNaN(double d) { return qt_is_nan(d); }

/*!
    Returns true if the double \a {d} is a finite number.
*/
Q_CORE_EXPORT bool qIsFinite(double d) { return qt_is_finite(d); }

/*!
    Returns true if the float \a {f} is equivalent to infinity.
*/
Q_CORE_EXPORT bool qIsInf(float f) { return qt_is_inf(f); }

/*!
    Returns true if the float \a {f} is not a number (NaN).
*/
Q_CORE_EXPORT bool qIsNaN(float f) { return qt_is_nan(f); }

/*!
    Returns true if the float \a {f} is a finite number.
*/
Q_CORE_EXPORT bool qIsFinite(float f) { return qt_is_finite(f); }

/*!
    Returns the bit pattern of a signalling NaN as a double.
*/
Q_CORE_EXPORT double qSNaN() { return qt_snan(); }

/*!
    Returns the bit pattern of a quiet NaN as a double.
*/
Q_CORE_EXPORT double qQNaN() { return qt_qnan(); }

/*!
    Returns the bit pattern for an infinite number as a double.
*/
Q_CORE_EXPORT double qInf() { return qt_inf(); }


QT_END_NAMESPACE
