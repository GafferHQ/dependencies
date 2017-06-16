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

#include <QtCore/QProcess>
#include <QtCore/QLibraryInfo>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

class tst_QtDiag : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void run();

private:
    QString m_binary;
};

void tst_QtDiag::initTestCase()
{
#ifdef QT_NO_PROCESS
    QSKIP("This test requires QProcess support");
#else
    QString binary = QLibraryInfo::location(QLibraryInfo::BinariesPath) + QStringLiteral("/qtdiag");
#  ifdef Q_OS_WIN
    binary += QStringLiteral(".exe");
#  endif
    const QFileInfo fi(binary);
    if (fi.isFile()) {
        m_binary = fi.absoluteFilePath();
    } else {
        const QByteArray message = QByteArrayLiteral("The binary '")
            + QDir::toNativeSeparators(binary).toLocal8Bit()
            +  QByteArrayLiteral("' does not exist.");
        QSKIP(message.constData());
    }
#endif // !QT_NO_PROCESS
}

void tst_QtDiag::run()
{
#ifdef QT_NO_PROCESS
    QSKIP("This test requires QProcess support");
#else
    if (m_binary.isEmpty())
        QSKIP("Binary could not be found");
    QProcess process;
    qDebug() << "Launching " << QDir::toNativeSeparators(m_binary);
    process.start(m_binary);
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    QByteArray output = process.readAll();
    QVERIFY(!output.isEmpty());
    output.replace('\r', "");
    qDebug("\n%s", output.constData());
#endif // !QT_NO_PROCESS
}

QTEST_MAIN(tst_QtDiag)
#include "tst_qtdiag.moc"
