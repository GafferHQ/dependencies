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
#ifndef GENERATOR_H
#define GENERATOR_H

#include <QTextStream>
#include <QStringList>

#include "nfa.h"

class LineStream
{
private:
    struct SharedStream
    {
        int ref;
        QTextStream *stream;
    };

public:
    LineStream(QTextStream *textStream)
    {
        shared = new SharedStream;
        shared->ref = 1;
        shared->stream = textStream;
    }
    LineStream(const LineStream &other)
    {
        shared = other.shared;
        shared->ref++;
    }
    LineStream &operator=(const LineStream &other)
    {
        if (this == &other)
            return *this;
        LineStream copy(other); // keep refcount up
        qSwap(*shared, *other.shared);
        return *this;
    }
    ~LineStream()
    {
        if (!--shared->ref) {
            (*shared->stream) << endl;
            delete shared;
        }
    }

    template <typename T>
    LineStream &operator<<(const T &value)
    { (*shared->stream) << value; return *this; }

    SharedStream *shared;
};

class CodeBlock
{
public:
    inline CodeBlock() { stream.setString(&output, QIODevice::WriteOnly); }

    inline void indent() { indentStr += QLatin1String("    "); }
    inline void outdent() { indentStr.remove(0, 4); }

    template <typename T>
    LineStream operator<<(const T &value)
    { stream << indentStr; stream << value; return LineStream(&stream); }

    inline void addNewLine() { stream << endl; }

    inline QString toString() const { stream.flush(); return output; }

private:
    QString output;
    mutable QTextStream stream;
    QString indentStr;
};

class Function
{
public:
    inline Function(const QString &returnType, const QString &name)
        : rtype(returnType), fname(name), iline(false), cnst(false) {}
    inline Function() : iline(false), cnst(false) {}

    inline void setName(const QString &name) { fname = name; }
    inline QString name() const { return fname; }

    inline void setInline(bool i) { iline = i; }
    inline bool isInline() const { return iline; }

    inline void setReturnType(const QString &type) { rtype = type; }
    inline QString returnType() const { return rtype; }

    inline void addBody(const QString &_body) { body += _body; }
    inline void addBody(const CodeBlock &block) { body += block.toString(); }
    inline bool hasBody() const { return !body.isEmpty(); }

    inline void setConst(bool konst) { cnst = konst; }
    inline bool isConst() const { return cnst; }

    void printDeclaration(CodeBlock &block, const QString &funcNamePrefix = QString()) const;
    QString definition() const;

private:
    QString signature(const QString &funcNamePrefix = QString()) const;

    QString rtype;
    QString fname;
    QString body;
    bool iline;
    bool cnst;
};

class Class
{
public:
    enum Access { PublicMember, ProtectedMember, PrivateMember };

    inline Class(const QString &name) : cname(name) {}

    inline void setName(const QString &name) { cname = name; }
    inline QString name() const { return cname; }

    inline void addMember(Access access, const QString &name)
    { sections[access].variables.append(name); }
    inline void addMember(Access access, const Function &func)
    { sections[access].functions.append(func); }

    void addConstructor(Access access, const QString &body, const QString &args = QString());
    inline void addConstructor(Access access, const CodeBlock &body, const QString &args = QString())
    { addConstructor(access, body.toString(), args); }

    QString declaration() const;
    QString definition() const;

private:
    QString cname;
    struct Section
    {
        QVector<Function> functions;
        QStringList variables;
        QVector<Function> constructors;

        inline bool isEmpty() const
        { return functions.isEmpty() && variables.isEmpty() && constructors.isEmpty(); }

        void printDeclaration(const Class *klass, CodeBlock &block) const;
        QString definition(const Class *klass) const;
    };

    Section sections[3];
};

class Generator
{
public:
    Generator(const DFA &dfa, const Config &config);

    QString generate();

private:
    void generateTransitions(CodeBlock &body, const TransitionMap &transitions);
    bool isSingleReferencedFinalState(int i) const;

    DFA dfa;
    Config cfg;
    InputType minInput;
    InputType maxInput;
    QHash<int, int> backReferenceMap;
    QString headerFileName;
public:
    struct TransitionSequence
    {
        inline TransitionSequence() : first(-1), last(-1), transition(-1) {}
        InputType first;
        InputType last;
        int transition;
        QString testFunction;
    };
private:
    QVector<TransitionSequence> charFunctionRanges;
};

#endif
