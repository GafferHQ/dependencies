/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

// Not really clean but as the QHandle(idx, counter) is private
// This allows to use this constructor in the tests
#define private public
#include <Qt3DCore/private/qhandle_p.h>

class tst_Handle : public QObject
{
    Q_OBJECT
public:
    tst_Handle() {}
    ~tst_Handle() {}

private slots:
    void defaultConstruction();
    void construction();
    void copyConstruction();
    void assignment();
    void equality();
    void inequality();
    void staticLimits();
    void bigHandle();
};

class SimpleResource
{
public:
    SimpleResource()
        : m_value(0)
    {}

    int m_value;
};

typedef Qt3DCore::QHandle<SimpleResource> Handle;
typedef Qt3DCore::QHandle<SimpleResource, 22> BigHandle;

void tst_Handle::defaultConstruction()
{
    Handle h;
    QVERIFY(h.isNull() == true);
    QVERIFY(h.index() == 0);
    QVERIFY(h.counter() == 0);
    QVERIFY(h.handle() == 0);
}

void tst_Handle::construction()
{
    Handle h(0, 1);
    QVERIFY(h.isNull() == false);
    QVERIFY(h.index() == 0);
    QVERIFY(h.counter() == 1);
    qDebug() << h;
    QVERIFY(h.handle() == 65536);

    Handle h2(1, 1);
    QVERIFY(h2.isNull() == false);
    QVERIFY(h2.index() == 1);
    QVERIFY(h2.counter() == 1);
    qDebug() << h2;
    QVERIFY(h2.handle() == 65537);
}

void tst_Handle::copyConstruction()
{
    Handle h1(0, 1);
    Handle h2(h1);
    QVERIFY(h2.isNull() == false);
    QVERIFY(h2.index() == 0);
    QVERIFY(h2.counter() == 1);
    QVERIFY(h2.handle() == 65536);
}

void tst_Handle::assignment()
{
    Handle h1(0, 1);
    Handle h2 = h1;
    QVERIFY(h2.isNull() == false);
    QVERIFY(h2.index() == 0);
    QVERIFY(h2.counter() == 1);
    QVERIFY(h2.handle() == 65536);
}

void tst_Handle::equality()
{
    Handle h1(2, 1);
    Handle h2(2, 1);
    QVERIFY(h1.isNull() == false);
    QVERIFY(h1.index() == 2);
    QVERIFY(h1.counter() == 1);
    QVERIFY(h1.handle() == 65538);
    QVERIFY(h1 == h2);
}

void tst_Handle::inequality()
{
    Handle h1(2, 1);
    Handle h2(3, 1);
    QVERIFY(h1.isNull() == false);
    QVERIFY(h1.index() == 2);
    QVERIFY(h1.counter() == 1);
    QVERIFY(h1.handle() == 65538);
    QVERIFY(h1 != h2);

    Handle h3(2, 2);
    QVERIFY(h1 != h3);
}

void tst_Handle::staticLimits()
{
    QVERIFY(Handle::maxIndex() == (1 << 16) - 1);
    QVERIFY(Handle::maxCounter() == (1 << (32 - 16 - 2)) - 1);
}

void tst_Handle::bigHandle()
{
    BigHandle h;
    QVERIFY(h.isNull() == true);
    QVERIFY(h.index() == 0);
    QVERIFY(h.counter() == 0);
    QVERIFY(h.handle() == 0);

    BigHandle h1(0, 1);
    QVERIFY(h1.isNull() == false);
    QVERIFY(h1.index() == 0);
    QVERIFY(h1.counter() == 1);
    QVERIFY(h1.handle() == 4194304);

    BigHandle h2(1, 1);
    QVERIFY(h2.isNull() == false);
    QVERIFY(h2.index() == 1);
    QVERIFY(h2.counter() == 1);
    QVERIFY(h2.handle() == 4194305);

    QVERIFY(BigHandle::maxIndex() == (1 << 22) - 1);
    QVERIFY(BigHandle::maxCounter() == (1 << (32 - 22 - 2)) - 1);
}

QTEST_APPLESS_MAIN(tst_Handle)

#include "tst_handle.moc"
