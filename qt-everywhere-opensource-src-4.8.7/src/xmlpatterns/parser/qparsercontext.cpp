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

#include <QtGlobal>

#include "qexpression_p.h"
#include "qstaticcontext_p.h"
#include "qtokenizer_p.h"

#include "qparsercontext_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

ParserContext::ParserContext(const StaticContext::Ptr &context,
                             const QXmlQuery::QueryLanguage lang,
                             Tokenizer *const tokener) : staticContext(context)
                                                       , tokenizer(tokener)
                                                       , languageAccent(lang)
                                                       , nodeTestSource(BuiltinTypes::element)
                                                       , moduleNamespace(StandardNamespaces::empty)
                                                       , isPreviousEnclosedExpr(false)
                                                       , elementConstructorDepth(0)
                                                       , hasSecondPrologPart(false)
                                                       , preserveNamespacesMode(true)
                                                       , inheritNamespacesMode(true)
                                                       , isParsingPattern(false)
                                                       , currentImportPrecedence(1)
                                                       , m_evaluationCacheSlot(-1)
                                                       , m_expressionSlot(0)
                                                       , m_positionSlot(-1)
                                                       , m_globalVariableSlot(-1)
                                                       , m_currentTemplateID(InitialTemplateID)
{
    resolvers.push(context->namespaceBindings());
    Q_ASSERT(tokenizer);
    Q_ASSERT(context);
    m_isParsingWithParam.push(false);
    isBackwardsCompat.push(false);
}

void ParserContext::finalizePushedVariable(const int amount,
                                           const bool shouldPop)
{
    for(int i = 0; i < amount; ++i)
    {
        const VariableDeclaration::Ptr var(shouldPop ? variables.pop() : variables.top());
        Q_ASSERT(var);

        if(var->isUsed())
            continue;
        else
        {
            staticContext->warning(QtXmlPatterns::tr("The variable %1 is unused")
                                                     .arg(formatKeyword(var, staticContext->namePool())));
        }
    }
}

QT_END_NAMESPACE

