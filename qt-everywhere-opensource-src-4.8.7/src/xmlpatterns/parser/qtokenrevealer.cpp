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

#include "qtokenrevealer_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

TokenRevealer::TokenRevealer(const QUrl &uri,
                              const Tokenizer::Ptr &other) : Tokenizer(uri)
                                                           , m_tokenizer(other)
{
    Q_ASSERT(other);
}

TokenRevealer::~TokenRevealer()
{
    qDebug() << "Tokens Revealed:" << m_result;
}

void TokenRevealer::setParserContext(const ParserContext::Ptr &parseInfo)
{
    m_tokenizer->setParserContext(parseInfo);
}

Tokenizer::Token TokenRevealer::nextToken(YYLTYPE *const sourceLocator)
{
    const Token token(m_tokenizer->nextToken(sourceLocator));
    const QString asString(tokenToString(token));
    const TokenType type = token.type;

    /* Indent. */
    switch(type)
    {
        case CURLY_LBRACE:
        {
            m_result += QLatin1Char('\n') + m_indentationString + asString + QLatin1Char('\n');
            m_indentationString.append(QLatin1String("    "));
            m_result += m_indentationString;
            break;
        }
        case CURLY_RBRACE:
        {
            m_indentationString.chop(4);
            m_result += QLatin1Char('\n') + m_indentationString + asString;
            break;
        }
        case SEMI_COLON:
        /* Fallthrough. */
        case COMMA:
        {
            m_result += asString + QLatin1Char('\n') + m_indentationString;
            break;
        }
        default:
            m_result += asString + QLatin1Char(' ');
    }

    return token;
}

int TokenRevealer::commenceScanOnly()
{
    return m_tokenizer->commenceScanOnly();
}

void TokenRevealer::resumeTokenizationFrom(const int position)
{
    m_tokenizer->resumeTokenizationFrom(position);
}

QT_END_NAMESPACE
