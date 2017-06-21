/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QMLPROFILERAPPLICATION_H
#define QMLPROFILERAPPLICATION_H

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include "qmlprofilerclient.h"
#include "qmlprofilerdata.h"

enum PendingRequest {
    REQUEST_QUIT,
    REQUEST_FLUSH_FILE,
    REQUEST_FLUSH,
    REQUEST_OUTPUT_FILE,
    REQUEST_TOGGLE_RECORDING,
    REQUEST_NONE
};

class QmlProfilerApplication : public QCoreApplication
{
    Q_OBJECT
public:
    QmlProfilerApplication(int &argc, char **argv);
    ~QmlProfilerApplication();

    void parseArguments();
    int exec();
    bool isInteractive() const;

signals:
    void readyForCommand();

public slots:
    void userCommand(const QString &command);
    void notifyTraceStarted();
    void outputData();

private slots:
    void run();
    void tryToConnect();
    void connected();
    void connectionStateChanged(QAbstractSocket::SocketState state);
    void connectionError(QAbstractSocket::SocketError error);
    void processHasOutput();
    void processFinished();

    void traceClientEnabled();
    void profilerClientEnabled();
    void traceFinished();

    void prompt(const QString &line = QString(), bool ready = true);
    void logError(const QString &error);
    void logStatus(const QString &status);

    void qmlComplete();
    void v8Complete();

private:
    quint64 parseFeatures(const QStringList &featureList, const QString &values, bool exclude);
    bool checkOutputFile(PendingRequest pending);
    void flush();
    void output();

    enum ApplicationMode {
        LaunchMode,
        AttachMode
    } m_runMode;

    // LaunchMode
    QString m_programPath;
    QStringList m_programArguments;
    QProcess *m_process;

    QString m_hostName;
    quint16 m_port;
    QString m_outputFile;
    QString m_interactiveOutputFile;

    PendingRequest m_pendingRequest;
    bool m_verbose;
    bool m_recording;
    bool m_interactive;

    QQmlDebugConnection m_connection;
    QmlProfilerClient m_qmlProfilerClient;
    V8ProfilerClient m_v8profilerClient;
    QmlProfilerData m_profilerData;
    QTimer m_connectTimer;
    uint m_connectionAttempts;

    bool m_qmlDataReady;
    bool m_v8DataReady;
};

#endif // QMLPROFILERAPPLICATION_H
