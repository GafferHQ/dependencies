/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcommonsequencetypes_p.h"
#include "qexpressionsequence_p.h"
#include "qsorttuple_p.h"

#include "qreturnorderby_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ReturnOrderBy::ReturnOrderBy(const OrderBy::Stability aStability,
                             const OrderBy::OrderSpec::Vector &oSpecs,
                             const Expression::List &ops) : UnlimitedContainer(ops)
                                                          , m_stability(aStability)
                                                          , m_orderSpecs(oSpecs)
                                                          , m_flyAway(true)
{
    Q_ASSERT_X(m_operands.size() >= 2, Q_FUNC_INFO,
               "ReturnOrderBy must have the return expression, and at least one sort key.");
    Q_ASSERT(m_orderSpecs.size() == ops.size() - 1);
}

Item ReturnOrderBy::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    Q_ASSERT(m_operands.size() > 1);
    const Item::Iterator::Ptr value(makeListIterator(m_operands.first()->evaluateSequence(context)->toList()));
    Item::Vector sortKeys;

    /* We're skipping the first operand. */
    const int len = m_operands.size() - 1;
    sortKeys.resize(len);

    for(int i = 1; i <= len; ++i)
        sortKeys[i - 1] = m_operands.at(i)->evaluateSingleton(context);

    return Item(new SortTuple(value, sortKeys));
}

bool ReturnOrderBy::evaluateEBV(const DynamicContext::Ptr &context) const
{
    // TODO This is temporary code.
    return m_operands.first()->evaluateEBV(context);
}

Expression::Ptr ReturnOrderBy::compress(const StaticContext::Ptr &context)
{
    /* We first did this in typeCheck(), but that broke due to that type checks were
     * missed, which other pieces relied on. */
    if(m_flyAway)
    {
        /* We only want the return expression, not the sort keys. */
        return m_operands.first()->compress(context);
    }
    else
    {
        /* We don't need the members, so don't keep a reference to them. */
        m_orderSpecs.clear();

        return UnlimitedContainer::compress(context);
    }
}

Expression::Properties ReturnOrderBy::properties() const
{
    /* For some unknown reason this is necessary for XQTS test case orderBy18. */
    return DisableElimination;
}

ExpressionVisitorResult::Ptr ReturnOrderBy::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

SequenceType::Ptr ReturnOrderBy::staticType() const
{
    return m_operands.first()->staticType();
}

SequenceType::List ReturnOrderBy::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    result.append(CommonSequenceTypes::ZeroOrOneAtomicType);
    return result;
}

Expression::ID ReturnOrderBy::id() const
{
    return IDReturnOrderBy;
}

QT_END_NAMESPACE
