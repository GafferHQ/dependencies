/****************************************************************************
**
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


#include "qminimalbackingstore.h"
#include "qminimalintegration.h"
#include "qscreen.h"
#include <QtCore/qdebug.h>
#include <qpa/qplatformscreen.h>
#include <private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

QMinimalBackingStore::QMinimalBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
    , mDebug(QMinimalIntegration::instance()->options() & QMinimalIntegration::DebugBackingStore)
{
    if (mDebug)
        qDebug() << "QMinimalBackingStore::QMinimalBackingStore:" << (quintptr)this;
}

QMinimalBackingStore::~QMinimalBackingStore()
{
}

QPaintDevice *QMinimalBackingStore::paintDevice()
{
    if (mDebug)
        qDebug() << "QMinimalBackingStore::paintDevice";

    return &mImage;
}

void QMinimalBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(window);
    Q_UNUSED(region);
    Q_UNUSED(offset);

    if (mDebug) {
        static int c = 0;
        QString filename = QString("output%1.png").arg(c++, 4, 10, QLatin1Char('0'));
        qDebug() << "QMinimalBackingStore::flush() saving contents to" << filename.toLocal8Bit().constData();
        mImage.save(filename);
    }
}

void QMinimalBackingStore::resize(const QSize &size, const QRegion &)
{
    QImage::Format format = QGuiApplication::primaryScreen()->handle()->format();
    if (mImage.size() != size)
        mImage = QImage(size, format);
}

QT_END_NAMESPACE
