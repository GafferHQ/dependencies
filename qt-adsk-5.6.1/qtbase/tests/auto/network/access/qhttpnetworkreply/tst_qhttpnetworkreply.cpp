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
#include <QtCore/QBuffer>
#include <QtCore/QByteArray>

#include "private/qhttpnetworkconnection_p.h"

class tst_QHttpNetworkReply: public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void initTestCase();
    void cleanupTestCase();

    void parseHeader_data();
    void parseHeader();

    void parseEndOfHeader_data();
    void parseEndOfHeader();
};


void tst_QHttpNetworkReply::initTestCase()
{
}

void tst_QHttpNetworkReply::cleanupTestCase()
{
}

void tst_QHttpNetworkReply::init()
{
}

void tst_QHttpNetworkReply::cleanup()
{
}

void tst_QHttpNetworkReply::parseHeader_data()
{
    QTest::addColumn<QByteArray>("headers");
    QTest::addColumn<QStringList>("fields");
    QTest::addColumn<QStringList>("values");

    QTest::newRow("empty-field") << QByteArray("Set-Cookie: \r\n")
                                 << (QStringList() << "Set-Cookie")
                                 << (QStringList() << "");
    QTest::newRow("single-field") << QByteArray("Content-Type: text/html; charset=utf-8\r\n")
                                  << (QStringList() << "Content-Type")
                                  << (QStringList() << "text/html; charset=utf-8");
    QTest::newRow("single-field-continued") << QByteArray("Content-Type: text/html;\r\n"
                                                          " charset=utf-8\r\n")
                                            << (QStringList() << "Content-Type")
                                            << (QStringList() << "text/html; charset=utf-8");

    QTest::newRow("multi-field") << QByteArray("Content-Type: text/html; charset=utf-8\r\n"
                                               "Content-Length: 1024\r\n"
                                               "Content-Encoding: gzip\r\n")
                                 << (QStringList() << "Content-Type" << "Content-Length" << "Content-Encoding")
                                 << (QStringList() << "text/html; charset=utf-8" << "1024" << "gzip");
    QTest::newRow("multi-field-with-emtpy") << QByteArray("Content-Type: text/html; charset=utf-8\r\n"
                                                          "Content-Length: 1024\r\n"
                                                          "Set-Cookie: \r\n"
                                                          "Content-Encoding: gzip\r\n")
                                            << (QStringList() << "Content-Type" << "Content-Length" << "Set-Cookie" << "Content-Encoding")
                                            << (QStringList() << "text/html; charset=utf-8" << "1024" << "" << "gzip");

    QTest::newRow("lws-field") << QByteArray("Content-Type: text/html; charset=utf-8\r\n"
                                             "Content-Length:\r\n 1024\r\n"
                                             "Content-Encoding: gzip\r\n")
                               << (QStringList() << "Content-Type" << "Content-Length" << "Content-Encoding")
                               << (QStringList() << "text/html; charset=utf-8" << "1024" << "gzip");

    QTest::newRow("duplicated-field") << QByteArray("Vary: Accept-Language\r\n"
                                                    "Vary: Cookie\r\n"
                                                    "Vary: User-Agent\r\n")
                                      << (QStringList() << "Vary")
                                      << (QStringList() << "Accept-Language, Cookie, User-Agent");
}

void tst_QHttpNetworkReply::parseHeader()
{
    QFETCH(QByteArray, headers);
    QFETCH(QStringList, fields);
    QFETCH(QStringList, values);

    QHttpNetworkReply reply;
    reply.parseHeader(headers);
    for (int i = 0; i < fields.count(); ++i) {
        //qDebug() << "field" << fields.at(i) << "value" << reply.headerField(fields.at(i)) << "expected" << values.at(i);
        QString field = reply.headerField(fields.at(i).toLatin1());
        QCOMPARE(field, values.at(i));
    }
}

class TestHeaderSocket : public QAbstractSocket
{
public:
    explicit TestHeaderSocket(const QByteArray &input) : QAbstractSocket(QAbstractSocket::TcpSocket, Q_NULLPTR)
    {
        inputBuffer.setData(input);
        inputBuffer.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    qint64 readData(char *data, qint64 maxlen) { return inputBuffer.read(data, maxlen); }

    QBuffer inputBuffer;
};

class TestHeaderReply : public QHttpNetworkReply
{
public:
    QHttpNetworkReplyPrivate *replyPrivate() { return static_cast<QHttpNetworkReplyPrivate *>(d_ptr.data()); }
};

void tst_QHttpNetworkReply::parseEndOfHeader_data()
{
    QTest::addColumn<QByteArray>("headers");
    QTest::addColumn<qint64>("lengths");

    QTest::newRow("CRLFCRLF") << QByteArray("Content-Type: text/html; charset=utf-8\r\n"
                                            "Content-Length:\r\n 1024\r\n"
                                            "Content-Encoding: gzip\r\n\r\nHTTPBODY")
                               << qint64(90);

    QTest::newRow("CRLFLF") << QByteArray("Content-Type: text/html; charset=utf-8\r\n"
                                          "Content-Length:\r\n 1024\r\n"
                                          "Content-Encoding: gzip\r\n\nHTTPBODY")
                            << qint64(89);

    QTest::newRow("LFCRLF") << QByteArray("Content-Type: text/html; charset=utf-8\r\n"
                                          "Content-Length:\r\n 1024\r\n"
                                          "Content-Encoding: gzip\n\r\nHTTPBODY")
                            << qint64(89);

    QTest::newRow("LFLF") << QByteArray("Content-Type: text/html; charset=utf-8\r\n"
                                        "Content-Length:\r\n 1024\r\n"
                                        "Content-Encoding: gzip\n\nHTTPBODY")
                          << qint64(88);
}

void tst_QHttpNetworkReply::parseEndOfHeader()
{
    QFETCH(QByteArray, headers);
    QFETCH(qint64, lengths);

    TestHeaderSocket socket(headers);

    TestHeaderReply reply;

    QHttpNetworkReplyPrivate *replyPrivate = reply.replyPrivate();
    qint64 headerBytes = replyPrivate->readHeader(&socket);
    QCOMPARE(headerBytes, lengths);
}

QTEST_MAIN(tst_QHttpNetworkReply)
#include "tst_qhttpnetworkreply.moc"
