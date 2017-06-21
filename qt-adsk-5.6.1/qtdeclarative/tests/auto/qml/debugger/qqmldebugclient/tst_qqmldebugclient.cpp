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
#include <qtest.h>
#include <QSignalSpy>
#include <QTimer>
#include <QHostAddress>
#include <QDebug>
#include <QThread>

#include <QtQml/qqmlengine.h>

#include "debugutil_p.h"
#include "qqmldebugtestservice.h"

#include <private/qqmldebugconnector_p.h>

#define PORT 13770
#define STR_PORT "13770"

class tst_QQmlDebugClient : public QObject
{
    Q_OBJECT

private:
    QQmlDebugConnection *m_conn;
    QQmlDebugTestService *m_service;

private slots:
    void initTestCase();

    void name();
    void state();
    void sendMessage();
    void parallelConnect();
    void sequentialConnect();
};

void tst_QQmlDebugClient::initTestCase()
{
    QQmlDebugConnector::setPluginKey(QLatin1String("QQmlDebugServer"));
    QTest::ignoreMessage(QtWarningMsg,
                         "QML debugger: Cannot set plugin key after loading the plugin.");

    m_service = new QQmlDebugTestService("tst_QQmlDebugClient::handshake()");
    const QString waitingMsg = QString("QML Debugger: Waiting for connection on port %1...").arg(PORT);
    QTest::ignoreMessage(QtDebugMsg, waitingMsg.toLatin1().constData());
    QQmlDebuggingEnabler::startTcpDebugServer(PORT);

    new QQmlEngine(this);

    m_conn = new QQmlDebugConnection(this);

    QQmlDebugTestClient client("tst_QQmlDebugClient::handshake()", m_conn);


    for (int i = 0; i < 50; ++i) {
        // try for 5 seconds ...
        m_conn->connectToHost("127.0.0.1", PORT);
        if (m_conn->waitForConnected(100))
            break;
    }

    QVERIFY(m_conn->isConnected());

    QTRY_COMPARE(client.state(), QQmlDebugClient::Enabled);
}

void tst_QQmlDebugClient::name()
{
    QString name = "tst_QQmlDebugClient::name()";

    QQmlDebugClient client(name, m_conn);
    QCOMPARE(client.name(), name);
}

void tst_QQmlDebugClient::state()
{
    {
        QQmlDebugConnection dummyConn;
        QQmlDebugClient client("tst_QQmlDebugClient::state()", &dummyConn);
        QCOMPARE(client.state(), QQmlDebugClient::NotConnected);
        QCOMPARE(client.serviceVersion(), -1.0f);
    }

    QQmlDebugTestClient client("tst_QQmlDebugClient::state()", m_conn);
    QCOMPARE(client.state(), QQmlDebugClient::Unavailable);

    // duplicate plugin name
    QTest::ignoreMessage(QtWarningMsg, "QQmlDebugClient: Conflicting plugin name \"tst_QQmlDebugClient::state()\"");
    QQmlDebugClient client2("tst_QQmlDebugClient::state()", m_conn);
    QCOMPARE(client2.state(), QQmlDebugClient::NotConnected);

    QQmlDebugClient client3("tst_QQmlDebugClient::state3()", 0);
    QCOMPARE(client3.state(), QQmlDebugClient::NotConnected);
}

void tst_QQmlDebugClient::sendMessage()
{
    QQmlDebugTestClient client("tst_QQmlDebugClient::handshake()", m_conn);

    QByteArray msg = "hello!";

    QTRY_COMPARE(client.state(), QQmlDebugClient::Enabled);

    client.sendMessage(msg);
    QByteArray resp = client.waitForResponse();
    QCOMPARE(resp, msg);
}

void tst_QQmlDebugClient::parallelConnect()
{
    QQmlDebugConnection connection2;

    QTest::ignoreMessage(QtWarningMsg, "QML Debugger: Another client is already connected.");
    // will connect & immediately disconnect
    connection2.connectToHost("127.0.0.1", PORT);
    QTRY_COMPARE(connection2.state(), QAbstractSocket::UnconnectedState);
    QVERIFY(m_conn->isConnected());
}

void tst_QQmlDebugClient::sequentialConnect()
{
    QQmlDebugConnection connection2;
    QQmlDebugTestClient client2("tst_QQmlDebugClient::handshake()", &connection2);

    m_conn->close();
    QVERIFY(!m_conn->isConnected());
    QCOMPARE(m_conn->state(), QAbstractSocket::UnconnectedState);

    // Make sure that the disconnect is actually delivered to the server
    QTest::qWait(100);

    connection2.connectToHost("127.0.0.1", PORT);
    QVERIFY(connection2.waitForConnected());
    QVERIFY(connection2.isConnected());
    QTRY_COMPARE(client2.state(), QQmlDebugClient::Enabled);
}

QTEST_MAIN(tst_QQmlDebugClient)

#include "tst_qqmldebugclient.moc"

