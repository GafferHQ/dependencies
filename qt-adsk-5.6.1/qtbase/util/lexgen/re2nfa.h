/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the utils of the Qt Toolkit.
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

#ifndef RE2NFA_H
#define RE2NFA_H

#include "nfa.h"
#include <QSet>

class RE2NFA
{
public:
    RE2NFA(const QMap<QString, NFA> &macros, const QSet<InputType> &maxInputSet, Qt::CaseSensitivity cs);

    NFA parse(const QString &expression, int *errorColumn = 0);

private:
    NFA parseExpr();
    NFA parseBranch();
    NFA parsePiece();
    NFA parseAtom();
    NFA parseMaybeQuantifier(const NFA &nfa);
    NFA parseSet();
    NFA parseSet2();

    NFA createCharNFA();

private:
    friend class RegExpTokenizer;

    enum Token {
        TOK_INVALID,
        TOK_STRING,
        TOK_LBRACE,   // {
        TOK_RBRACE,   // }
        TOK_LBRACKET, // [
        TOK_RBRACKET, // ]
        TOK_LPAREN,   // (
        TOK_RPAREN,   // )
        TOK_COMMA,
        TOK_STAR,
        TOK_OR,
        TOK_QUESTION,
        TOK_DOT,
        TOK_PLUS,
        TOK_SEQUENCE,
        TOK_QUOTED_STRING
    };

    struct Symbol
    {
        inline Symbol() : token(TOK_INVALID), column(-1) {}
        inline Symbol(Token t, const QString &l = QString()) : token(t), lexem(l), column(-1) {}
        Token token;
        QString lexem;
        int column;
    };

    inline bool hasNext() const { return index < symbols.count(); }
    inline Token next() { return symbols.at(index++).token; }
    bool next(Token t);
    bool test(Token t);
    inline void prev() { index--; }
    inline const Symbol &symbol() const { return symbols.at(index - 1); }
    QString lexemUntil(Token t);

    void tokenize(const QString &input);

    QMap<QString, NFA> macros;
    QVector<Symbol> symbols;
    int index;
    int errorColumn;
    const QSet<InputType> maxInputSet;
    Qt::CaseSensitivity caseSensitivity;
};

#endif // RE2NFA_H

