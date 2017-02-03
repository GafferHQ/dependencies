/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qt3to4 porting application of the Qt Toolkit.
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

#ifndef TOKENREPLACEMENTS_H
#define TOKENREPLACEMENTS_H

#include "tokenengine.h"
#include "textreplacement.h"
#include <QStringList>
#include <QByteArray>

QT_BEGIN_NAMESPACE

void addLogSourceEntry(const QString &text, const TokenEngine::TokenContainer&, const int index);
void addLogWarning(const QString &text);

class TokenReplacement
{
public:

    virtual bool doReplace(const TokenEngine::TokenContainer& ,
                           int /*tokenIndex*/,
                           TextReplacements&){return false;};
    /*
        returns the replace key for this replacement. Every time a token matches the replace key,
        doReplace() will be called for this TokenReplacement.
    */
    virtual QByteArray getReplaceKey(){return QByteArray();};
    virtual ~TokenReplacement(){};
};

/*
    A TokenReplacement that change any token
*/
class GenericTokenReplacement : public TokenReplacement
{
public:
    GenericTokenReplacement(QByteArray oldToken, QByteArray newToken);
    bool doReplace(const TokenEngine::TokenContainer &tokenContainer,
                   int tokenIndex, TextReplacements &textReplacements);
    QByteArray getReplaceKey();
private:
    QByteArray oldToken;
    QByteArray newToken;
};

/*
    A TokenReplacement that changes tokens that specify class names.
    In some cases where the class name token is a part of a qualified name
    it is not correct to rename it. ex:

    QButton::toggleState

    Here it is wrong to rename QButton -> Q3Button, since there is
    a rule that says QButton::ToggleState -> QCheckBox::ToggleState,
    but no rule for Q3Button::ToggleState.
*/
class ClassNameReplacement : public TokenReplacement
{
public:
    ClassNameReplacement(QByteArray oldToken, QByteArray newToken);
    bool doReplace(const TokenEngine::TokenContainer &tokenContainer,
                   int tokenIndex, TextReplacements &textReplacements);
    QByteArray getReplaceKey();
private:
    QByteArray oldToken;
    QByteArray newToken;
};

/*
   Changes scoped tokens:
   AA::BB -> CC::DD
   oldToken corresponds to the AA::BB part, newToken corresponds CC::DD.
   Since this is a token replacement, the AA part of oldToken is typically
   unknown. This means that we might change tokens named BB that does not belong
   to the AA scope. Ast replacemnts will fix this.

*/
class ScopedTokenReplacement : public TokenReplacement
{
public:
    ScopedTokenReplacement(const QByteArray &oldToken, const QByteArray &newToken);
    bool doReplace(const TokenEngine::TokenContainer &tokenContainer,
                   int tokenIndex, TextReplacements &textReplacements);
    QByteArray getReplaceKey();
private:
    QByteArray oldName;
    QByteArray oldScope;
    QByteArray newName;
    QByteArray newScope;
    QByteArray newScopedName;
    bool strictMode;
};

class QualifiedNameParser
{
public:
    QualifiedNameParser(const TokenEngine::TokenContainer &tokenContainer,
                        const int tokenIndex);
    enum Direction { Left=-1, Right=1 };
    bool isPartOfQualifiedName();
    bool isValidIndex(int index);
    bool isQualifier();
    bool isName();
    int peek(Direction direction);
    int move(Direction direction);
private:
    int nextScopeToken(Direction direction);
    int findScopeOperator(Direction direction);
    const TokenEngine::TokenContainer tokenContainer;
    int currentIndex;
};

QT_END_NAMESPACE

#endif
