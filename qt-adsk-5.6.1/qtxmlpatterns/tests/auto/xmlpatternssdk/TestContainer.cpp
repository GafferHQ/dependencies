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

#include <QPair>
#include <QtDebug>

#include "TestContainer.h"

using namespace QPatternistSDK;

TestContainer::TestContainer() : m_deleteChildren(true)
{
}

TestContainer::~TestContainer()
{
    if(m_deleteChildren)
        qDeleteAll(m_children);
}

TestResult::List TestContainer::execute(const ExecutionStage stage,
                                        TestSuite *ts)
{
    Q_ASSERT(ts);
    const unsigned int c = m_children.count();
    TestResult::List result;

    for(unsigned int i = 0; i != c; ++i)
        result += static_cast<TestItem *>(child(i))->execute(stage, ts);

    return result;
}

TestItem::ResultSummary TestContainer::resultSummary() const
{
    const int c = childCount();
    int total = 0;
    int pass = 0;

    for(int i = 0; i != c; ++i)
    {
        TestItem *t = static_cast<TestItem *>(child(i));
        const ResultSummary sum(t->resultSummary());
        pass += sum.first;
        total += sum.second;
    }

    return ResultSummary(pass, total);
}

TreeItem::List TestContainer::children() const
{
    return m_children;
}

void TestContainer::appendChild(TreeItem *item)
{
    /* When one of our children changes, we changes. */
    connect(item, SIGNAL(changed(TreeItem *)), SIGNAL(changed(TreeItem *)));
    m_children.append(item);
}

TreeItem *TestContainer::child(const unsigned int rowP) const
{
    return m_children.value(rowP);
}

unsigned int TestContainer::childCount() const
{
    return m_children.count();
}

void TestContainer::setTitle(const QString &titleP)
{
    m_title = titleP;
}

QString TestContainer::title() const
{
    return m_title;
}

bool TestContainer::isFinalNode() const
{
    return false;
}

int TestContainer::columnCount() const
{
    return 4;
}

QString TestContainer::description() const
{
    return m_description;
}

void TestContainer::setDescription(const QString &desc)
{
    m_description = desc;
}

void TestContainer::setDeleteChildren(const bool val)
{
    m_deleteChildren = val;
}

void TestContainer::removeLast()
{
    m_children.removeLast();
}

// vim: et:ts=4:sw=4:sts=4
