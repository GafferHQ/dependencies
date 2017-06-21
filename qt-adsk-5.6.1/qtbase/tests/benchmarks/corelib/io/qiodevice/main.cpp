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
#include <QDebug>
#include <QIODevice>
#include <QFile>
#include <QString>

#include <qtest.h>


class tst_qiodevice : public QObject
{
    Q_OBJECT
private slots:
    void read_old();
    void read_old_data() { read_data(); }
    //void read_new();
    //void read_new_data() { read_data(); }
private:
    void read_data();
};


void tst_qiodevice::read_data()
{
    QTest::addColumn<qint64>("size");
    QTest::newRow("10k")      << qint64(10 * 1024);
    QTest::newRow("100k")     << qint64(100 * 1024);
    QTest::newRow("1000k")    << qint64(1000 * 1024);
    QTest::newRow("10000k")   << qint64(10000 * 1024);
    QTest::newRow("100000k")  << qint64(100000 * 1024);
    QTest::newRow("1000000k") << qint64(1000000 * 1024);
}

void tst_qiodevice::read_old()
{
    QFETCH(qint64, size);

    QString name = "tmp" + QString::number(size);

    {
        QFile file(name);
        file.open(QIODevice::WriteOnly);
        file.seek(size);
        file.write("x", 1);
        file.close();
    }

    QBENCHMARK {
        QFile file(name);
        file.open(QIODevice::ReadOnly);
        QByteArray ba;
        qint64 s = size - 1024;
        file.seek(512);
        ba = file.read(s);  // crash happens during this read / assignment operation
    }

    {
        QFile file(name);
        file.remove();
    }
}


QTEST_MAIN(tst_qiodevice)

#include "main.moc"
