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


#include "qexternalvariablereference_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ExternalVariableReference::ExternalVariableReference(const QXmlName &name,
                                                     const SequenceType::Ptr &type) : m_name(name),
                                                                                      m_seqType(type)
{
    Q_ASSERT(!m_name.isNull());
    Q_ASSERT(m_seqType);
}

Item::Iterator::Ptr ExternalVariableReference::evaluateSequence(const DynamicContext::Ptr &context) const
{
    Q_ASSERT(context->externalVariableLoader());
    return context->externalVariableLoader()->evaluateSequence(m_name, context);
}

Item ExternalVariableReference::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    return context->externalVariableLoader()->evaluateSingleton(m_name, context);
}

bool ExternalVariableReference::evaluateEBV(const DynamicContext::Ptr &context) const
{
    return context->externalVariableLoader()->evaluateEBV(m_name, context);
}

SequenceType::Ptr ExternalVariableReference::staticType() const
{
    return m_seqType;
}

Expression::Properties ExternalVariableReference::properties() const
{
    return DisableElimination;
}

ExpressionVisitorResult::Ptr ExternalVariableReference::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

QT_END_NAMESPACE
