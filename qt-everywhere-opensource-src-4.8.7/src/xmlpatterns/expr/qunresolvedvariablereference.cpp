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

#include "qunresolvedvariablereference_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

UnresolvedVariableReference::UnresolvedVariableReference(const QXmlName &name) : m_name(name)
{
    Q_ASSERT(!m_name.isNull());
}

Expression::Ptr UnresolvedVariableReference::typeCheck(const StaticContext::Ptr &context,
                                                       const SequenceType::Ptr &reqType)
{
    /* We may be called before m_replacement is called, when we're part of a
     * function body whose type checking is performed for. See
     * UserFunctionCallsite::typeCheck(). */
    if(m_replacement)
        return m_replacement->typeCheck(context, reqType);
    else
        return EmptyContainer::typeCheck(context, reqType);
}

SequenceType::Ptr UnresolvedVariableReference::staticType() const
{
    /* We may be called by xmlpatternsview before the typeCheck() stage. */
    if(m_replacement)
        return m_replacement->staticType();
    else
        return CommonSequenceTypes::ZeroOrMoreItems;
}

SequenceType::List UnresolvedVariableReference::expectedOperandTypes() const
{
    return SequenceType::List();
}

ExpressionVisitorResult::Ptr UnresolvedVariableReference::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

Expression::ID UnresolvedVariableReference::id() const
{
    return IDUnresolvedVariableReference;
}

QT_END_NAMESPACE
