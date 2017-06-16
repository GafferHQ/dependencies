/****************************************************************************
**
** Copyright (C) 2014 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebSockets module of the Qt Toolkit.
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
#include <QtTest/qtestcase.h>
#include <QSignalSpy>
#include <QHostInfo>
#include <QDebug>
#include "QtWebSockets/QWebSocket"

class tst_WebSocketsTest : public QObject
{
    Q_OBJECT

public:
    tst_WebSocketsTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    /**
      * @brief Test isValid() with an unoped socket
      */
    void testInvalidWithUnopenedSocket();

    /**
     * @brief testTextMessage Tests sending and receiving a text message
     */
    void testTextMessage();

    void testBinaryMessage();

    /**
     * @brief Tests the method localAddress and localPort
     */
    void testLocalAddress();

    /**
     * @brief Test the methods peerAddress, peerName and peerPort
     */
    void testPeerAddress();

    /**
     * @brief Test the methods proxy() and setProxy() and check if it can be correctly set
     */
    void testProxy();

    /**
     * @brief Runs the autobahn tests against our implementation
     */
    //void autobahnTest();

private:
    QWebSocket *m_pWebSocket;
    QUrl m_url;
};

tst_WebSocketsTest::tst_WebSocketsTest() :
    m_pWebSocket(0),
    m_url(QStringLiteral("ws://localhost:9000"))
{
}

void tst_WebSocketsTest::initTestCase()
{
    m_pWebSocket = new QWebSocket();
    m_pWebSocket->open(m_url, true);
    QTRY_VERIFY_WITH_TIMEOUT(m_pWebSocket->state() == QAbstractSocket::ConnectedState, 1000);
    QVERIFY(m_pWebSocket->isValid());
}

void tst_WebSocketsTest::cleanupTestCase()
{
    if (m_pWebSocket)
    {
        m_pWebSocket->close();
        //QVERIFY2(m_pWebSocket->waitForDisconnected(1000), "Disconnection failed.");
        delete m_pWebSocket;
        m_pWebSocket = 0;
    }
}

void tst_WebSocketsTest::init()
{
}

void tst_WebSocketsTest::cleanup()
{
}

void tst_WebSocketsTest::testTextMessage()
{
    const QString message = QStringLiteral("Hello world!");

    QSignalSpy spy(m_pWebSocket, SIGNAL(textMessageReceived(QString)));

    QCOMPARE(m_pWebSocket->sendTextMessage(message), qint64(message.length()));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() != 0, 1000);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), message);
}

void tst_WebSocketsTest::testBinaryMessage()
{
    QSignalSpy spy(m_pWebSocket, SIGNAL(binaryMessageReceived(QByteArray)));

    QByteArray data("Hello world!");

    QCOMPARE(m_pWebSocket->sendBinaryMessage(data), qint64(data.size()));

    QTRY_VERIFY_WITH_TIMEOUT(spy.count() != 0, 1000);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), data);
}

void tst_WebSocketsTest::testLocalAddress()
{
    QCOMPARE(m_pWebSocket->localAddress().toString(), QStringLiteral("127.0.0.1"));
    quint16 localPort = m_pWebSocket->localPort();
    QVERIFY2(localPort > 0, "Local port is invalid.");
}

void tst_WebSocketsTest::testPeerAddress()
{
    QHostInfo hostInfo = QHostInfo::fromName(m_url.host());
    QList<QHostAddress> addresses = hostInfo.addresses();
    QVERIFY(addresses.length() > 0);
    QHostAddress peer = m_pWebSocket->peerAddress();
    bool found = false;
    Q_FOREACH (const QHostAddress &a, addresses)
    {
        if (a == peer)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        QFAIL("PeerAddress is not found as a result of a reverse lookup");
    }
    QCOMPARE(m_pWebSocket->peerName(), m_url.host());
    QCOMPARE(m_pWebSocket->peerPort(), (quint16)m_url.port(80));
}

void tst_WebSocketsTest::testProxy()
{
    QNetworkProxy oldProxy = m_pWebSocket->proxy();
    QNetworkProxy proxy(QNetworkProxy::HttpProxy, QStringLiteral("proxy.network.com"), 80);
    m_pWebSocket->setProxy(proxy);
    QCOMPARE(proxy, m_pWebSocket->proxy());
    m_pWebSocket->setProxy(oldProxy);
    QCOMPARE(oldProxy, m_pWebSocket->proxy());
}

void tst_WebSocketsTest::testInvalidWithUnopenedSocket()
{
    QWebSocket qws;
    QCOMPARE(qws.isValid(), false);
}

QTEST_MAIN(tst_WebSocketsTest)

#include "tst_websockets.moc"

