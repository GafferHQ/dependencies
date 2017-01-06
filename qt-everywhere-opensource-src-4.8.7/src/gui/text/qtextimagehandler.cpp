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


#include "qtextimagehandler_p.h"

#include <qapplication.h>
#include <qtextformat.h>
#include <qpainter.h>
#include <qdebug.h>
#include <private/qtextengine_p.h>
#include <qpalette.h>
#include <qtextbrowser.h>
#include <qthread.h>

QT_BEGIN_NAMESPACE

// set by the mime source factory in Qt3Compat
QTextImageHandler::ExternalImageLoaderFunction QTextImageHandler::externalLoader = 0;

static QPixmap getPixmap(QTextDocument *doc, const QTextImageFormat &format)
{
    QPixmap pm;

    QString name = format.name();
    if (name.startsWith(QLatin1String(":/"))) // auto-detect resources
        name.prepend(QLatin1String("qrc"));
    QUrl url = QUrl::fromEncoded(name.toUtf8());
    const QVariant data = doc->resource(QTextDocument::ImageResource, url);
    if (data.type() == QVariant::Pixmap || data.type() == QVariant::Image) {
        pm = qvariant_cast<QPixmap>(data);
    } else if (data.type() == QVariant::ByteArray) {
        pm.loadFromData(data.toByteArray());
    }

    if (pm.isNull()) {
        QString context;
#ifndef QT_NO_TEXTBROWSER
        QTextBrowser *browser = qobject_cast<QTextBrowser *>(doc->parent());
        if (browser)
            context = browser->source().toString();
#endif
        QImage img;
        if (QTextImageHandler::externalLoader)
            img = QTextImageHandler::externalLoader(name, context);

        if (img.isNull()) { // try direct loading
            name = format.name(); // remove qrc:/ prefix again
            if (name.isEmpty() || !img.load(name))
                return QPixmap(QLatin1String(":/trolltech/styles/commonstyle/images/file-16.png"));
        }
        pm = QPixmap::fromImage(img);
        doc->addResource(QTextDocument::ImageResource, url, pm);
    }

    return pm;
}

static QSize getPixmapSize(QTextDocument *doc, const QTextImageFormat &format)
{
    QPixmap pm;

    const bool hasWidth = format.hasProperty(QTextFormat::ImageWidth);
    const int width = qRound(format.width());
    const bool hasHeight = format.hasProperty(QTextFormat::ImageHeight);
    const int height = qRound(format.height());

    QSize size(width, height);
    if (!hasWidth || !hasHeight) {
        pm = getPixmap(doc, format);
        if (!hasWidth) {
            if (!hasHeight)
                size.setWidth(pm.width());
            else
                size.setWidth(qRound(height * (pm.width() / (qreal) pm.height())));
        }
        if (!hasHeight) {
            if (!hasWidth)
                size.setHeight(pm.height());
            else
                size.setHeight(qRound(width * (pm.height() / (qreal) pm.width())));
        }
    }

    qreal scale = 1.0;
    QPaintDevice *pdev = doc->documentLayout()->paintDevice();
    if (pdev) {
        if (pm.isNull())
            pm = getPixmap(doc, format);
        if (!pm.isNull())
            scale = qreal(pdev->logicalDpiY()) / qreal(qt_defaultDpi());
    }
    size *= scale;

    return size;
}

static QImage getImage(QTextDocument *doc, const QTextImageFormat &format)
{
    QImage image;

    QString name = format.name();
    if (name.startsWith(QLatin1String(":/"))) // auto-detect resources
        name.prepend(QLatin1String("qrc"));
    QUrl url = QUrl::fromEncoded(name.toUtf8());
    const QVariant data = doc->resource(QTextDocument::ImageResource, url);
    if (data.type() == QVariant::Image) {
        image = qvariant_cast<QImage>(data);
    } else if (data.type() == QVariant::ByteArray) {
        image.loadFromData(data.toByteArray());
    }

    if (image.isNull()) {
        QString context;
#ifndef QT_NO_TEXTBROWSER
        QTextBrowser *browser = qobject_cast<QTextBrowser *>(doc->parent());
        if (browser)
            context = browser->source().toString();
#endif
        if (QTextImageHandler::externalLoader)
            image = QTextImageHandler::externalLoader(name, context);

        if (image.isNull()) { // try direct loading
            name = format.name(); // remove qrc:/ prefix again
            if (name.isEmpty() || !image.load(name))
                return QImage(QLatin1String(":/trolltech/styles/commonstyle/images/file-16.png"));
        }
        doc->addResource(QTextDocument::ImageResource, url, image);
    }

    return image;
}

static QSize getImageSize(QTextDocument *doc, const QTextImageFormat &format)
{
    QImage image;

    const bool hasWidth = format.hasProperty(QTextFormat::ImageWidth);
    const int width = qRound(format.width());
    const bool hasHeight = format.hasProperty(QTextFormat::ImageHeight);
    const int height = qRound(format.height());

    QSize size(width, height);
    if (!hasWidth || !hasHeight) {
        image = getImage(doc, format);
        if (!hasWidth)
            size.setWidth(image.width());
        if (!hasHeight)
            size.setHeight(image.height());
    }

    qreal scale = 1.0;
    QPaintDevice *pdev = doc->documentLayout()->paintDevice();
    if (pdev) {
        if (image.isNull())
            image = getImage(doc, format);
        if (!image.isNull())
            scale = qreal(pdev->logicalDpiY()) / qreal(qt_defaultDpi());
    }
    size *= scale;

    return size;
}

QTextImageHandler::QTextImageHandler(QObject *parent)
    : QObject(parent)
{
}

QSizeF QTextImageHandler::intrinsicSize(QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(posInDocument)
    const QTextImageFormat imageFormat = format.toImageFormat();

    if (qApp->thread() != QThread::currentThread())
        return getImageSize(doc, imageFormat);
    return getPixmapSize(doc, imageFormat);
}

void QTextImageHandler::drawObject(QPainter *p, const QRectF &rect, QTextDocument *doc, int posInDocument, const QTextFormat &format)
{
    Q_UNUSED(posInDocument)
        const QTextImageFormat imageFormat = format.toImageFormat();

    if (qApp->thread() != QThread::currentThread()) {
        const QImage image = getImage(doc, imageFormat);
        p->drawImage(rect, image, image.rect());
    } else {
        const QPixmap pixmap = getPixmap(doc, imageFormat);
        p->drawPixmap(rect, pixmap, pixmap.rect());
    }
}

QT_END_NAMESPACE
