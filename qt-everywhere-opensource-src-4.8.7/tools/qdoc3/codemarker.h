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
  codemarker.h
*/

#ifndef CODEMARKER_H
#define CODEMARKER_H

#include <qpair.h>

#include "atom.h"
#include "node.h"

QT_BEGIN_NAMESPACE

class Config;
class Tree;

struct Section
{
    QString name;
    QString divClass;
    QString singularMember;
    QString pluralMember;
    NodeList members;
    NodeList reimpMembers;
    QList<QPair<ClassNode *, int> > inherited;

    Section() { }
    Section(const QString& name0, 
            const QString& divClass0,
            const QString& singularMember0, 
            const QString& pluralMember0) 
        : name(name0),
          divClass(divClass0),
          singularMember(singularMember0),
	  pluralMember(pluralMember0) { }
    void appendMember(Node* node) { members.append(node); }
    void appendReimpMember(Node* node) { reimpMembers.append(node); }
};

struct FastSection
{
    const InnerNode *innerNode;
    QString name;
    QString divClass;
    QString singularMember;
    QString pluralMember;
    QMap<QString, Node *> memberMap;
    QMap<QString, Node *> reimpMemberMap;
    QList<QPair<ClassNode *, int> > inherited;

    FastSection(const InnerNode *innerNode0, 
                const QString& name0,
                const QString& divClass0,
                const QString& singularMember0,
                const QString& pluralMember0)
	: innerNode(innerNode0), 
          name(name0), 
          divClass(divClass0),
          singularMember(singularMember0),
	  pluralMember(pluralMember0) { }
    bool isEmpty() const { 
        return (memberMap.isEmpty() && 
                inherited.isEmpty() && 
                reimpMemberMap.isEmpty()); 
    }

};

class CodeMarker
{
 public:
    enum SynopsisStyle { Summary, Detailed, SeparateList, Accessors };
    enum Status { Compat, Obsolete, Okay };

    CodeMarker();
    virtual ~CodeMarker();

    virtual void initializeMarker(const Config& config);
    virtual void terminateMarker();
    virtual bool recognizeCode(const QString& code) = 0;
    virtual bool recognizeExtension(const QString& ext) = 0;
    virtual bool recognizeLanguage(const QString& lang) = 0;
    virtual Atom::Type atomType() const = 0;
    virtual QString plainName(const Node *node) = 0;
    virtual QString plainFullName(const Node *node, 
                                  const Node *relative = 0) = 0;
    virtual QString markedUpCode(const QString& code, 
                                 const Node *relative,
                                 const Location &location) = 0;
    virtual QString markedUpSynopsis(const Node *node, 
                                     const Node *relative,
                                     SynopsisStyle style) = 0;
#ifdef QDOC_QML
    virtual QString markedUpQmlItem(const Node* , bool) { return QString(); }
#endif
    virtual QString markedUpName(const Node *node) = 0;
    virtual QString markedUpFullName(const Node *node,
                                     const Node *relative = 0) = 0;
    virtual QString markedUpEnumValue(const QString &enumValue, 
                                      const Node *relative) = 0;
    virtual QString markedUpIncludes(const QStringList& includes) = 0;
    virtual QString functionBeginRegExp(const QString& funcName) = 0;
    virtual QString functionEndRegExp(const QString& funcName) = 0;
    virtual QList<Section> sections(const InnerNode *inner, 
                                    SynopsisStyle style, 
                                    Status status) = 0;
#ifdef QDOC_QML
    virtual QList<Section> qmlSections(const QmlClassNode* qmlClassNode,
                                       SynopsisStyle style,
                                       const Tree* tree);
#endif
    virtual const Node* resolveTarget(const QString& target, 
                                      const Tree* tree,
		                      const Node* relative,
                                      const Node* self = 0);
    virtual QStringList macRefsForNode(Node* node);

    static void initialize(const Config& config);
    static void terminate();
    static CodeMarker *markerForCode(const QString& code);
    static CodeMarker *markerForFileName(const QString& fileName);
    static CodeMarker *markerForLanguage(const QString& lang);
    static const Node *nodeForString(const QString& string);
    static QString stringForNode(const Node *node);

    QString typified(const QString &string);

 protected:
    virtual QString sortName(const Node *node);
    QString protect(const QString &string);
    QString taggedNode(const Node* node);
#ifdef QDOC_QML
    QString taggedQmlNode(const Node* node);
#endif
    QString linkTag(const Node *node, const QString& body);
    void insert(FastSection &fastSection, 
                Node *node, 
                SynopsisStyle style, 
                Status status);
    bool insertReimpFunc(FastSection& fs, Node* node, Status status);
    void append(QList<Section>& sectionList, const FastSection& fastSection);

 private:
    QString macName(const Node *parent, const QString &name = QString());

    static QString defaultLang;
    static QList<CodeMarker *> markers;
};

QT_END_NAMESPACE

#endif
