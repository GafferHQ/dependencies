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
  node.h
*/

#ifndef NODE_H
#define NODE_H

#include <qdir.h>
#include <qmap.h>
#include <qpair.h>
#include <qstringlist.h>

#include "codechunk.h"
#include "doc.h"
#include "location.h"
#include "text.h"

QT_BEGIN_NAMESPACE

class Node;
class ClassNode;
class InnerNode;
class ExampleNode;
class QmlClassNode;

typedef QMap<QString, const Node*> NodeMap;
typedef QMultiMap<QString, Node*> NodeMultiMap;
typedef QMap<QString, const ExampleNode*> ExampleNodeMap;

class Node
{
 public:
    enum Type { 
        Namespace, 
        Class, 
        Fake, 
        Enum, 
        Typedef, 
        Function, 
        Property,
        Variable, 
#ifdef QDOC_QML
        Target,
        QmlProperty,
        QmlSignal,
        QmlMethod,
        LastType
#else
        Target,
        LastType
#endif
    };

    enum SubType { 
        NoSubType,
        Example,
        HeaderFile,
        File,
        Image,
        Group,
        Module,
        Page,
#ifdef QDOC_QML
        ExternalPage,
        QmlClass,
        QmlPropertyGroup,
        QmlBasicType
#else
        ExternalPage
#endif
    };
    
    enum Access { Public, Protected, Private };
    
    enum Status { 
        Compat, 
        Obsolete, 
        Deprecated, 
        Preliminary, 
        Commendable, 
        Main, 
        Internal 
    }; // don't reorder this enum
    
    enum ThreadSafeness { 
        UnspecifiedSafeness, 
        NonReentrant, 
        Reentrant, 
        ThreadSafe 
    };
    
    enum LinkType { 
        StartLink, 
        NextLink, 
        PreviousLink,
        ContentsLink, 
        IndexLink,
        InheritsLink /*, 
        GlossaryLink, 
        CopyrightLink,
        ChapterLink, 
        SectionLink, 
        SubsectionLink, 
        AppendixLink */ 
    };

    enum PageType {
        NoPageType,
        ApiPage,
        ArticlePage,
        ExamplePage
    };

    virtual ~Node();

    void setAccess(Access access) { acc = access; }
    void setLocation(const Location& location) { loc = location; }
    void setDoc(const Doc& doc, bool replace = false);
    void setStatus(Status status) { sta = status; }
    void setThreadSafeness(ThreadSafeness safeness) { saf = safeness; }
    void setSince(const QString &since);
    void setRelates(InnerNode* pseudoParent);
    void setModuleName(const QString &module) { mod = module; }
    void setLink(LinkType linkType, const QString &link, const QString &desc);
    void setUrl(const QString &url);
    void setTemplateStuff(const QString &templateStuff) { tpl = templateStuff; }
    void setPageType(PageType t) { pageTyp = t; }
    void setPageType(const QString& t);
    virtual void addDependency(const QString& ) { }

    virtual bool isInnerNode() const = 0;
    virtual bool isReimp() const { return false; }
    virtual bool isFunction() const { return false; }
    virtual bool isQmlNode() const { return false; }
    virtual bool isInternal() const { return false; }
    Type type() const { return typ; }
    virtual SubType subType() const { return NoSubType; }
    InnerNode* parent() const { return par; }
    InnerNode* relates() const { return rel; }
    const QString& name() const { return nam; }
    QMap<LinkType, QPair<QString,QString> > links() const { return linkMap; }
    QString moduleName() const;
    QString url() const;
    virtual QString nameForLists() const { return nam; }

    Access access() const { return acc; }
    QString accessString() const;
    const Location& location() const { return loc; }
    const Doc& doc() const { return d; }
    Status status() const { return sta; }
    Status inheritedStatus() const;
    ThreadSafeness threadSafeness() const;
    ThreadSafeness inheritedThreadSafeness() const;
    QString since() const { return sinc; }
    QString templateStuff() const { return tpl; }
    PageType pageType() const { return pageTyp; }
    virtual void addPageKeywords(const QString& ) { }

    void clearRelated() { rel = 0; }

    virtual QString fileBase() const;
    QString guid() const;
    QString ditaXmlHref();
    QString extractClassName(const QString &string) const;
    const QmlClassNode* qmlClassNode() const;
    const ClassNode* declarativeCppNode() const;

 protected:
    Node(Type type, InnerNode* parent, const QString& name);

 private:

#ifdef Q_WS_WIN
    Type typ;
    Access acc;
    ThreadSafeness saf;
    PageType pageTyp;
    Status sta;
#else
    Type typ : 4;
    Access acc : 2;
    ThreadSafeness saf : 2;
    PageType pageTyp : 4;
    Status sta : 3;
#endif
    InnerNode* par;
    InnerNode* rel;
    QString nam;
    Location loc;
    Doc d;
    QMap<LinkType, QPair<QString, QString> > linkMap;
    QString mod;
    QString u;
    QString sinc;
    QString tpl;
    mutable QString uuid;
};

class FunctionNode;
class EnumNode;

typedef QList<Node*> NodeList;

class InnerNode : public Node
{
 public:
    virtual ~InnerNode();

    Node* findNode(const QString& name);
    Node* findNode(const QString& name, Type type);
    FunctionNode* findFunctionNode(const QString& name);
    FunctionNode* findFunctionNode(const FunctionNode* clone);
    void addInclude(const QString &include);
    void setIncludes(const QStringList &includes);
    void setOverload(const FunctionNode* func, bool overlode);
    void normalizeOverloads();
    void makeUndocumentedChildrenInternal();
    void deleteChildren();
    void removeFromRelated();

    virtual bool isInnerNode() const;
    const Node* findNode(const QString& name) const;
    const Node* findNode(const QString& name, Type type) const;
    const FunctionNode* findFunctionNode(const QString& name) const;
    const FunctionNode* findFunctionNode(const FunctionNode* clone) const;
    const EnumNode* findEnumNodeForValue(const QString &enumValue) const;
    const NodeList & childNodes() const { return children; }
    const NodeList & relatedNodes() const { return related; }
    int count() const { return children.size(); }
    int overloadNumber(const FunctionNode* func) const;
    int numOverloads(const QString& funcName) const;
    NodeList overloads(const QString &funcName) const;
    const QStringList& includes() const { return inc; }

    QStringList primaryKeys();
    QStringList secondaryKeys();
    const QStringList& pageKeywords() const { return pageKeywds; }
    virtual void addPageKeywords(const QString& t) { pageKeywds << t; }
    virtual bool isAbstract() const { return false; }
    virtual void setAbstract(bool ) { }

 protected:
    InnerNode(Type type, InnerNode* parent, const QString& name);

 private:
    friend class Node;

    static bool isSameSignature(const FunctionNode* f1, const FunctionNode* f2);
    void addChild(Node* child);
    void removeChild(Node* child);
    void removeRelated(Node* pseudoChild);

    QStringList pageKeywds;
    QStringList inc;
    NodeList children;
    NodeList enumChildren;
    NodeList related;
    QMap<QString, Node*> childMap;
    QMap<QString, Node*> primaryFunctionMap;
    QMap<QString, NodeList> secondaryFunctionMap;
};

class LeafNode : public Node
{
 public:
    LeafNode();
    virtual ~LeafNode() { }

    virtual bool isInnerNode() const;

 protected:
    LeafNode(Type type, InnerNode* parent, const QString& name);
};

class NamespaceNode : public InnerNode
{
 public:
    NamespaceNode(InnerNode* parent, const QString& name);
    virtual ~NamespaceNode() { }
};

class ClassNode;

struct RelatedClass
{
    RelatedClass() { }
    RelatedClass(Node::Access access0, 
                 ClassNode* node0,
                 const QString& dataTypeWithTemplateArgs0 = "")
      : access(access0), 
        node(node0),
        dataTypeWithTemplateArgs(dataTypeWithTemplateArgs0) { }
    QString accessString() const;

    Node::Access        access;
    ClassNode*          node;
    QString             dataTypeWithTemplateArgs;
};

class PropertyNode;

class ClassNode : public InnerNode
{
 public:
    ClassNode(InnerNode* parent, const QString& name);
    virtual ~ClassNode() { }

    void addBaseClass(Access access, 
                      ClassNode* node, 
                      const QString &dataTypeWithTemplateArgs = "");
    void fixBaseClasses();

    const QList<RelatedClass> &baseClasses() const { return bases; }
    const QList<RelatedClass> &derivedClasses() const { return derived; }
    const QList<RelatedClass> &ignoredBaseClasses() const { return ignoredBases; }

    bool hideFromMainList() const { return hidden; }
    void setHideFromMainList(bool value) { hidden = value; }

    QString serviceName() const { return sname; }
    void setServiceName(const QString& value) { sname = value; }
    QString qmlElement() const { return qmlelement; }
    void setQmlElement(const QString& value) { qmlelement = value; }
    virtual bool isAbstract() const { return abstract; }
    virtual void setAbstract(bool b) { abstract = b; }
    const PropertyNode* findPropertyNode(const QString& name) const;

 private:
    QList<RelatedClass> bases;
    QList<RelatedClass> derived;
    QList<RelatedClass> ignoredBases;
    bool hidden;
    bool abstract;
    QString sname;
    QString qmlelement;
};

class FakeNode : public InnerNode
{
 public:

    FakeNode(InnerNode* parent, const QString& name, SubType subType);
    virtual ~FakeNode() { }

    void setTitle(const QString &title) { tle = title; }
    void setSubTitle(const QString &subTitle) { stle = subTitle; }
    void addGroupMember(Node* node) { gr.append(node); }

    SubType subType() const { return sub; }
    virtual QString title() const;
    virtual QString fullTitle() const;
    virtual QString subTitle() const;
    virtual QString imageFileName() const { return QString(); }
    const NodeList &groupMembers() const { return gr; }
    virtual QString nameForLists() const { return title(); }
    virtual void setImageFileName(const QString& ) { }

 private:
    SubType sub;
    QString tle;
    QString stle;
    NodeList gr;
};

class ExampleNode : public FakeNode
{
 public:
    ExampleNode(InnerNode* parent, const QString& name);
    virtual ~ExampleNode() { }
    virtual QString imageFileName() const { return imageFileName_; }
    virtual void setImageFileName(const QString& ifn) { imageFileName_ = ifn; }
    virtual void addDependency(const QString& arg) { dependencies_.append(arg); }
    const QStringList& dependencies() const { return dependencies_; }

 public:
    static ExampleNodeMap exampleNodeMap;

 private:
    QString imageFileName_;
    QStringList dependencies_;
};

class QmlClassNode : public FakeNode
{
 public:
    QmlClassNode(InnerNode* parent, 
                 const QString& name, 
                 const ClassNode* cn);
    virtual ~QmlClassNode();
    virtual bool isQmlNode() const { return true; }

    const ClassNode* classNode() const { return cnode; }
    virtual QString fileBase() const;
    static void addInheritedBy(const QString& base, Node* sub);
    static void subclasses(const QString& base, NodeList& subs);
    static void clear();

 public:
    static bool qmlOnly;
    static QMultiMap<QString,Node*> inheritedBy;

 private:
    const ClassNode*    cnode;
};

class QmlBasicTypeNode : public FakeNode
{
 public:
    QmlBasicTypeNode(InnerNode* parent, 
                     const QString& name);
    virtual ~QmlBasicTypeNode() { }
    virtual bool isQmlNode() const { return true; }
};

class QmlPropGroupNode : public FakeNode
{
 public:
    QmlPropGroupNode(QmlClassNode* parent, 
                     const QString& name,
                     bool attached);
    virtual ~QmlPropGroupNode() { }
    virtual bool isQmlNode() const { return true; }

    const QString& element() const { return parent()->name(); }
    void setDefault() { isdefault = true; }
    bool isDefault() const { return isdefault; }
    bool isAttached() const { return att; }

 private:
    bool    isdefault;
    bool    att;
};

class Tree;

class QmlPropertyNode : public LeafNode
{
 public:
    QmlPropertyNode(QmlPropGroupNode* parent, 
                    const QString& name,
                    const QString& type,
                    bool attached);
    virtual ~QmlPropertyNode() { }

    void setDataType(const QString& dataType) { dt = dataType; }
    void setStored(bool stored) { sto = toTrool(stored); }
    void setDesignable(bool designable) { des = toTrool(designable); }
    void setWritable(bool writable) { wri = toTrool(writable); }

    const QString &dataType() const { return dt; }
    QString qualifiedDataType() const { return dt; }
    bool isStored() const { return fromTrool(sto,true); }
    bool isDesignable() const { return fromTrool(des,false); }
    bool isWritable(const Tree* tree) const;
    bool isAttached() const { return att; }
    virtual bool isQmlNode() const { return true; }

    const PropertyNode *correspondingProperty(const Tree *tree) const;

    const QString& element() const { return static_cast<QmlPropGroupNode*>(parent())->element(); }

 private:
    enum Trool { Trool_True, Trool_False, Trool_Default };

    static Trool toTrool(bool boolean);
    static bool fromTrool(Trool troolean, bool defaultValue);

    QString dt;
    Trool   sto;
    Trool   des;
    Trool   wri;
    bool    att;
};

class EnumItem
{
 public:
    EnumItem() { }
    EnumItem(const QString& name, const QString& value)
	: nam(name), val(value) { }
    EnumItem(const QString& name, const QString& value, const Text &txt)
	: nam(name), val(value), txt(txt) { }

    const QString& name() const { return nam; }
    const QString& value() const { return val; }
    const Text &text() const { return txt; }

 private:
    QString nam;
    QString val;
    Text txt;
};

class TypedefNode;

class EnumNode : public LeafNode
{
 public:
    EnumNode(InnerNode* parent, const QString& name);
    virtual ~EnumNode() { }

    void addItem(const EnumItem& item);
    void setFlagsType(TypedefNode* typedeff);
    bool hasItem(const QString &name) const { return names.contains(name); }

    const QList<EnumItem>& items() const { return itms; }
    Access itemAccess(const QString& name) const;
    const TypedefNode* flagsType() const { return ft; }
    QString itemValue(const QString &name) const;

 private:
    QList<EnumItem> itms;
    QSet<QString> names;
    const TypedefNode* ft;
};

class TypedefNode : public LeafNode
{
 public:
    TypedefNode(InnerNode* parent, const QString& name);
    virtual ~TypedefNode() { }

    const EnumNode* associatedEnum() const { return ae; }

 private:
    void setAssociatedEnum(const EnumNode* enume);

    friend class EnumNode;

    const EnumNode* ae;
};

inline void EnumNode::setFlagsType(TypedefNode* typedeff)
{
    ft = typedeff;
    typedeff->setAssociatedEnum(this);
}


class Parameter
{
 public:
    Parameter() {}
    Parameter(const QString& leftType, 
              const QString& rightType = "",
              const QString& name = "", 
              const QString& defaultValue = "");
    Parameter(const Parameter& p);

    Parameter& operator=(const Parameter& p);

    void setName(const QString& name) { nam = name; }

    bool hasType() const { return lef.length() + rig.length() > 0; }
    const QString& leftType() const { return lef; }
    const QString& rightType() const { return rig; }
    const QString& name() const { return nam; }
    const QString& defaultValue() const { return def; }

    QString reconstruct(bool value = false) const;

 private:
    QString lef;
    QString rig;
    QString nam;
    QString def;
};

class PropertyNode;

class FunctionNode : public LeafNode
{
 public:
    enum Metaness { 
        Plain, 
        Signal, 
        Slot,
        Ctor,
        Dtor, 
        MacroWithParams,
        MacroWithoutParams, 
        Native };
    enum Virtualness { NonVirtual, ImpureVirtual, PureVirtual };

    FunctionNode(InnerNode* parent, const QString &name);
    FunctionNode(Type type, InnerNode* parent, const QString &name, bool attached);
    virtual ~FunctionNode() { }

    void setReturnType(const QString& returnType) { rt = returnType; }
    void setParentPath(const QStringList& parentPath) { pp = parentPath; }
    void setMetaness(Metaness metaness) { met = metaness; }
    void setVirtualness(Virtualness virtualness);
    void setConst(bool conste) { con = conste; }
    void setStatic(bool statique) { sta = statique; }
    void setOverload(bool overlode);
    void setReimp(bool r);
    void addParameter(const Parameter& parameter);
    inline void setParameters(const QList<Parameter>& parameters);
    void borrowParameterNames(const FunctionNode* source);
    void setReimplementedFrom(FunctionNode* from);

    const QString& returnType() const { return rt; }
    Metaness metaness() const { return met; }
    bool isMacro() const { 
        return met == MacroWithParams || met == MacroWithoutParams; 
    }
    Virtualness virtualness() const { return vir; }
    bool isConst() const { return con; }
    bool isStatic() const { return sta; }
    bool isOverload() const { return ove; }
    bool isReimp() const { return reimp; }
    bool isFunction() const { return true; }
    int overloadNumber() const;
    int numOverloads() const;
    const QList<Parameter>& parameters() const { return params; }
    QStringList parameterNames() const;
    QString rawParameters(bool names = false, bool values = false) const;
    const FunctionNode* reimplementedFrom() const { return rf; }
    const QList<FunctionNode*> &reimplementedBy() const { return rb; }
    const PropertyNode* associatedProperty() const { return ap; }
    const QStringList& parentPath() const { return pp; }

    QStringList reconstructParams(bool values = false) const;
    QString signature(bool values = false) const;
    const QString& element() const { return parent()->name(); }
    bool isAttached() const { return att; }
    virtual bool isQmlNode() const { 
        return ((type() == QmlSignal) || (type() == QmlMethod)); 
    }
    virtual bool isInternal() const;

    void debug() const;

 private:
    void setAssociatedProperty(PropertyNode* property);

    friend class InnerNode;
    friend class PropertyNode;

    QString     rt;
    QStringList pp;
#ifdef Q_WS_WIN
    Metaness    met;
    Virtualness vir;
#else
    Metaness met : 4;
    Virtualness vir : 2;
#endif
    bool con : 1;
    bool sta : 1;
    bool ove : 1;
    bool reimp: 1; 
    bool att: 1;
    QList<Parameter> params;
    const FunctionNode* rf;
    const PropertyNode* ap;
    QList<FunctionNode*> rb;
};

class PropertyNode : public LeafNode
{
 public:
    enum FunctionRole { Getter, Setter, Resetter, Notifier };
    enum { NumFunctionRoles = Notifier + 1 };

    PropertyNode(InnerNode* parent, const QString& name);
    virtual ~PropertyNode() { }

    void setDataType(const QString& dataType) { dt = dataType; }
    void addFunction(FunctionNode* function, FunctionRole role);
    void addSignal(FunctionNode* function, FunctionRole role);
    void setStored(bool stored) { sto = toTrool(stored); }
    void setDesignable(bool designable) { des = toTrool(designable); }
    void setScriptable(bool scriptable) { scr = toTrool(scriptable); }
    void setWritable(bool writable) { wri = toTrool(writable); }
    void setUser(bool user) { usr = toTrool(user); }
    void setOverriddenFrom(const PropertyNode* baseProperty);
    void setRuntimeDesFunc(const QString& rdf) { runtimeDesFunc = rdf; }
    void setRuntimeScrFunc(const QString& scrf) { runtimeScrFunc = scrf; }
    void setConstant() { cst = true; }
    void setFinal() { fnl = true; }
    void setRevision(int revision) { rev = revision; }

    const QString &dataType() const { return dt; }
    QString qualifiedDataType() const;
    NodeList functions() const;
    NodeList functions(FunctionRole role) const { return funcs[(int)role]; }
    NodeList getters() const { return functions(Getter); }
    NodeList setters() const { return functions(Setter); }
    NodeList resetters() const { return functions(Resetter); }
    NodeList notifiers() const { return functions(Notifier); }
    bool isStored() const { return fromTrool(sto, storedDefault()); }
    bool isDesignable() const { return fromTrool(des, designableDefault()); }
    bool isScriptable() const { return fromTrool(scr, scriptableDefault()); }
    const QString& runtimeDesignabilityFunction() const { return runtimeDesFunc; }
    const QString& runtimeScriptabilityFunction() const { return runtimeScrFunc; }
    bool isWritable() const { return fromTrool(wri, writableDefault()); }
    bool isUser() const { return fromTrool(usr, userDefault()); }
    bool isConstant() const { return cst; }
    bool isFinal() const { return fnl; }
    const PropertyNode* overriddenFrom() const { return overrides; }

    bool storedDefault() const { return true; }
    bool userDefault() const { return false; }
    bool designableDefault() const { return !setters().isEmpty(); }
    bool scriptableDefault() const { return true; }
    bool writableDefault() const { return !setters().isEmpty(); }

 private:
    enum Trool { Trool_True, Trool_False, Trool_Default };

    static Trool toTrool(bool boolean);
    static bool fromTrool(Trool troolean, bool defaultValue);

    QString dt;
    QString runtimeDesFunc;
    QString runtimeScrFunc;
    NodeList funcs[NumFunctionRoles];
    Trool sto;
    Trool des;
    Trool scr;
    Trool wri;
    Trool usr;
    bool cst;
    bool fnl;
    int rev;
    const PropertyNode* overrides;
};

inline void FunctionNode::setParameters(const QList<Parameter> &parameters)
{
    params = parameters;
}

inline void PropertyNode::addFunction(FunctionNode* function, FunctionRole role)
{
    funcs[(int)role].append(function);
    function->setAssociatedProperty(this);
}

inline void PropertyNode::addSignal(FunctionNode* function, FunctionRole role)
{
    funcs[(int)role].append(function);
}

inline NodeList PropertyNode::functions() const
{
    NodeList list;
    for (int i = 0; i < NumFunctionRoles; ++i)
	list += funcs[i];
    return list;
}

class VariableNode : public LeafNode
{
 public:
    VariableNode(InnerNode* parent, const QString &name);
    virtual ~VariableNode() { }

    void setLeftType(const QString &leftType) { lt = leftType; }
    void setRightType(const QString &rightType) { rt = rightType; }
    void setStatic(bool statique) { sta = statique; }

    const QString &leftType() const { return lt; }
    const QString &rightType() const { return rt; }
    QString dataType() const { return lt + rt; }
    bool isStatic() const { return sta; }

 private:
    QString lt;
    QString rt;
    bool sta;
};

inline VariableNode::VariableNode(InnerNode* parent, const QString &name)
    : LeafNode(Variable, parent, name), sta(false)
{
    // nothing.
}

class TargetNode : public LeafNode
{
 public:
    TargetNode(InnerNode* parent, const QString& name);
    virtual ~TargetNode() { }

    virtual bool isInnerNode() const;
};

QT_END_NAMESPACE

#endif
