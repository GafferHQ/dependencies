/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2013 Kitware, Inc.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of PySide2.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "point.h"

#include "libsamplemacros.h"

LIBSAMPLE_API Point
applyHomogeneousTransform(
    const Point& in,
    double m11, double m12, double m13,
    double m21, double m22, double m23,
    double m31, double m32, double m33,
    bool* okay);

#endif // TRANSFORM_H
