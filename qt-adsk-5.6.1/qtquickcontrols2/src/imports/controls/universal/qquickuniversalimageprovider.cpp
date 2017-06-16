/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickuniversalimageprovider_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qpainter.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qscreen.h>
#include <QtGui/qicon.h>

QT_BEGIN_NAMESPACE

QQuickUniversalImageProvider::QQuickUniversalImageProvider() : QQuickImageProvider(Image)
{
}

QImage QQuickUniversalImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize);

    int sep = id.indexOf(QLatin1Char('/'));
    QString name = id.left(sep);
    QString color = id.mid(sep + 1);
    qreal dpr = qApp->primaryScreen()->devicePixelRatio();
    QString file = qt_findAtNxFile(QStringLiteral(":/qt-project.org/imports/Qt/labs/controls/universal/images/") + name + QStringLiteral(".png"), dpr);

    QImage image(file);
    if (image.isNull()) {
        qWarning() << "QQuickUniversalImageProvider: unknown id:" << id;
        return QImage();
    }

    if (size)
        *size = image.size();

    if (!color.isEmpty()) {
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), QColor(color));
    }

    return image;
}

QT_END_NAMESPACE
