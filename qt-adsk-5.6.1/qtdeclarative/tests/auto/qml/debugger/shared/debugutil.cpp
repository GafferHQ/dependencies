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

#include "debugutil_p.h"

#include <QEventLoop>
#include <QTimer>
#include <QFileInfo>
#include <QDir>

bool QQmlDebugTest::waitForSignal(QObject *receiver, const char *member, int timeout) {
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    QObject::connect(receiver, member, &loop, SLOT(quit()));
    timer.start(timeout);
    loop.exec();
    if (!timer.isActive())
        qWarning("waitForSignal %s timed out after %d ms", member, timeout);
    return timer.isActive();
}

QQmlDebugTestClient::QQmlDebugTestClient(const QString &s, QQmlDebugConnection *c)
    : QQmlDebugClient(s, c)
{
}

QByteArray QQmlDebugTestClient::waitForResponse()
{
    lastMsg.clear();
    QQmlDebugTest::waitForSignal(this, SIGNAL(serverMessage(QByteArray)));
    if (lastMsg.isEmpty()) {
        qWarning() << "no response from server!";
        return QByteArray();
    }
    return lastMsg;
}

void QQmlDebugTestClient::stateChanged(State stat)
{
    QCOMPARE(stat, state());
    emit stateHasChanged();
}

void QQmlDebugTestClient::messageReceived(const QByteArray &ba)
{
    lastMsg = ba;
    emit serverMessage(ba);
}

QQmlDebugProcess::QQmlDebugProcess(const QString &executable, QObject *parent)
    : QObject(parent)
    , m_executable(executable)
    , m_started(false)
    , m_port(0)
    , m_maximumBindErrors(0)
    , m_receivedBindErrors(0)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_timer.setSingleShot(true);
    m_timer.setInterval(5000);
    connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(processAppOutput()));
    connect(&m_process, SIGNAL(errorOccurred(QProcess::ProcessError)),
            this, SLOT(processError(QProcess::ProcessError)));
    connect(&m_timer, SIGNAL(timeout()), SLOT(timeout()));
}

QQmlDebugProcess::~QQmlDebugProcess()
{
    stop();
}

QString QQmlDebugProcess::state()
{
    QString stateStr;
    switch (m_process.state()) {
    case QProcess::NotRunning: {
        stateStr = "not running";
        if (m_process.exitStatus() == QProcess::CrashExit)
            stateStr += " (crashed!)";
        else
            stateStr += ", return value " + QString::number(m_process.exitCode());
        break;
    }
    case QProcess::Starting: stateStr = "starting"; break;
    case QProcess::Running: stateStr = "running"; break;
    }
    return stateStr;
}

void QQmlDebugProcess::start(const QStringList &arguments)
{
#ifdef Q_OS_MAC
    // make sure m_executable points to the actual binary even if it's inside an app bundle
    QFileInfo binFile(m_executable);
    if (!binFile.isExecutable()) {
        QDir bundleDir(m_executable + ".app");
        if (bundleDir.exists()) {
            m_executable = bundleDir.absoluteFilePath("Contents/MacOS/" + binFile.baseName());
            //qDebug() << Q_FUNC_INFO << "found bundled binary" << m_executable;
        }
    }
#endif
    m_mutex.lock();
    m_port = 0;
    m_process.setEnvironment(m_environment);
    m_process.start(m_executable, arguments);
    if (!m_process.waitForStarted()) {
        qWarning() << "QML Debug Client: Could not launch app " << m_executable
                   << ": " << m_process.errorString();
        m_eventLoop.quit();
    } else {
        m_timer.start();
    }
    m_mutex.unlock();
}

void QQmlDebugProcess::stop()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(5000);
    }
}

void QQmlDebugProcess::setMaximumBindErrors(int ignore)
{
    m_maximumBindErrors = ignore;
}

void QQmlDebugProcess::timeout()
{
    qWarning() << "Timeout while waiting for QML debugging messages "
                  "in application output. Process is in state" << m_process.state() << ", Output:" << m_output << ".";
    m_eventLoop.quit();
}

bool QQmlDebugProcess::waitForSessionStart()
{
    if (m_process.state() != QProcess::Running) {
        qWarning() << "Could not start up " << m_executable;
        return false;
    }
    m_eventLoop.exec();

    return m_started;
}

int QQmlDebugProcess::debugPort() const
{
    return m_port;
}

bool QQmlDebugProcess::waitForFinished()
{
    return m_process.waitForFinished();
}

QProcess::ExitStatus QQmlDebugProcess::exitStatus() const
{
    return m_process.exitStatus();
}

void QQmlDebugProcess::setEnvironment(const QStringList &environment)
{
    m_environment = environment;
}

QString QQmlDebugProcess::output() const
{
    return m_output;
}

void QQmlDebugProcess::processAppOutput()
{
    m_mutex.lock();

    bool outputFromAppItself = false;

    QString newOutput = m_process.readAll();
    m_output.append(newOutput);
    m_outputBuffer.append(newOutput);

    while (true) {
        const int nlIndex = m_outputBuffer.indexOf(QLatin1Char('\n'));
        if (nlIndex < 0) // no further complete lines
            break;
        const QString line = m_outputBuffer.left(nlIndex);
        m_outputBuffer = m_outputBuffer.right(m_outputBuffer.size() - nlIndex - 1);

        if (line.contains("QML Debugger:")) {
            const QRegExp portRx("Waiting for connection on port (\\d+)");
            if (portRx.indexIn(line) != -1) {
                m_port = portRx.cap(1).toInt();
                m_timer.stop();
                m_started = true;
                m_eventLoop.quit();
                continue;
            }
            if (line.contains("Unable to listen")) {
                if (++m_receivedBindErrors >= m_maximumBindErrors) {
                    if (m_maximumBindErrors == 0)
                        qWarning() << "App was unable to bind to port!";
                    m_timer.stop();
                    m_eventLoop.quit();
                 }
                 continue;
            }
        } else {
            // set to true if there is output not coming from the debugger
            outputFromAppItself = true;
        }
    }
    m_mutex.unlock();

    if (outputFromAppItself)
        emit readyReadStandardOutput();
}

void QQmlDebugProcess::processError(QProcess::ProcessError error)
{
    if (!m_eventLoop.isRunning())
       return;

    qDebug() << "An error occurred while waiting for debug process to become available:";
    switch (error) {
    case QProcess::FailedToStart: qDebug() << "Process failed to start."; break;
    case QProcess::Crashed: qDebug() << "Process crashed."; break;
    case QProcess::Timedout: qDebug() << "Process timed out."; break;
    case QProcess::WriteError: qDebug() << "Error while writing to process."; break;
    case QProcess::ReadError: qDebug() << "Error while reading from process."; break;
    case QProcess::UnknownError: qDebug() << "Unknown process error."; break;
    }

    m_eventLoop.exit();
}
