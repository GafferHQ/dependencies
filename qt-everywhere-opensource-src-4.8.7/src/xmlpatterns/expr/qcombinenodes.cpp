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
#include "qexceptiterator_p.h"
#include "qgenericsequencetype_p.h"
#include "qintersectiterator_p.h"
#include "qitemmappingiterator_p.h"
#include "qnodesort_p.h"
#include "qunioniterator_p.h"

#include "qcombinenodes_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

CombineNodes::CombineNodes(const Expression::Ptr &operand1,
                           const Operator op,
                           const Expression::Ptr &operand2) : PairContainer(operand1, operand2),
                                                              m_operator(op)
{
    Q_ASSERT(op == Union    ||
             op == Except   ||
             op == Intersect);
}

Item::Iterator::Ptr CombineNodes::evaluateSequence(const DynamicContext::Ptr &context) const
{
    const Item::Iterator::Ptr op1(m_operand1->evaluateSequence(context));
    const Item::Iterator::Ptr op2(m_operand2->evaluateSequence(context));

    switch(m_operator)
    {
        case Intersect:
            return Item::Iterator::Ptr(new IntersectIterator(op1, op2));
        case Except:
            return Item::Iterator::Ptr(new ExceptIterator(op1, op2));
        default:
        {
            Q_ASSERT(m_operator == Union);
            return Item::Iterator::Ptr(new UnionIterator(op1, op2));
        }
    }
}

Item CombineNodes::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    return evaluateSequence(context)->next();
}

Expression::Ptr CombineNodes::typeCheck(const StaticContext::Ptr &context,
                                        const SequenceType::Ptr &reqType)
{

    const Expression::Ptr me(PairContainer::typeCheck(context, reqType));

    m_operand1 = NodeSortExpression::wrapAround(m_operand1, context);
    m_operand2 = NodeSortExpression::wrapAround(m_operand2, context);

    return me;
}

bool CombineNodes::evaluateEBV(const DynamicContext::Ptr &context) const
{
    /* If it's the union operator, we can possibly avoid
     * evaluating the second operand. */

    if(m_operator == Union)
    {
        return m_operand1->evaluateEBV(context) ||
               m_operand2->evaluateEBV(context);
    }
    else
        return PairContainer::evaluateEBV(context);
}

QString CombineNodes::displayName(const Operator op)
{
    switch(op)
    {
        case Intersect:
            return QLatin1String("intersect");
        case Except:
            return QLatin1String("except");
        default:
        {
            Q_ASSERT(op == Union);
            return QLatin1String("union");
        }
    }
}

SequenceType::Ptr CombineNodes::staticType() const
{
    const SequenceType::Ptr t1(m_operand1->staticType());
    const SequenceType::Ptr t2(m_operand2->staticType());

     Cardinality card;

     /* Optimization: the cardinality can be better inferred for
      * Intersect and Except, although it's not trivial code. */
     if(m_operator == Union)
         card = t1->cardinality() | t2->cardinality();
     else /* Except. */
         card = Cardinality::zeroOrMore();

     return makeGenericSequenceType(t1->itemType() | t2->itemType(), card);
}

SequenceType::List CombineNodes::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreNodes);
    result.append(CommonSequenceTypes::ZeroOrMoreNodes);
    return result;
}

CombineNodes::Operator CombineNodes::operatorID() const
{
    return m_operator;
}

ExpressionVisitorResult::Ptr CombineNodes::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::ID CombineNodes::id() const
{
    return IDCombineNodes;
}

QT_END_NAMESPACE
