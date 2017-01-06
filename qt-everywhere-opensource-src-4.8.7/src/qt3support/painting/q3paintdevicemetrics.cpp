/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#include "q3paintdevicemetrics.h"

QT_BEGIN_NAMESPACE

/*!
    \class Q3PaintDeviceMetrics
    \brief The Q3PaintDeviceMetrics class provides information about a
    paint device.

    \compat

    Sometimes when drawing graphics it is necessary to obtain
    information about the physical characteristics of a paint device.
    This class provides the information. For example, to compute the
    aspect ratio of a paint device:

    \snippet doc/src/snippets/code/src_qt3support_painting_q3paintdevicemetrics.cpp 0

    Q3PaintDeviceMetrics contains methods to provide the width and
    height of a device in both pixels (width() and height()) and
    millimeters (widthMM() and heightMM()), the number of colors the
    device supports (numColors()), the number of bit planes (depth()),
    and the resolution of the device (logicalDpiX() and
    logicalDpiY()).

    It is not always possible for Q3PaintDeviceMetrics to compute the
    values you ask for, particularly for external devices. The
    ultimate example is asking for the resolution of of a QPrinter
    that is set to "print to file": who knows what printer that file
    will end up on?
*/

/*!
  \fn Q3PaintDeviceMetrics::Q3PaintDeviceMetrics(const QPaintDevice *pd)

    Constructs a metric for the paint device \a pd.
*/


/*!
    \fn int Q3PaintDeviceMetrics::width() const

    Returns the width of the paint device in default coordinate system
    units (e.g. pixels for QPixmap and QWidget).
*/

/*!
    \fn int Q3PaintDeviceMetrics::height() const

    Returns the height of the paint device in default coordinate
    system units (e.g. pixels for QPixmap and QWidget).
*/

/*!
    \fn int Q3PaintDeviceMetrics::widthMM() const

    Returns the width of the paint device, measured in millimeters.
*/

/*!
    \fn int Q3PaintDeviceMetrics::heightMM() const

    Returns the height of the paint device, measured in millimeters.
*/

/*!
    \fn int Q3PaintDeviceMetrics::numColors() const

    Returns the number of different colors available for the paint
    device. Since this value is an int will not be sufficient to represent
    the number of colors on 32 bit displays, in which case INT_MAX is
    returned instead.
*/

/*!
    \fn int Q3PaintDeviceMetrics::depth() const

    Returns the bit depth (number of bit planes) of the paint device.
*/

/*!
    \fn int Q3PaintDeviceMetrics::logicalDpiX() const

    Returns the horizontal resolution of the device in dots per inch,
    which is used when computing font sizes. For X, this is usually
    the same as could be computed from widthMM(), but it varies on
    Windows.
*/

/*!
    \fn int Q3PaintDeviceMetrics::logicalDpiY() const

    Returns the vertical resolution of the device in dots per inch,
    which is used when computing font sizes. For X, this is usually
    the same as could be computed from heightMM(), but it varies on
    Windows.
*/

/*!
    \fn int Q3PaintDeviceMetrics::physicalDpiX() const
    \internal
*/
/*!
    \fn int Q3PaintDeviceMetrics::physicalDpiY() const
    \internal
*/

QT_END_NAMESPACE
