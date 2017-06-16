/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the TGA autotests in the Qt ImageFormats module.
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
#include <QtGui/QtGui>

class tst_qtga: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void readImage_data();
    void readImage();
};

void tst_qtga::initTestCase()
{
    if (!QImageReader::supportedImageFormats().contains("tga"))
        QSKIP("The image format handler is not installed.");
}

void tst_qtga::readImage_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QSize>("size");

    QTest::newRow("test-flag") << QString("test-flag.tga") << QSize(400, 400);
}

void tst_qtga::readImage()
{
    QFETCH(QString, fileName);
    QFETCH(QSize, size);

    QString path = QString(":/tga/") + fileName;
    QImageReader reader(path);
    QVERIFY(reader.canRead());
    QImage image = reader.read();
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), size);
}

QTEST_MAIN(tst_qtga)
#include "tst_qtga.moc"
