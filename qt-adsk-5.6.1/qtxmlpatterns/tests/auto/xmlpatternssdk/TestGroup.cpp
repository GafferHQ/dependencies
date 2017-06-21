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

#include <QColor>
#include <QPair>
#include <QVariant>

#include "TestGroup.h"

using namespace QPatternistSDK;

TestGroup::TestGroup(TreeItem *p) : m_parent(p)
{
}

QVariant TestGroup::data(const Qt::ItemDataRole role, int column) const
{
    if(role != Qt::DisplayRole && role != Qt::BackgroundRole && role != Qt::ToolTipRole)
        return QVariant();

    /* In ResultSummary, the first is the amount of passes and the second is the total. */
    const ResultSummary sum(resultSummary());
    const int failures = sum.second - sum.first;

    switch(role)
    {
        case Qt::DisplayRole:
        {

            switch(column)
            {
                case 0:
                    return title();
                case 1:
                    /* Passes. */
                    return QString::number(sum.first);
                case 2:
                    /* Failures. */
                    return QString::number(failures);
                case 3:
                    /* Total. */
                    return QString::number(sum.second);
                default:
                {
                    Q_ASSERT(false);
                    return QString();
                }
            }
        }
        case Qt::BackgroundRole:
        {
            switch(column)
            {
                case 1:
                {
                    if(sum.first)
                    {
                        /* Pass. */
                        return QColor(Qt::green);
                    }
                    else
                        return QVariant();
                }
                case 2:
                {
                    if(failures)
                    {
                        /* Failure. */
                        return QColor(Qt::red);
                    }
                    else
                        return QVariant();
                }
                default:
                    return QVariant();
            }
        }
        case Qt::ToolTipRole:
        {
            return description();
        }
        default:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO, "This shouldn't be reached");
            return QVariant();
        }
    }
}

void TestGroup::setNote(const QString &n)
{
    m_note = n;
}

QString TestGroup::note() const
{
    return m_note;
}

TreeItem *TestGroup::parent() const
{
    return m_parent;
}

// vim: et:ts=4:sw=4:sts=4
