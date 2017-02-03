/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

/*
  atom.h
*/

#ifndef ATOM_H
#define ATOM_H

#include <qstringlist.h>

#define QDOC_QML

QT_BEGIN_NAMESPACE

class Atom
{
 public:
    enum Type { 
        AbstractLeft,
        AbstractRight,
        AnnotatedList,
        AutoLink,
        BaseName,
        BriefLeft,
        BriefRight,
        C,
        CaptionLeft,
        CaptionRight,
        Code,
        CodeBad,
        CodeNew,
        CodeOld,
        CodeQuoteArgument,
        CodeQuoteCommand,
        DivLeft,
        DivRight,
        EndQmlText,
        FootnoteLeft,
        FootnoteRight,
        FormatElse,
        FormatEndif,
        FormatIf,
        FormattingLeft,
        FormattingRight,
        GeneratedList,
        GuidLink,
        Image,
        ImageText,
        InlineImage,
        JavaScript,
        EndJavaScript,
        LegaleseLeft,
        LegaleseRight,
        LineBreak,
        Link,
        LinkNode,
        ListLeft,
        ListItemNumber,
        ListTagLeft,
        ListTagRight,
        ListItemLeft,
        ListItemRight,
        ListRight,
        Nop,
        ParaLeft,
        ParaRight,
        Qml,
        QmlText,
        QuotationLeft,
        QuotationRight,
        RawString,
        SectionLeft,
        SectionRight,
        SectionHeadingLeft,
        SectionHeadingRight,
        SidebarLeft,
        SidebarRight,
        SinceList,
        SnippetCommand,
        SnippetIdentifier,
        SnippetLocation,
        String,
        TableLeft,
        TableRight,
        TableHeaderLeft,
        TableHeaderRight,
        TableRowLeft,
        TableRowRight,
        TableItemLeft,
        TableItemRight,
        TableOfContents,
        Target,
        UnhandledFormat, 
        UnknownCommand,
        Last = UnknownCommand
    };

    Atom(Type type, const QString& string = "")
	: nxt(0), typ(type) 
    {
        strs << string; 
    }

    Atom(Type type, const QString& p1, const QString& p2)
	: nxt(0), typ(type) 
    { 
        strs << p1; 
        if (!p2.isEmpty()) 
            strs << p2; 
    }

    Atom(Atom* prev, Type type, const QString& string = "")
	: nxt(prev->nxt), typ(type) 
    { 
        strs << string; 
        prev->nxt = this; 
    }
    
    Atom(Atom* prev, Type type, const QString& p1, const QString& p2)
	: nxt(prev->nxt), typ(type) 
    { 
        strs << p1; 
        if (!p2.isEmpty()) 
            strs << p2; 
        prev->nxt = this; 
    }

    void appendChar(QChar ch) { strs[0] += ch; }
    void appendString(const QString& string) { strs[0] += string; }
    void chopString() { strs[0].chop(1); }
    void setString(const QString& string) { strs[0] = string; }
    Atom* next() { return nxt; }
    void setNext(Atom* newNext) { nxt = newNext; }

    const Atom* next() const { return nxt; }
    const Atom* next(Type t) const;
    const Atom* next(Type t, const QString& s) const;
    Type type() const { return typ; }
    QString typeString() const;
    const QString& string() const { return strs[0]; }
    const QString& string(int i) const { return strs[i]; }
    int count() const { return strs.size(); }
    void dump() const;

    static QString BOLD_;
    static QString INDEX_;
    static QString ITALIC_;
    static QString LINK_;
    static QString PARAMETER_;
    static QString SPAN_;
    static QString SUBSCRIPT_;
    static QString SUPERSCRIPT_;
    static QString TELETYPE_;
    static QString UNDERLINE_;

    static QString BULLET_;
    static QString TAG_;
    static QString VALUE_;
    static QString LOWERALPHA_;
    static QString LOWERROMAN_;
    static QString NUMERIC_;
    static QString UPPERALPHA_;
    static QString UPPERROMAN_;

 private:
    Atom* nxt;
    Type typ;
    QStringList strs;
};

#define ATOM_FORMATTING_BOLD            "bold"
#define ATOM_FORMATTING_INDEX           "index"
#define ATOM_FORMATTING_ITALIC          "italic"
#define ATOM_FORMATTING_LINK            "link"
#define ATOM_FORMATTING_PARAMETER       "parameter"
#define ATOM_FORMATTING_SPAN            "span "
#define ATOM_FORMATTING_SUBSCRIPT       "subscript"
#define ATOM_FORMATTING_SUPERSCRIPT     "superscript"
#define ATOM_FORMATTING_TELETYPE        "teletype"
#define ATOM_FORMATTING_UNDERLINE       "underline"

#define ATOM_LIST_BULLET                "bullet"
#define ATOM_LIST_TAG                   "tag"
#define ATOM_LIST_VALUE                 "value"
#define ATOM_LIST_LOWERALPHA            "loweralpha"
#define ATOM_LIST_LOWERROMAN            "lowerroman"
#define ATOM_LIST_NUMERIC               "numeric"
#define ATOM_LIST_UPPERALPHA            "upperalpha"
#define ATOM_LIST_UPPERROMAN            "upperroman"

QT_END_NAMESPACE

#endif
