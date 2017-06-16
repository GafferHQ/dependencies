/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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


#include <QtTest/QtTest>

#include <qpicture.h>
#include <qpainter.h>
#include <qimage.h>
#include <qpaintengine.h>
#ifndef QT_NO_WIDGETS
#include <qdesktopwidget.h>
#include <qapplication.h>
#endif
#include <limits.h>

class tst_QPicture : public QObject
{
    Q_OBJECT

public:
    tst_QPicture();

private slots:
    void getSetCheck();
    void devType();
    void paintingActive();
    void boundingRect();
    void swap();
    void serialization();

#ifndef QT_NO_WIDGETS
    void save_restore();
#endif

    void boundaryValues_data();
    void boundaryValues();
};

// Testing get/set functions
void tst_QPicture::getSetCheck()
{
    QPictureIO obj1;
    // const QPicture & QPictureIO::picture()
    // void QPictureIO::setPicture(const QPicture &)
    // const char * QPictureIO::format()
    // void QPictureIO::setFormat(const char *)
    const char var2[] = "PNG";
    obj1.setFormat(var2);
    QCOMPARE(var2, obj1.format());
    obj1.setFormat((char *)0);
    // The format is stored internally in a QString, so return is always a valid char *
    QVERIFY(QString(obj1.format()).isEmpty());

    // const char * QPictureIO::parameters()
    // void QPictureIO::setParameters(const char *)
    const char var3[] = "Bogus data";
    obj1.setParameters(var3);
    QCOMPARE(var3, obj1.parameters());
    obj1.setParameters((char *)0);
    // The format is stored internally in a QString, so return is always a valid char *
    QVERIFY(QString(obj1.parameters()).isEmpty());
}

tst_QPicture::tst_QPicture()
{
}

void tst_QPicture::devType()
{
    QPicture p;
    QCOMPARE( p.devType(), (int)QInternal::Picture );
}

void tst_QPicture::paintingActive()
{
    // actually implemented in QPainter but QPicture is a good
    // example of an external paint device
    QPicture p;
    QVERIFY( !p.paintingActive() );
    QPainter pt( &p );
    QVERIFY( p.paintingActive() );
    pt.end();
    QVERIFY( !p.paintingActive() );
}

void tst_QPicture::boundingRect()
{
    QPicture p1;
    // default value
    QVERIFY( !p1.boundingRect().isValid() );

    QRect r1( 20, 30, 5, 15 );
    p1.setBoundingRect( r1 );
    QCOMPARE( p1.boundingRect(), r1 );
    p1.setBoundingRect(QRect());

    QPainter pt( &p1 );
    pt.drawLine( 10, 20, 110, 80 );
    pt.end();

    // assignment and copy constructor
    QRect r2( 10, 20, 100, 60 );
    QCOMPARE( p1.boundingRect(), r2 );
    QPicture p2( p1 );
    QCOMPARE( p2.boundingRect(), r2 );
    QPicture p3;
    p3 = p1;
    QCOMPARE( p3.boundingRect(), r2 );

    {
        QPicture p4;
        QPainter p(&p4);
        p.drawLine(0, 0, 5, 0);
        p.drawLine(0, 0, 0, 5);
        p.end();

        QRect r3(0, 0, 5, 5);
        QCOMPARE(p4.boundingRect(), r3);
    }
}

void tst_QPicture::swap()
{
    QPicture p1, p2;
    QPainter(&p1).drawLine(0, 0, 5, 5);
    QPainter(&p2).drawLine(0, 3, 3, 0);
    QCOMPARE(p1.boundingRect(), QRect(0,0,5,5));
    QCOMPARE(p2.boundingRect(), QRect(0,0,3,3));
    p1.swap(p2);
    QCOMPARE(p1.boundingRect(), QRect(0,0,3,3));
    QCOMPARE(p2.boundingRect(), QRect(0,0,5,5));
}

Q_DECLARE_METATYPE(QDataStream::Version)
Q_DECLARE_METATYPE(QPicture)

void ensureSerializesCorrectly(const QPicture &picture, QDataStream::Version version)
 {
    QDataStream stream;

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    stream.setDevice(&buffer);
    stream.setVersion(version);
    stream << picture;
    buffer.close();

    buffer.open(QIODevice::ReadOnly);
    QPicture readpicture;
    stream >> readpicture;
    QVERIFY2(memcmp(picture.data(), readpicture.data(), picture.size()) == 0,
        qPrintable(QString::fromLatin1("Picture data does not compare equal for QDataStream version %1").arg(version)));
}

class PaintEngine : public QPaintEngine
{
public:
    PaintEngine() : QPaintEngine() {}
    bool begin(QPaintDevice *) { return true; }
    bool end() { return true; }
    void updateState(const QPaintEngineState &) {}
    void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) {}
    Type type() const { return Raster; }

    QFont font() { return state->font(); }
};

class Picture : public QPicture
{
public:
    Picture() : QPicture() {}
    QPaintEngine *paintEngine() const { return (QPaintEngine*)&mPaintEngine; }
private:
    PaintEngine mPaintEngine;
};

void tst_QPicture::serialization()
{
    QDataStream stream;
    const int thisVersion = stream.version();

    for (int version = QDataStream::Qt_1_0; version <= thisVersion; ++version) {
        const QDataStream::Version versionEnum = static_cast<QDataStream::Version>(version);

        {
            // streaming of null pictures
            ensureSerializesCorrectly(QPicture(), versionEnum);
        }
        {
            // picture with a simple line, checking bitwise equality
            QPicture picture;
            QPainter painter(&picture);
            painter.drawLine(10, 20, 30, 40);
            ensureSerializesCorrectly(picture, versionEnum);
        }
    }

    {
        // Test features that were added after Qt 4.5, as that was hard-coded as the major
        // version for a while, which was incorrect. In this case, we'll test font hints.
        QPicture picture;
        QPainter painter;
        QFont font;
        font.setStyleName("Blah");
        font.setHintingPreference(QFont::PreferFullHinting);
        painter.begin(&picture);
        painter.setFont(font);
        painter.drawText(20, 20, "Hello");
        painter.end();

        Picture customPicture;
        painter.begin(&customPicture);
        picture.play(&painter);
        const QFont actualFont = ((PaintEngine*)customPicture.paintEngine())->font();
        painter.end();
        QCOMPARE(actualFont.styleName(), QStringLiteral("Blah"));
        QCOMPARE(actualFont.hintingPreference(), QFont::PreferFullHinting);
    }
}


#ifndef QT_NO_WIDGETS
static QPointF scalePoint(const QPointF &point, QPaintDevice *sourceDevice, QPaintDevice *destDevice)
{
    return QPointF(point.x() * qreal(destDevice->logicalDpiX()) / qreal(sourceDevice->logicalDpiX()),
                   point.y() * qreal(destDevice->logicalDpiY()) / qreal(sourceDevice->logicalDpiY()));
}

static QRectF scaleRect(const QRectF &rect, QPaintDevice *sourceDevice, QPaintDevice *destDevice)
{
    return QRectF(rect.left() * qreal(destDevice->logicalDpiX()) / qreal(sourceDevice->logicalDpiX()),
                  rect.top() * qreal(destDevice->logicalDpiY()) / qreal(sourceDevice->logicalDpiY()),
                  rect.width() * qreal(destDevice->logicalDpiX()) / qreal(sourceDevice->logicalDpiX()),
                  rect.height() * qreal(destDevice->logicalDpiY()) / qreal(sourceDevice->logicalDpiY()));
}

static void paintStuff(QPainter *p)
{
    QPaintDevice *screenDevice = QApplication::desktop();
    p->drawRect(scaleRect(QRectF(100, 100, 100, 100), screenDevice, p->device()));
    p->save();
    p->translate(scalePoint(QPointF(10, 10), screenDevice, p->device()));
    p->restore();
    p->drawRect(scaleRect(QRectF(100, 100, 100, 100), screenDevice, p->device()));
}

/* See task: 41469
   Problem is that the state is not properly restored if the basestate of
   the painter is different when the picture data was created compared to
   the base state of the painter when it is played back.
 */
void tst_QPicture::save_restore()
{
    QPicture pic;
    QPainter p;
    p.begin(&pic);
    paintStuff(&p);
    p.end();

    QPixmap pix1(300, 300);
    pix1.fill(Qt::white);
    p.begin(&pix1);
    p.drawPicture(50, 50, pic);
    p.end();

    QPixmap pix2(300, 300);
    pix2.fill(Qt::white);
    p.begin(&pix2);
    p.translate(50, 50);
    paintStuff(&p);
    p.end();

    QVERIFY( pix1.toImage() == pix2.toImage() );
}
#endif

void tst_QPicture::boundaryValues_data()
{
    QTest::addColumn<int>("x");
    QTest::addColumn<int>("y");
    QTest::newRow("max x") << INT_MAX << 50;
    QTest::newRow("max y") << 50 << INT_MAX;
    QTest::newRow("max x and y") << INT_MAX << INT_MAX;

    QTest::newRow("min x") << INT_MIN << 50;
    QTest::newRow("min y") << 50 << INT_MIN;
    QTest::newRow("min x and y") << INT_MIN << INT_MIN;

    QTest::newRow("min x, max y") << INT_MIN << INT_MAX;
    QTest::newRow("max x, min y") << INT_MAX << INT_MIN;
}

void tst_QPicture::boundaryValues()
{
    QPicture picture;

    QPainter painter;
    painter.begin(&picture);

    QFETCH(int, x);
    QFETCH(int, y);
    painter.drawPoint(QPoint(x, y));

    painter.end();
}


QTEST_MAIN(tst_QPicture)
#include "tst_qpicture.moc"
