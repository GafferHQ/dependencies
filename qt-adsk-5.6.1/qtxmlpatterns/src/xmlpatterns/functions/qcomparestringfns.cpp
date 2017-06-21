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

#include "qcommonnamespaces_p.h"

#include "qboolean_p.h"
#include "qcommonvalues_p.h"
#include "qinteger_p.h"
#include "qatomicstring_p.h"

#include "qcomparestringfns_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Item CodepointEqualFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item op1(m_operands.first()->evaluateSingleton(context));
    if(!op1)
        return Item();

    const Item op2(m_operands.last()->evaluateSingleton(context));
    if(!op2)
        return Item();

    if(caseSensitivity() == Qt::CaseSensitive)
        return Boolean::fromValue(op1.stringValue() == op2.stringValue());
    else
    {
        const QString s1(op1.stringValue());
        const QString s2(op2.stringValue());

        return Boolean::fromValue(s1.length() == s2.length() &&
                                  s1.startsWith(s2, Qt::CaseInsensitive));
    }
}

Item CompareFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item op1(m_operands.first()->evaluateSingleton(context));
    if(!op1)
        return Item();

    const Item op2(m_operands.at(1)->evaluateSingleton(context));
    if(!op2)
        return Item();

    const int retval = caseSensitivity() == Qt::CaseSensitive
                       ? op1.stringValue().compare(op2.stringValue())
                       : op1.stringValue().toLower().compare(op2.stringValue().toLower());

    if(retval > 0)
        return CommonValues::IntegerOne;
    else if(retval < 0)
        return CommonValues::IntegerOneNegative;
    else
    {
        Q_ASSERT(retval == 0);
        return CommonValues::IntegerZero;
    }
}

QT_END_NAMESPACE
