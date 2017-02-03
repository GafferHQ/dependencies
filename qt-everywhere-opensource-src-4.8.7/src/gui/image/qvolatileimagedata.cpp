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

#include "qvolatileimagedata_p.h"
#include <QtGui/qpaintengine.h>

QT_BEGIN_NAMESPACE

QVolatileImageData::QVolatileImageData()
    : pengine(0)
{
}

QVolatileImageData::QVolatileImageData(int w, int h, QImage::Format format)
    : pengine(0)
{
    image = QImage(w, h, format);
}

QVolatileImageData::QVolatileImageData(const QImage &sourceImage)
    : pengine(0)
{
    image = sourceImage;
}

QVolatileImageData::QVolatileImageData(void *, void *)
    : pengine(0)
{
    // Not supported.
}

QVolatileImageData::QVolatileImageData(const QVolatileImageData &other)
    : QSharedData()
{
    image = other.image;
    // The detach is not mandatory here but we do it nonetheless in order to
    // keep the behavior consistent with other platforms.
    image.detach();
    pengine = 0;
}

QVolatileImageData::~QVolatileImageData()
{
    delete pengine;
}

void QVolatileImageData::beginDataAccess() const
{
    // nothing to do here
}

void QVolatileImageData::endDataAccess(bool readOnly) const
{
    Q_UNUSED(readOnly);
    // nothing to do here
}

bool QVolatileImageData::ensureFormat(QImage::Format format)
{
    if (image.format() != format) {
        image = image.convertToFormat(format);
    }
    return true;
}

void *QVolatileImageData::duplicateNativeImage() const
{
    return 0;
}

void QVolatileImageData::ensureImage()
{
    // nothing to do here
}

QT_END_NAMESPACE
