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

#include "qabstractfloatmathematician_p.h"
#include "qatomicmathematicianlocators_p.h"
#include "qatomicmathematicians_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

#define implMathVisit(ownerClass, visitor, mather, validOps)                            \
AtomicTypeVisitorResult::Ptr                                                            \
ownerClass##MathematicianLocator::visit(const visitor *, const qint16 op,               \
                                        const SourceLocationReflection *const r) const  \
{                                                                                       \
    Q_UNUSED(r)                                                                         \
    /* Note the extra paranteses around validOps. */                                    \
    if(((validOps) & AtomicMathematician::Operator(op)) == op)                                                         \
        return AtomicTypeVisitorResult::Ptr(new mather());                              \
    else                                                                                \
        return AtomicTypeVisitorResult::Ptr();                                          \
}

#define implReportingMathVisit(ownerClass, visitor, mather, validOps)                   \
AtomicTypeVisitorResult::Ptr                                                            \
ownerClass##MathematicianLocator::visit(const visitor *, const qint16 op,               \
                                        const SourceLocationReflection *const r) const  \
{                                                                                       \
    /* Note the extra paranteses around validOps. */                                    \
    if(((validOps) & AtomicMathematician::Operator(op)) == op)                                                         \
        return AtomicTypeVisitorResult::Ptr(new mather(r));                             \
    else                                                                                \
        return AtomicTypeVisitorResult::Ptr();                                          \
}

#define implRevReportingMathVisit(ownerClass, visitor, mather, validOps)                \
AtomicTypeVisitorResult::Ptr                                                            \
ownerClass##MathematicianLocator::visit(const visitor *, const qint16 op,               \
                                        const SourceLocationReflection *const r) const  \
{                                                                                       \
    /* Note the extra paranteses around validOps. */                                    \
    if(((validOps) & AtomicMathematician::Operator(op)) == op)                                                         \
        return AtomicTypeVisitorResult::Ptr(new OperandSwitcherMathematician(           \
                                            AtomicMathematician::Ptr(new mather(r))));  \
    else                                                                                \
        return AtomicTypeVisitorResult::Ptr();                                          \
}

static const AtomicMathematician::Operators AllMathOperators(AtomicMathematician::Add       |
                                                             AtomicMathematician::Div       |
                                                             AtomicMathematician::IDiv      |
                                                             AtomicMathematician::Mod       |
                                                             AtomicMathematician::Multiply  |
                                                             AtomicMathematician::Substract);

static const AtomicMathematician::Operators DivMultiply(AtomicMathematician::Multiply       |
                                                        AtomicMathematician::Div);

static const AtomicMathematician::Operators DurationOps(AtomicMathematician::Div            |
                                                        AtomicMathematician::Substract      |
                                                        AtomicMathematician::Add);

static const AtomicMathematician::Operators DTOps(AtomicMathematician::Substract            |
                                                  AtomicMathematician::Add);

implReportingMathVisit(Double,           DecimalType,            DoubleMathematician,    AllMathOperators)
implReportingMathVisit(Double,           DoubleType,             DoubleMathematician,    AllMathOperators)
implReportingMathVisit(Double,           FloatType,              DoubleMathematician,    AllMathOperators)
implReportingMathVisit(Double,           IntegerType,            DoubleMathematician,    AllMathOperators)
implRevReportingMathVisit(Double,           YearMonthDurationType,  DurationNumericMathematician,  AtomicMathematician::Multiply)
implRevReportingMathVisit(Double,           DayTimeDurationType,    DurationNumericMathematician,  AtomicMathematician::Multiply)

implReportingMathVisit(Float,            DecimalType,            FloatMathematician,     AllMathOperators)
implReportingMathVisit(Float,            DoubleType,    DoubleMathematician,    AllMathOperators)
implReportingMathVisit(Float,            FloatType,              FloatMathematician,     AllMathOperators)
implReportingMathVisit(Float,            IntegerType,            FloatMathematician,     AllMathOperators)
implRevReportingMathVisit(Float,            YearMonthDurationType,  DurationNumericMathematician,  AtomicMathematician::Multiply)
implRevReportingMathVisit(Float,            DayTimeDurationType,    DurationNumericMathematician,  AtomicMathematician::Multiply)

implReportingMathVisit(Decimal, DecimalType,            DecimalMathematician,   AllMathOperators)
implReportingMathVisit(Decimal,          DoubleType,    DoubleMathematician,    AllMathOperators)
implReportingMathVisit(Decimal,          FloatType,              FloatMathematician,     AllMathOperators)
implReportingMathVisit(Decimal, IntegerType,            DecimalMathematician,   AllMathOperators)
implRevReportingMathVisit(Decimal,          YearMonthDurationType,  DurationNumericMathematician,  AtomicMathematician::Multiply)
implRevReportingMathVisit(Decimal,          DayTimeDurationType,    DurationNumericMathematician,  AtomicMathematician::Multiply)

implReportingMathVisit(Integer, DecimalType,            DecimalMathematician,   AllMathOperators)
implReportingMathVisit(Integer,          DoubleType,    DoubleMathematician,    AllMathOperators)
implReportingMathVisit(Integer,          FloatType,              FloatMathematician,     AllMathOperators)
implReportingMathVisit(Integer, IntegerType,            IntegerMathematician,   AllMathOperators)
implRevReportingMathVisit(Integer,          YearMonthDurationType,  DurationNumericMathematician,  AtomicMathematician::Multiply)
implRevReportingMathVisit(Integer,          DayTimeDurationType,    DurationNumericMathematician,  AtomicMathematician::Multiply)

implRevReportingMathVisit(DayTimeDuration,  DateTimeType,           DateTimeDurationMathematician,       AtomicMathematician::Add)
implRevReportingMathVisit(DayTimeDuration,  DateType,               DateTimeDurationMathematician,       AtomicMathematician::Add)
implMathVisit(DayTimeDuration,  DayTimeDurationType,    DurationDurationMathematician, DurationOps)
implReportingMathVisit(DayTimeDuration,  DecimalType,   DurationNumericMathematician,  DivMultiply)
implReportingMathVisit(DayTimeDuration,  DoubleType,    DurationNumericMathematician,  DivMultiply)
implReportingMathVisit(DayTimeDuration,  FloatType,     DurationNumericMathematician,  DivMultiply)
implReportingMathVisit(DayTimeDuration,  IntegerType,   DurationNumericMathematician,  DivMultiply)
implRevReportingMathVisit(DayTimeDuration,  SchemaTimeType,               DateTimeDurationMathematician,       AtomicMathematician::Add)

implRevReportingMathVisit(YearMonthDuration, DateTimeType,          DateTimeDurationMathematician,       AtomicMathematician::Add)
implRevReportingMathVisit(YearMonthDuration, DateType,              DateTimeDurationMathematician,       AtomicMathematician::Add)
implReportingMathVisit(YearMonthDuration, DecimalType,  DurationNumericMathematician,  DivMultiply)
implReportingMathVisit(YearMonthDuration, DoubleType,   DurationNumericMathematician,  DivMultiply)
implReportingMathVisit(YearMonthDuration, FloatType,    DurationNumericMathematician,  DivMultiply)
implReportingMathVisit(YearMonthDuration, IntegerType,  DurationNumericMathematician,  DivMultiply)
implMathVisit(YearMonthDuration, YearMonthDurationType, DurationDurationMathematician, DurationOps)

implMathVisit(Date,              DateType,              AbstractDateTimeMathematician,
              AtomicMathematician::Substract)
implReportingMathVisit(Date,     YearMonthDurationType, DateTimeDurationMathematician,       DTOps)
implReportingMathVisit(Date,     DayTimeDurationType,   DateTimeDurationMathematician,       DTOps)

implMathVisit(SchemaTime,              SchemaTimeType,              AbstractDateTimeMathematician,
              AtomicMathematician::Substract)
implReportingMathVisit(SchemaTime,     DayTimeDurationType,   DateTimeDurationMathematician,       DTOps)

implMathVisit(DateTime,          DateTimeType,          AbstractDateTimeMathematician,
              AtomicMathematician::Substract)
implReportingMathVisit(DateTime, YearMonthDurationType, DateTimeDurationMathematician,       DTOps)
implReportingMathVisit(DateTime, DayTimeDurationType,   DateTimeDurationMathematician,       DTOps)

#undef implMathVisit
#undef implReportingMathVisit
#undef implRevReportingMathVisit

QT_END_NAMESPACE
