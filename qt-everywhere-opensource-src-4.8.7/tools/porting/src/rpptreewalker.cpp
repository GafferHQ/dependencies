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

#include "rpptreewalker.h"

QT_BEGIN_NAMESPACE

namespace Rpp {

void RppTreeWalker::evaluateItem(const Item *item)
{
    if(!item)
        return;
    if (Source *source = item->toSource())
        evaluateSource(source);
    else if (Directive *directive = item->toDirective())
        evaluateDirective(directive);
    else if (IfSection *ifSection = item->toIfSection())
        evaluateIfSection(ifSection);
    else if (Text *text = item->toText())
        evaluateText(text);
}

void RppTreeWalker::evaluateItemComposite(const ItemComposite *itemComposite)
{
    if (!itemComposite)
        return;
    for (int i = 0; i < itemComposite->count(); ++i) {
        evaluateItem(itemComposite->item(i));
    }
}

void RppTreeWalker::evaluateSource(const Source *source)
{
    evaluateItemComposite(source->toItemComposite());
}

void RppTreeWalker::evaluateDirective(const Directive *directive)
{
    if (!directive)
        return;
    if (EmptyDirective *dir = directive->toEmptyDirective())
        evaluateEmptyDirective(dir);
    else if (ErrorDirective *dir = directive->toErrorDirective())
        evaluateErrorDirective(dir);
    else if (PragmaDirective *dir = directive->toPragmaDirective())
        evaluatePragmaDirective(dir);
    else if (IncludeDirective *dir = directive->toIncludeDirective())
        evaluateIncludeDirective(dir);
    else if (DefineDirective *dir = directive->toDefineDirective())
        evaluateDefineDirective(dir);
    else if (UndefDirective *dir = directive->toUndefDirective())
        evaluateUndefDirective(dir);
    else if (LineDirective *dir = directive->toLineDirective())
        evaluateLineDirective(dir);
    else if (NonDirective *dir = directive->toNonDirective())
        evaluateNonDirective(dir);
    else if (NonDirective *dir = directive->toNonDirective())
        evaluateNonDirective(dir);
    else if (ConditionalDirective *dir = directive->toConditionalDirective())
        evaluateConditionalDirective(dir);
}

/*
    This function evaluates all the branches of an IfSection. You should 
    override it if you want to only evaluate the "correct" branch.
*/
void RppTreeWalker::evaluateIfSection(const IfSection *ifSection)
{
    if (!ifSection)
        return;
    evaluateItemComposite(ifSection->toItemComposite());
}

void RppTreeWalker::evaluateConditionalDirective(const ConditionalDirective *conditionalDirective)
{
    if (!conditionalDirective)
        return;
    if (IfdefDirective *dir = conditionalDirective->toIfdefDirective())
         evaluateIfdefDirective(dir);
    else if (IfndefDirective *dir = conditionalDirective->toIfndefDirective())
         evaluateIfndefDirective(dir);
    else if (IfDirective *dir = conditionalDirective->toIfDirective())
         evaluateIfDirective(dir);
    else if (ElifDirective *dir = conditionalDirective->toElifDirective())
         evaluateElifDirective(dir);
    else if (ElseDirective *dir = conditionalDirective->toElseDirective())
         evaluateElseDirective(dir);
}

void RppTreeWalker::evaluateIfdefDirective(const IfdefDirective *directive)
{
   if (!directive) 
       return;
   evaluateItemComposite(directive->toItemComposite());
}

void RppTreeWalker::evaluateIfndefDirective(const IfndefDirective *directive)
{
   if (!directive) 
       return;
   evaluateItemComposite(directive->toItemComposite());
}

void RppTreeWalker::evaluateIfDirective(const IfDirective *directive)
{
   if (!directive) 
       return;
   evaluateItemComposite(directive->toItemComposite());
}

void RppTreeWalker::evaluateElifDirective(const ElifDirective *directive)
{
   if (!directive) 
       return;
   evaluateItemComposite(directive->toItemComposite());
}

void RppTreeWalker::evaluateElseDirective(const ElseDirective *directive)
{
   if (!directive) 
       return;
   evaluateItemComposite(directive->toItemComposite());
}

}

QT_END_NAMESPACE
