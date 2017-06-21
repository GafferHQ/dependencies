/****************************************************************************
**
** Copyright (C) 2014 BogDan Vatra <bogdan@kde.org>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qandroidplatformbackingstore.h"
#include "qandroidplatformscreen.h"
#include "qandroidplatformwindow.h"
#include <qpa/qplatformscreen.h>

QT_BEGIN_NAMESPACE

QAndroidPlatformBackingStore::QAndroidPlatformBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
{
    if (window->handle())
        setBackingStore(window);
}

QPaintDevice *QAndroidPlatformBackingStore::paintDevice()
{
    return &m_image;
}

void QAndroidPlatformBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(offset);

    if (!m_backingStoreSet)
        setBackingStore(window);

    (static_cast<QAndroidPlatformWindow *>(window->handle()))->repaint(region);
}

void QAndroidPlatformBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);

    if (m_image.size() != size)
        m_image = QImage(size, window()->screen()->handle()->format());
}

void QAndroidPlatformBackingStore::setBackingStore(QWindow *window)
{
    if (window->surfaceType() == QSurface::RasterSurface || window->surfaceType() == QSurface::RasterGLSurface) {
        (static_cast<QAndroidPlatformWindow *>(window->handle()))->setBackingStore(this);
        m_backingStoreSet = true;
    } else {
        qWarning("QAndroidPlatformBackingStore does not support OpenGL-only windows.");
    }
}

QT_END_NAMESPACE
