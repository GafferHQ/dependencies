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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtLocation/QPlaceDetailsReply>

QT_USE_NAMESPACE

class TestDetailsReply : public QPlaceDetailsReply
{
    Q_OBJECT
public:
    TestDetailsReply(QObject *parent) : QPlaceDetailsReply(parent){}
    ~TestDetailsReply() {}
    void setPlace(const QPlace &place) { QPlaceDetailsReply::setPlace(place); }
};

class tst_QPlaceDetailsReply : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceDetailsReply();

private Q_SLOTS:
    void constructorTest();
    void typeTest();
    void placeTest();
};

tst_QPlaceDetailsReply::tst_QPlaceDetailsReply()
{
}

void tst_QPlaceDetailsReply::constructorTest()
{
    TestDetailsReply *reply = new TestDetailsReply(this);
    QVERIFY(reply->parent() == this);
    delete reply;
}

void tst_QPlaceDetailsReply::typeTest()
{
    TestDetailsReply *reply = new TestDetailsReply(this);
    QCOMPARE(reply->type(), QPlaceReply::DetailsReply);
    delete reply;
}

void tst_QPlaceDetailsReply::placeTest()
{
    TestDetailsReply *reply = new TestDetailsReply(this);
    QPlace place;
    place.setName(QStringLiteral("Gotham City"));
    reply->setPlace(place);

    QCOMPARE(reply->place(), place);
    delete reply;
}

QTEST_APPLESS_MAIN(tst_QPlaceDetailsReply)

#include "tst_qplacedetailsreply.moc"
