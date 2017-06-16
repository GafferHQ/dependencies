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
#include <QLibraryInfo>

#include "debugutil_p.h"
#include "qqmldebugclient.h"
#include "../../../shared/util.h"

#define STR_PORT_FROM "13773"
#define STR_PORT_TO "13783"

class QQmlEngineControlClient : public QQmlDebugClient
{
    Q_OBJECT
public:
    enum MessageType {
        EngineAboutToBeAdded,
        EngineAdded,
        EngineAboutToBeRemoved,
        EngineRemoved,

        MaximumMessageType
    };

    enum CommandType {
        StartWaitingEngine,
        StopWaitingEngine,

        MaximumCommandType
    };

    QQmlEngineControlClient(QQmlDebugConnection *connection)
        : QQmlDebugClient(QLatin1String("EngineControl"), connection)
    {}


    void command(CommandType command, int engine) {
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream << (int)command << engine;
        sendMessage(message);
    }

    QList<int> startingEngines;
    QList<int> stoppingEngines;

signals:
    void engineAboutToBeAdded();
    void engineAdded();
    void engineAboutToBeRemoved();
    void engineRemoved();

protected:
    void messageReceived(const QByteArray &message);
};

class tst_QQmlEngineControl : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_QQmlEngineControl()
        : m_process(0)
        , m_connection(0)
        , m_client(0)
    {}


private:
    QQmlDebugProcess *m_process;
    QQmlDebugConnection *m_connection;
    QQmlEngineControlClient *m_client;

    void connect(const QString &testFile, bool restrictServices);
    void engine_data();

private slots:
    void cleanup();

    void startEngine_data();
    void startEngine();
    void stopEngine_data();
    void stopEngine();
};

void QQmlEngineControlClient::messageReceived(const QByteArray &message)
{
    QByteArray msg = message;
    QDataStream stream(&msg, QIODevice::ReadOnly);

    int messageType;
    int engineId;
    stream >> messageType >> engineId;

    switch (messageType) {
    case EngineAboutToBeAdded:
        startingEngines.append(engineId);
        emit engineAboutToBeAdded();
        break;
    case EngineAdded:
        QVERIFY(startingEngines.contains(engineId));
        startingEngines.removeOne(engineId);
        emit engineAdded();
        break;
    case EngineAboutToBeRemoved:
        stoppingEngines.append(engineId);
        emit engineAboutToBeRemoved();
        break;
    case EngineRemoved:
        QVERIFY(stoppingEngines.contains(engineId));
        stoppingEngines.removeOne(engineId);
        emit engineRemoved();
        break;
    default:
        QString failMsg = QString("Unknown message type:") + messageType;
        QFAIL(qPrintable(failMsg));
        break;
    }
    QVERIFY(stream.atEnd());
}

void tst_QQmlEngineControl::connect(const QString &testFile, bool restrictServices)
{
    const QString executable = QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlscene";
    QStringList arguments;
    arguments << QString::fromLatin1("-qmljsdebugger=port:%1,%2,block%3")
                 .arg(STR_PORT_FROM).arg(STR_PORT_TO)
                 .arg(restrictServices ? QStringLiteral(",services:EngineControl") : QString());

    arguments << QQmlDataTest::instance()->testFile(testFile);

    m_process = new QQmlDebugProcess(executable, this);
    m_process->start(QStringList() << arguments);
    QVERIFY2(m_process->waitForSessionStart(), "Could not launch application, or did not get 'Waiting for connection'.");

    m_connection = new QQmlDebugConnection();
    m_client = new QQmlEngineControlClient(m_connection);

    const int port = m_process->debugPort();
    m_connection->connectToHost(QLatin1String("127.0.0.1"), port);

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);
}

void tst_QQmlEngineControl::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << (m_process ? m_process->state() : QLatin1String("null"));
        qDebug() << "Application Output:" << (m_process ? m_process->output() : QLatin1String("null"));
        qDebug() << "Connection State:" << (m_connection ? m_connection->stateString() : QLatin1String("null"));
        qDebug() << "Client State:" << (m_client ? m_client->stateString() : QLatin1String("null"));
    }
    delete m_process;
    m_process = 0;
    delete m_client;
    m_client = 0;
    delete m_connection;
    m_connection = 0;
}

void tst_QQmlEngineControl::engine_data()
{
    QTest::addColumn<bool>("restrictMode");
    QTest::newRow("unrestricted") << false;
    QTest::newRow("restricted") << true;
}

void tst_QQmlEngineControl::startEngine_data()
{
    engine_data();
}

void tst_QQmlEngineControl::startEngine()
{
    QFETCH(bool, restrictMode);

    connect("test.qml", restrictMode);

    QTRY_VERIFY(!m_client->startingEngines.empty());
    m_client->command(QQmlEngineControlClient::StartWaitingEngine, m_client->startingEngines.last());

    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineAdded())),
             "No engine start message received in time.");
}

void tst_QQmlEngineControl::stopEngine_data()
{
    engine_data();
}

void tst_QQmlEngineControl::stopEngine()
{
    QFETCH(bool, restrictMode);

    connect("exit.qml", restrictMode);

    QTRY_VERIFY(!m_client->startingEngines.empty());
    m_client->command(QQmlEngineControlClient::StartWaitingEngine, m_client->startingEngines.last());

    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineAdded())),
             "No engine start message received in time.");

    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineAboutToBeRemoved())),
             "No engine about to stop message received in time.");
    m_client->command(QQmlEngineControlClient::StopWaitingEngine, m_client->stoppingEngines.last());
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineRemoved())),
             "No engine stop message received in time.");
}

QTEST_MAIN(tst_QQmlEngineControl)

#include "tst_qqmlenginecontrol.moc"
