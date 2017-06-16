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

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QLibraryInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>
#include <QtTest/QtTest>

#ifndef QT_NO_PROCESS
static const QString msgProcessError(const QProcess &process, const QString &what)
{
    QString result;
    QTextStream(&result) << what << ": \"" << process.program() << ' '
        << process.arguments().join(QLatin1Char(' ')) << "\": " << process.errorString();
    return result;
}

static bool runProcess(const QString &binary,
                       const QStringList &arguments,
                       QString *errorMessage,
                       const QString &workingDir = QString(),
                       const QProcessEnvironment &env = QProcessEnvironment(),
                       int timeOut = 5000,
                       QByteArray *stdOut = Q_NULLPTR, QByteArray *stdErr = Q_NULLPTR)
{
    QProcess process;
    if (!env.isEmpty())
        process.setProcessEnvironment(env);
    if (!workingDir.isEmpty())
        process.setWorkingDirectory(workingDir);
    qDebug().noquote().nospace() << "Running: " << QDir::toNativeSeparators(binary)
        << ' ' << arguments.join(QLatin1Char(' '));
    process.start(binary, arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted()) {
        *errorMessage = msgProcessError(process, "Failed to start");
        return false;
    }
    if (!process.waitForFinished(timeOut)) {
        *errorMessage = msgProcessError(process, "Timed out");
        process.terminate();
        if (!process.waitForFinished(300))
            process.kill();
        return false;
    }
    if (stdOut)
        *stdOut = process.readAllStandardOutput();
    if (stdErr)
        *stdErr= process.readAllStandardError();
    if (process.exitStatus() != QProcess::NormalExit) {
        *errorMessage = msgProcessError(process, "Crashed");
        return false;
    }
    if (process.exitCode() != QProcess::NormalExit) {
        *errorMessage = msgProcessError(process, "Exit code " + QString::number(process.exitCode()));
        return false;
    }
    return true;
}

#endif // !QT_NO_PROCESS

class tst_windeployqt : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void help();
    void deploy();

private:
    QString m_windeployqtBinary;
    QString m_testApp;
    QString m_testAppBinary;
};

void tst_windeployqt::initTestCase()
{
#ifdef QT_NO_PROCESS
    QSKIP("This test requires QProcess support");
#else
    m_windeployqtBinary = QStandardPaths::findExecutable("windeployqt");
    QVERIFY(!m_windeployqtBinary.isEmpty());
    m_testApp = QFINDTESTDATA("testapp");
    QVERIFY(!m_testApp.isEmpty());
    const QFileInfo testAppBinary(m_testApp + QLatin1String("/testapp.exe"));
    QVERIFY2(testAppBinary.isFile(), qPrintable(testAppBinary.absoluteFilePath()));
    m_testAppBinary = testAppBinary.absoluteFilePath();
#endif // QT_NO_PROCESS
}

void tst_windeployqt::help()
{
#ifdef QT_NO_PROCESS
    QSKIP("This test requires QProcess support");
#else
    QString errorMessage;
    QByteArray stdOut;
    QByteArray stdErr;
    QVERIFY2(runProcess(m_windeployqtBinary, QStringList("--help"), &errorMessage,
                        QString(),  QProcessEnvironment(), 5000, &stdOut, &stdErr),
             qPrintable(errorMessage));
    QVERIFY2(!stdOut.isEmpty(), stdErr);
#endif // QT_NO_PROCESS
}

// deploy(): Deploys the test application and launches it with Qt removed from the environment
// to verify it runs stand-alone.

void tst_windeployqt::deploy()
{
#ifdef QT_NO_PROCESS
    QSKIP("This test requires QProcess support");
#else
    QString errorMessage;
    // Deploy application
    QStringList deployArguments;
    deployArguments << QLatin1String("--no-translations") << QDir::toNativeSeparators(m_testAppBinary);
    QVERIFY2(runProcess(m_windeployqtBinary, deployArguments, &errorMessage, QString(), QProcessEnvironment(), 20000),
             qPrintable(errorMessage));

    // Create environment with Qt and all "lib" paths removed.
    const QString qtBinDir = QDir::toNativeSeparators(QLibraryInfo::location(QLibraryInfo::BinariesPath));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString pathKey = QLatin1String("PATH");
    const QChar pathSeparator(QLatin1Char(';')); // ### fixme: Qt 5.6: QDir::listSeparator()
    const QString origPath = env.value(pathKey);
    QString newPath;
    foreach (const QString &pathElement, origPath.split(pathSeparator, QString::SkipEmptyParts)) {
        if (pathElement.compare(qtBinDir, Qt::CaseInsensitive)
            && !pathElement.contains(QLatin1String("\\lib"), Qt::CaseInsensitive)) {
            if (!newPath.isEmpty())
                newPath.append(pathSeparator);
            newPath.append(pathElement);
        }
    }
    if (newPath == origPath)
        qWarning() << "Unable to remove Qt from PATH";
    env.insert(pathKey, newPath);

    // Create qt.conf to enforce usage of local plugins
    QFile qtConf(QFileInfo(m_testAppBinary).absolutePath() + QLatin1String("/qt.conf"));
    QVERIFY2(qtConf.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(qtConf.fileName() + QLatin1String(": ") + qtConf.errorString()));
    QVERIFY(qtConf.write("[Paths]\nPrefix = .\n"));
    qtConf.close();

    // Verify that application still runs
    QVERIFY2(runProcess(m_testAppBinary, QStringList(), &errorMessage, QString(), env, 10000),
             qPrintable(errorMessage));
#endif // QT_NO_PROCESS
}

QTEST_MAIN(tst_windeployqt)
#include "tst_windeployqt.moc"
