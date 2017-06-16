/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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

#include "qmacfunctions.h"

#include <QByteArray>
#include <QPixmap>
#include <QUrl>
#include <QString>

#include "qmacfunctions_p.h"

#import <Foundation/Foundation.h>

#ifdef Q_OS_IOS
 #import <CoreGraphics/CoreGraphics.h>
#endif

QT_BEGIN_NAMESPACE

/*!
    \namespace QtMac
    \inmodule QtMacExtras
    \since 5.2
    \brief The QtMac namespace contains miscellaneous functionality specific to the OS X and iOS operating systems.
    \inheaderfile QtMac
 */

/*!
    \fn CGContextRef QtMac::currentCGContext()

    Returns the current CoreGraphics context.
*/

namespace QtMac
{

#if QT_DEPRECATED_SINCE(5,3)
/*!
    \obsolete
    \fn NSData* QtMac::toNSData(const QByteArray &data)

    Use QByteArray::toNSData() instead.
 */
NSData* toNSData(const QByteArray &data)
{
    return [NSData dataWithBytes:data.constData() length:data.size()];
}

/*!
    \obsolete
    \fn QByteArray QtMac::fromNSData(const NSData *data)

    Use QByteArray::fromNSData() instead.
 */
QByteArray fromNSData(const NSData *data)
{
    QByteArray ba;
    ba.resize([data length]);
    [data getBytes:ba.data() length:ba.size()];
    return ba;
}
#endif // QT_DEPRECATED_SINCE

/*!
    \fn CGImageRef QtMac::toCGImageRef(const QPixmap &pixmap)

    Creates a \c CGImageRef equivalent to the QPixmap \a pixmap. Returns the \c CGImageRef handle.

    It is the caller's responsibility to release the \c CGImageRef data
    after use.

    This function is not available in Qt 5.x until 5.0.2 and will return NULL in earlier versions.

    \sa fromCGImageRef()
*/
CGImageRef toCGImageRef(const QPixmap &pixmap)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("qimagetocgimage");
    if (function) {
        typedef CGImageRef (*QImageToCGImageFunction)(const QImage &image);
        return reinterpret_cast<QImageToCGImageFunction>(function)(pixmap.toImage());
    }

    return NULL;
#else
    return pixmap.toCGImageRef();
#endif
}

/*!
    \fn QPixmap QtMac::fromCGImageRef(CGImageRef image)

    Returns a QPixmap that is equivalent to the given \a image.

    This function is not available in Qt 5.x until 5.0.2 and will return a null pixmap in earlier versions.

    \sa toCGImageRef(), {QPixmap#Pixmap Conversion}{Pixmap Conversion}
*/
QPixmap fromCGImageRef(CGImageRef image)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("cgimagetoqimage");
    if (function) {
        typedef QImage (*CGImageToQImageFunction)(CGImageRef image);
        return QPixmap::fromImage(reinterpret_cast<CGImageToQImageFunction>(function)(image));
    }

    return QPixmap();
#else
    return QPixmap::fromCGImageRef(image);
#endif
}

} // namespace QtMac

QT_END_NAMESPACE
