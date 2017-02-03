/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QVOLATILEIMAGE_P_H
#define QVOLATILEIMAGE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/qimage.h>
#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE

class QVolatileImageData;

class Q_GUI_EXPORT QVolatileImage
{
public:
    QVolatileImage();
    QVolatileImage(int w, int h, QImage::Format format);
    explicit QVolatileImage(const QImage &sourceImage);
    explicit QVolatileImage(void *nativeImage, void *nativeMask = 0);
    QVolatileImage(const QVolatileImage &other);
    ~QVolatileImage();
    QVolatileImage &operator=(const QVolatileImage &rhs);

    bool paintingActive() const;
    bool isNull() const;
    QImage::Format format() const;
    int width() const;
    int height() const;
    int bytesPerLine() const;
    int byteCount() const;
    int depth() const;
    bool hasAlphaChannel() const;
    void beginDataAccess() const;
    void endDataAccess(bool readOnly = false) const;
    uchar *bits();
    const uchar *constBits() const;
    bool ensureFormat(QImage::Format format);
    QImage toImage() const;
    QImage &imageRef();
    const QImage &constImageRef() const;
    QPaintEngine *paintEngine();
    void setAlphaChannel(const QPixmap &alphaChannel);
    void fill(uint pixelValue);
    void *duplicateNativeImage() const;
    void copyFrom(QVolatileImage *source, const QRect &rect);

private:
    QSharedDataPointer<QVolatileImageData> d;
};

QT_END_NAMESPACE

#endif // QVOLATILEIMAGE_P_H
