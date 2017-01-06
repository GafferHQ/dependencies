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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef Patternist_TokenSource_H
#define Patternist_TokenSource_H

#include "qatomiccomparator_p.h"
#include "qatomicmathematician_p.h"
#include "qcombinenodes_p.h"
#include "qfunctionargument_p.h"
#include "qitem_p.h"
#include "qitemtype_p.h"
#include "qorderby_p.h"
#include "qpath_p.h"
#include "qquerytransformparser_p.h"
#include "qvalidate_p.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

template<typename T> class QQueue;

namespace QPatternist
{
    /**
     * @short A union of all the enums the parser uses.
     */
    union EnumUnion
    {
        AtomicComparator::Operator              valueOperator;
        AtomicMathematician::Operator           mathOperator;
        CombineNodes::Operator                  combinedNodeOp;
        QXmlNodeModelIndex::Axis                axis;
        QXmlNodeModelIndex::DocumentOrder       nodeOperator;
        StaticContext::BoundarySpacePolicy      boundarySpacePolicy;
        StaticContext::ConstructionMode         constructionMode;
        StaticContext::OrderingEmptySequence    orderingEmptySequence;
        StaticContext::OrderingMode             orderingMode;
        OrderBy::OrderSpec::Direction           sortDirection;
        Validate::Mode                          validationMode;
        VariableSlotID                          slot;
        int                                     tokenizerPosition;
        qint16                                  zeroer;
        bool                                    Bool;
        xsDouble                                Double;
        Path::Kind                              pathKind;
    };

    /**
     * @short Base class for components that needs to return tokens.
     *
     * TokenSource represents a stream of Token instances. The end
     * is reached when readNext() returns a Token constructed with
     * END_OF_FILE.
     *
     * @see <a href="http://www.w3.org/TR/xquery-xpath-parsing/">Building a
     * Tokenizer for XPath or XQuery</a>
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class TokenSource : public QSharedData
    {
    public:
        /**
         * typedef for the enum Bison generates that contains
         * the token symbols.
         */
        typedef yytokentype TokenType;

        /**
         * Represents a token by carrying its name and value.
         */
        class Token
        {
        public:
            /**
             * Constructs an invalid Token. This default constructor
             * is need in Qt's container classes.
             */
            inline Token() {}
            inline Token(const TokenType t) : type(t) {}
            inline Token(const TokenType t, const QString &val) : type(t), value(val) {}

            bool hasError() const
            {
                return type == ERROR;
            }

            TokenType type;
            QString value;
        };

        typedef QExplicitlySharedDataPointer<TokenSource> Ptr;
        typedef QQueue<Ptr> Queue;

        /**
         * The C++ compiler cannot synthesize it when we use the
         * Q_DISABLE_COPY() macro.
         */
        inline TokenSource()
        {
        }

        virtual ~TokenSource();

        /**
         * @returns the next token.
         */

            virtual Token nextToken(YYLTYPE *const sourceLocator) = 0;

    private:
        Q_DISABLE_COPY(TokenSource)
    };
}

QT_END_NAMESPACE

QT_END_HEADER

#endif
