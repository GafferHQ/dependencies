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

#include "qcommonsequencetypes_p.h"
#include "qtypechecker_p.h"

#include "qtreatas_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

TreatAs::TreatAs(const Expression::Ptr &operand,
                 const SequenceType::Ptr &reqType) : SingleContainer(operand),
                                                     m_reqType(reqType)
{
    Q_ASSERT(reqType);
}

Expression::Ptr TreatAs::typeCheck(const StaticContext::Ptr &context,
                                   const SequenceType::Ptr &reqType)
{
    Q_ASSERT(context);
    Q_ASSERT(reqType);

    /* Apply function conversion with the special error code XPDY0050. After that, we
     * let the regular typeCheck() function be invoked on the operand before we rewrite
     * to it. Hence is applyFunctionConversion() called twice, which doesn't break anything,
     * but indeed is redundant. */
    const Expression::Ptr treated(TypeChecker::applyFunctionConversion(m_operand,
                                                                       m_reqType,
                                                                       context,
                                                                       ReportContext::XPDY0050));
    return treated->typeCheck(context, reqType);
}

ExpressionVisitorResult::Ptr TreatAs::accept(const ExpressionVisitor::Ptr &visitor) const
{
    return visitor->visit(this);
}

SequenceType::Ptr TreatAs::staticType() const
{
    return m_reqType;
}

SequenceType::List TreatAs::expectedOperandTypes() const
{
    SequenceType::List result;
    result.append(CommonSequenceTypes::ZeroOrMoreItems);
    return result;
}

QT_END_NAMESPACE
