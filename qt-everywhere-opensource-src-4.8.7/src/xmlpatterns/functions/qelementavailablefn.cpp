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

#include "qqnameconstructor_p.h"

#include "qelementavailablefn_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ElementAvailableFN::ElementAvailableFN() : m_xsltInstructions(allXSLTInstructions())
{
}

QSet<QString> ElementAvailableFN::allXSLTInstructions()
{
    enum
    {
        StringSetSize = 27
    };

    QSet<QString> retval;
    retval.reserve(StringSetSize);

    /* Alphabetically. */
    retval.insert(QLatin1String("analyze-string"));
    retval.insert(QLatin1String("apply-imports"));
    retval.insert(QLatin1String("apply-templates"));
    retval.insert(QLatin1String("attribute"));
    retval.insert(QLatin1String("attribute-set"));
    retval.insert(QLatin1String("call-template"));
    retval.insert(QLatin1String("character-map"));
    retval.insert(QLatin1String("choose"));
    retval.insert(QLatin1String("comment"));
    retval.insert(QLatin1String("copy"));
    retval.insert(QLatin1String("copy-of"));
    retval.insert(QLatin1String("document"));
    retval.insert(QLatin1String("element"));
    retval.insert(QLatin1String("fallback"));
    retval.insert(QLatin1String("for-each"));
    retval.insert(QLatin1String("for-each-group"));
    retval.insert(QLatin1String("if"));
    retval.insert(QLatin1String("message"));
    retval.insert(QLatin1String("namespace"));
    retval.insert(QLatin1String("next-match"));
    retval.insert(QLatin1String("number"));
    retval.insert(QLatin1String("perform-sort"));
    retval.insert(QLatin1String("processing-instruction"));
    retval.insert(QLatin1String("result-document"));
    retval.insert(QLatin1String("sequence"));
    retval.insert(QLatin1String("text"));
    retval.insert(QLatin1String("variable"));

    Q_ASSERT(retval.count() == StringSetSize);
    return retval;
}

bool ElementAvailableFN::evaluateEBV(const DynamicContext::Ptr &context) const
{
    const Item arg(m_operands.first()->evaluateSingleton(context));
    const QString stringName(arg.stringValue());

    const QXmlName elementName(QNameConstructor::expandQName<DynamicContext::Ptr,
                                                             ReportContext::XTDE1440,
                                                             ReportContext::XTDE1440>(stringName,
                                                                                      context,
                                                                                      staticNamespaces(),
                                                                                      this,
                                                                                      false));

    if(elementName.namespaceURI() != StandardNamespaces::xslt)
        return false;

    QString prefix;
    QString localName;
    XPathHelper::splitQName(stringName, prefix, localName);

    return m_xsltInstructions.contains(localName);
}

QT_END_NAMESPACE
