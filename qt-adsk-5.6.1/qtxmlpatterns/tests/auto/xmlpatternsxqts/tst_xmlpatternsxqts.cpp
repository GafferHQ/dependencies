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
#include "tst_suitetest.h"

/*!
 \class tst_XmlPatternsXQTS
 \internal
 \since 4.4
 \brief Tests the actual engine by running W3C's conformance test suite.
 */
class tst_XmlPatternsXQTS : public tst_SuiteTest
{
    Q_OBJECT
public:
    tst_XmlPatternsXQTS();
public:
    virtual void catalogPath(QString &write) const;
};

tst_XmlPatternsXQTS::tst_XmlPatternsXQTS() : tst_SuiteTest(tst_SuiteTest::XQuerySuite)
{
}

void tst_XmlPatternsXQTS::catalogPath(QString &write) const
{
    if(dontRun())
        QSKIP("This test takes too long time to run on the majority of platforms.");

    write = QFINDTESTDATA("../3rdparty/testsuites/XQTS/XQTSCatalog.xml");
    return;
}

QTEST_MAIN(tst_XmlPatternsXQTS)

#include "tst_xmlpatternsxqts.moc"

// vim: et:ts=4:sw=4:sts=4
