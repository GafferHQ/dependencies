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
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <cstdlib>

class tst_qmlplugindump : public QObject
{
    Q_OBJECT
public:
    tst_qmlplugindump();

private slots:
    void initTestCase();
    void builtins();
    void singleton();

private:
    QString qmlplugindumpPath;
};

tst_qmlplugindump::tst_qmlplugindump()
{
}

void tst_qmlplugindump::initTestCase()
{
    qmlplugindumpPath = QLibraryInfo::location(QLibraryInfo::BinariesPath);

#if defined(Q_OS_WIN)
    qmlplugindumpPath += QLatin1String("/qmlplugindump.exe");
#else
    qmlplugindumpPath += QLatin1String("/qmlplugindump");
#endif

    if (!QFileInfo(qmlplugindumpPath).exists()) {
        QString message = QString::fromLatin1("qmlplugindump executable not found (looked for %0)")
                .arg(qmlplugindumpPath);
        QFAIL(qPrintable(message));
    }
}

void tst_qmlplugindump::builtins()
{
    QProcess dumper;
    QStringList args;
    args += QLatin1String("-builtins");
    dumper.start(qmlplugindumpPath, args);
    dumper.waitForFinished();

    if (dumper.error() != QProcess::UnknownError
            || dumper.exitStatus() != QProcess::NormalExit) {
        qWarning() << QString("Error while running '%1 %2'").arg(
                          qmlplugindumpPath, args.join(QLatin1Char(' ')));
    }

    if (dumper.error() == QProcess::FailedToStart) {
        QFAIL("failed to start");
    }
    if (dumper.error() == QProcess::Crashed) {
        qWarning() << "stderr:\n" << dumper.readAllStandardError();
        QFAIL("crashed");
    }

    QCOMPARE(dumper.error(), QProcess::UnknownError);
    QCOMPARE(dumper.exitStatus(), QProcess::NormalExit);

    const QString &result = dumper.readAllStandardOutput();
    QVERIFY(result.contains(QLatin1String("Module {")));
}

void tst_qmlplugindump::singleton()
{
    QProcess dumper;
    QStringList args;
    args << QLatin1String("tests.dumper.CompositeSingleton") << QLatin1String("1.0")
         << QLatin1String(".");
    dumper.start(qmlplugindumpPath, args);
    dumper.waitForFinished();

    const QString &result = dumper.readAllStandardOutput();
    QVERIFY(result.contains(QLatin1String("exports: [\"Singleton 1.0\"]")));
    QVERIFY(result.contains(QLatin1String("exportMetaObjectRevisions: [0]")));
}

QTEST_MAIN(tst_qmlplugindump)

#include "tst_qmlplugindump.moc"
