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

#include <QtXmlPatterns/QXmlItem>
#include <QtXmlPatterns/QXmlQuery>
#include <QtXmlPatterns/QXmlResultItems>

#include "../qxmlquery/MessageSilencer.h"
/*!
 \class tst_QXmlResultItems
 \internal
 \since 4.4
 \brief Tests class QXmlResultItems.

 */
class tst_QXmlResultItems : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultConstructor() const;
    void next() const;
    void current() const;
    void hasError() const;
    void objectSize() const;
    void constCorrectness() const;

    void evalateWithInstantError() const;
    void evalateWithQueryError() const;
    void evaluate() const;
    void evaluate_data() const;
};

void tst_QXmlResultItems::defaultConstructor() const
{
    {
        QXmlResultItems result;
    }

    {
        QXmlResultItems result1;
        QXmlResultItems result2;
    }

    {
        QXmlResultItems result1;
        QXmlResultItems result2;
        QXmlResultItems result3;
    }
}

void tst_QXmlResultItems::next() const
{
    /* Check default value. */
    {
        QXmlResultItems result;
        QVERIFY(result.next().isNull());
    }

    /* Stress it on a default constructed value. */
    {
        QXmlResultItems result;
        QVERIFY(result.next().isNull());
        QVERIFY(result.next().isNull());
        QVERIFY(result.next().isNull());
    }
}

void tst_QXmlResultItems::current() const
{
    /* Check default value. */
    {
        QXmlResultItems result;
        QVERIFY(result.current().isNull());
    }

    /* Stress it on a default constructed value. */
    {
        QXmlResultItems result;
        QVERIFY(result.current().isNull());
        QVERIFY(result.current().isNull());
        QVERIFY(result.current().isNull());
    }
}

void tst_QXmlResultItems::hasError() const
{
    /* Check default value. */
    {
        QXmlResultItems result;
        QVERIFY(!result.hasError());
    }

    /* Stress it on a default constructed value. */
    {
        QXmlResultItems result;
        QVERIFY(!result.hasError());
        QVERIFY(!result.hasError());
        QVERIFY(!result.hasError());
    }
}

void tst_QXmlResultItems::objectSize() const
{
    /* A d-pointer plus a vtable pointer. */
    QCOMPARE(sizeof(QXmlResultItems), sizeof(void *) * 2);
}

void tst_QXmlResultItems::constCorrectness() const
{
    const QXmlResultItems result;

    /* These functions should be const. */
    result.current();
    result.hasError();
}

/*!
 What's special about this is that it's not the QAbstractXmlForwardIterator::next()
 that triggers the error, it's QPatternist::Expression::evaluateSingleton() directly.
 */
void tst_QXmlResultItems::evalateWithInstantError() const
{
    QXmlQuery query;
    MessageSilencer silencer;
    query.setMessageHandler(&silencer);
    query.setQuery(QLatin1String("fn:error()"));

    QXmlResultItems result;
    query.evaluateTo(&result);

    /* Check the values, and stress it. */
    for(int i = 0; i < 3; ++i)
    {
        QVERIFY(result.current().isNull());
        QVERIFY(result.next().isNull());
        QVERIFY(result.hasError());
    }
}

void tst_QXmlResultItems::evaluate() const
{
    QFETCH(QString, queryString);

    QXmlQuery query;
    query.setQuery(queryString);

    QVERIFY(query.isValid());

    QXmlResultItems result;
    query.evaluateTo(&result);
    QXmlItem item(result.next());

    while(!item.isNull())
    {
        QVERIFY(!result.current().isNull());
        QVERIFY(!result.hasError());
        item = result.next();
    }

    /* Now, stress beyond the end. */
    for(int i = 0; i < 3; ++i)
    {
        QVERIFY(result.current().isNull());
        QVERIFY(result.next().isNull());
    }
}

void tst_QXmlResultItems::evaluate_data() const
{
    QTest::addColumn<QString>("queryString");

    QTest::newRow("one atomic value") << QString::fromLatin1("1");
    QTest::newRow("two atomic values") << QString::fromLatin1("1, xs:hexBinary('FF')");
    QTest::newRow("one node") << QString::fromLatin1("attribute name {'body'}");
    QTest::newRow("one node") << QString::fromLatin1("attribute name {'body'}");
    QTest::newRow("two nodes") << QString::fromLatin1("attribute name {'body'}, <!-- comment -->");
}

void tst_QXmlResultItems::evalateWithQueryError() const
{
    /* This query is invalid. */
    const QXmlQuery query;

    QXmlResultItems result;
    query.evaluateTo(&result);

    QVERIFY(result.hasError());
    QVERIFY(result.next().isNull());
}

QTEST_MAIN(tst_QXmlResultItems)

#include "tst_qxmlresultitems.moc"

// vim: et:ts=4:sw=4:sts=4
