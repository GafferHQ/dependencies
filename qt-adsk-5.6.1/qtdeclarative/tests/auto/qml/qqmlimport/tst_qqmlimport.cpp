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
#include <QQmlApplicationEngine>
#include <QtQuick/qquickview.h>
#include <QtQuick/qquickitem.h>
#include <private/qqmlimport_p.h>
#include "../../shared/util.h"

class tst_QQmlImport : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void testDesignerSupported();
    void uiFormatLoading();
    void cleanup();
};

void tst_QQmlImport::cleanup()
{
    QQmlImports::setDesignerSupportRequired(false);
}

void tst_QQmlImport::testDesignerSupported()
{
    QQuickView *window = new QQuickView();
    window->engine()->addImportPath(QT_TESTCASE_BUILDDIR);

    window->setSource(testFileUrl("testfile_supported.qml"));
    QVERIFY(window->errors().isEmpty());

    window->setSource(testFileUrl("testfile_unsupported.qml"));
    QVERIFY(window->errors().isEmpty());

    QQmlImports::setDesignerSupportRequired(true);

    //imports are cached so we create a new window
    delete window;
    window = new QQuickView();

    window->engine()->addImportPath(QT_TESTCASE_BUILDDIR);
    window->engine()->clearComponentCache();

    window->setSource(testFileUrl("testfile_supported.qml"));
    QVERIFY(window->errors().isEmpty());

    QString warningString("%1:35:1: module does not support the designer \"MyPluginUnsupported\" \n     import MyPluginUnsupported 1.0\r \n     ^ ");
#ifndef Q_OS_WIN
    warningString.remove('\r');
#endif
    warningString = warningString.arg(testFileUrl("testfile_unsupported.qml").toString());
    QTest::ignoreMessage(QtWarningMsg, warningString.toAscii());
    window->setSource(testFileUrl("testfile_unsupported.qml"));
    QVERIFY(!window->errors().isEmpty());

    delete window;
}

void tst_QQmlImport::uiFormatLoading()
{
    int size = 0;

    QQmlApplicationEngine *test = new QQmlApplicationEngine(testFileUrl("TestForm.ui.qml"));
    test->addImportPath(QT_TESTCASE_BUILDDIR);
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    QSignalSpy objectCreated(test, SIGNAL(objectCreated(QObject*,QUrl)));
    test->load(testFileUrl("TestForm.ui.qml"));
    QCOMPARE(objectCreated.count(), size);//one less than rootObjects().size() because we missed the first one
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    QByteArray testQml("import QtQml 2.0; QtObject{property bool success: true; property TestForm t: TestForm{}}");
    test->loadData(testQml, testFileUrl("dynamicTestForm.ui.qml"));
    QCOMPARE(objectCreated.count(), size);
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    test->load(testFileUrl("openTestFormFromDir.qml"));
    QCOMPARE(objectCreated.count(), size);
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    test->load(testFileUrl("openTestFormFromQmlDir.qml"));
    QCOMPARE(objectCreated.count(), size);
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    delete test;
}

QTEST_MAIN(tst_QQmlImport)

#include "tst_qqmlimport.moc"
