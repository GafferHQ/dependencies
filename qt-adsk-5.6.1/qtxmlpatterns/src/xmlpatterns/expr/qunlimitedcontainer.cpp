/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "qunlimitedcontainer_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

UnlimitedContainer::UnlimitedContainer(const Expression::List &ops) : m_operands(ops)
{
}

void UnlimitedContainer::setOperands(const Expression::List &list)
{
    m_operands = list;
}

Expression::List UnlimitedContainer::operands() const
{
    return m_operands;
}

bool UnlimitedContainer::compressOperands(const StaticContext::Ptr &context)
{
    const Expression::List::iterator end(m_operands.end());
    Expression::List::iterator it(m_operands.begin());
    int evaled = 0;

    for(; it != end; ++it)
    {
        Q_ASSERT((*it));
        rewrite((*it), (*it)->compress(context), context);
        if((*it)->isEvaluated())
                ++evaled;
    }

    return evaled == m_operands.count();
}

QT_END_NAMESPACE
