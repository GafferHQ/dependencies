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

#ifndef QPRINTENGINE_PS_P_H
#define QPRINTENGINE_PS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of qpsprinter.cpp and qprinter_x11.cpp.
// This header file may change from version to version without notice,
// or even be removed.
//
// We mean it.
//

#ifndef QT_NO_PRINTER

#include "private/qpdf_p.h"
#include "qplatformdefs.h"
#include "QtCore/qlibrary.h"
#include "QtCore/qstringlist.h"
#include "QtCore/qhash.h"
#include "QtCore/qabstractitemmodel.h"

QT_BEGIN_NAMESPACE

class QPrinter;
class QPSPrintEnginePrivate;

class QPSPrintEngine : public QPdfBaseEngine
{
    Q_DECLARE_PRIVATE(QPSPrintEngine)
public:
    // QPrinter uses these
    explicit QPSPrintEngine(QPrinter::PrinterMode m);
    ~QPSPrintEngine();


    virtual bool begin(QPaintDevice *pdev);
    virtual bool end();

    void setBrush();

    virtual void drawImage(const QRectF &r, const QImage &img, const QRectF &sr, Qt::ImageConversionFlags);
    virtual void drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr);
    virtual void drawTiledPixmap(const QRectF &r, const QPixmap &pixmap, const QPointF &s);

    virtual void drawImageInternal(const QRectF &r, QImage img, bool bitmap);

    virtual QPaintEngine::Type type() const { return QPaintEngine::PostScript; }

    virtual bool newPage();
    virtual bool abort();

    virtual QPrinter::PrinterState printerState() const;

    virtual Qt::HANDLE handle() const { return 0; }

private:
    Q_DISABLE_COPY(QPSPrintEngine)
};

class QPSPrintEnginePrivate : public QPdfBaseEnginePrivate {
public:
    QPSPrintEnginePrivate(QPrinter::PrinterMode m);
    ~QPSPrintEnginePrivate();

    void emitHeader(bool finished);
    void emitPages();
    void drawImage(qreal x, qreal y, qreal w, qreal h, const QImage &img, const QImage &mask);
    void flushPage(bool last = false);
    void drawImageHelper(qreal x, qreal y, qreal w, qreal h, const QImage &img, const QImage &mask,
                         bool gray, qreal scaleX, qreal scaleY);

    int         pageCount;
    bool        epsf;
    QByteArray     fontsUsed;

    // stores the descriptions of the n first pages.
    QPdf::ByteStream buffer;
    QByteArray trailer;

    bool firstPage;

    QRect boundingBox;

    QPrinter::PrinterState printerState;
    bool hugeDocument;
    bool headerDone;
};

QT_END_NAMESPACE

#endif // QT_NO_PRINTER

#endif // QPRINTENGINE_PS_P_H
