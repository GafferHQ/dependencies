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

#include "qboolean_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonsequencetypes_p.h"
#include "qgenericsequencetype_p.h"

#include "qliteral_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Literal::Literal(const Item &i) : m_item(i)
{
    Q_ASSERT(m_item);
    Q_ASSERT(m_item.isAtomicValue());
}

Item Literal::evaluateSingleton(const DynamicContext::Ptr &) const
{
    return m_item;
}

bool Literal::evaluateEBV(const DynamicContext::Ptr &context) const
{
    return Boolean::evaluateEBV(m_item, context);
}

void Literal::evaluateToSequenceReceiver(const DynamicContext::Ptr &context) const
{
    context->outputReceiver()->item(m_item);
}

SequenceType::Ptr Literal::staticType() const
{
    return makeGenericSequenceType(m_item.type(), Cardinality::exactlyOne());
}

ExpressionVisitorResult::Ptr Literal::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::ID Literal::id() const
{
    Q_ASSERT(m_item);
    Q_ASSERT(m_item.isAtomicValue());
    const ItemType::Ptr t(m_item.type());

    if(BuiltinTypes::xsBoolean->xdtTypeMatches(t))
        return IDBooleanValue;
    else if(BuiltinTypes::xsString->xdtTypeMatches(t) ||
            BuiltinTypes::xsAnyURI->xdtTypeMatches(t) ||
            BuiltinTypes::xsUntypedAtomic->xdtTypeMatches(t))
    {
        return IDStringValue;
    }
    else if(BuiltinTypes::xsInteger->xdtTypeMatches(t))
        return IDIntegerValue;
    else
        return IDFloat;
}

Expression::Properties Literal::properties() const
{
    return IsEvaluated;
}

QString Literal::description() const
{
    return m_item.stringValue();
}

QT_END_NAMESPACE
