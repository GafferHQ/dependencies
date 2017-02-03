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
  cppcodemarker.cpp
*/

#include "atom.h"
#include "cppcodemarker.h"
#include "node.h"
#include "text.h"
#include "tree.h"

#include <ctype.h>

QT_BEGIN_NAMESPACE

/*!
  The constructor does nothing.
 */
CppCodeMarker::CppCodeMarker()
{
    // nothing.
}

/*!
  The destructor does nothing.
 */
CppCodeMarker::~CppCodeMarker()
{
    // nothing.
}

/*!
  Returns true.
 */
bool CppCodeMarker::recognizeCode(const QString & /* code */)
{
    return true;
}

/*!
  Returns true if \a ext is any of a list of file extensions
  for the C++ language.
 */
bool CppCodeMarker::recognizeExtension(const QString& ext)
{
    return ext == "c" ||
        ext == "c++" ||
        ext == "cc" ||
        ext == "cpp" ||
        ext == "cxx" ||
        ext == "ch" ||
        ext == "h" ||
        ext == "h++" ||
        ext == "hh" ||
        ext == "hpp" ||
        ext == "hxx";
}

/*!
  Returns true if \a lang is either "C" or "Cpp".
 */
bool CppCodeMarker::recognizeLanguage(const QString &lang)
{
    return lang == "C" || lang == "Cpp";
}

/*!
  Returns the type of atom used to represent C++ code in the documentation.
*/
Atom::Type CppCodeMarker::atomType() const
{
    return Atom::Code;
}

/*!
  Returns the \a node name, or "()" if \a node is a
  Node::Function node.
 */
QString CppCodeMarker::plainName(const Node *node)
{
    QString name = node->name();
    if (node->type() == Node::Function)
	name += "()";
    return name;
}

QString CppCodeMarker::plainFullName(const Node *node, const Node *relative)
{
    if (node->name().isEmpty()) {
	return "global";
    }
    else {
	QString fullName;
	while (node) {
	    fullName.prepend(plainName(node));
	    if (node->parent() == relative || node->parent()->name().isEmpty())
		break;
	    fullName.prepend("::");
	    node = node->parent();
        }
        return fullName;
    }
}

QString CppCodeMarker::markedUpCode(const QString &code,
                                    const Node *relative,
				    const Location &location)
{
    return addMarkUp(code, relative, location);
}

QString CppCodeMarker::markedUpSynopsis(const Node *node,
                                        const Node * /* relative */,
					SynopsisStyle style)
{
    const int MaxEnumValues = 6;
    const FunctionNode *func;
    const PropertyNode *property;
    const VariableNode *variable;
    const EnumNode *enume;
    const TypedefNode *typedeff;
    QString synopsis;
    QString extra;
    QString name;

    name = taggedNode(node);
    if (style != Detailed)
	name = linkTag(node, name);
    name = "<@name>" + name + "</@name>";

    if (style == Detailed && !node->parent()->name().isEmpty() &&
        node->type() != Node::Property)
	name.prepend(taggedNode(node->parent()) + "::");

    switch (node->type()) {
    case Node::Namespace:
	synopsis = "namespace " + name;
	break;
    case Node::Class:
	synopsis = "class " + name;
	break;
    case Node::Function:
    case Node::QmlSignal:
    case Node::QmlMethod:
	func = (const FunctionNode *) node;
	if (style != SeparateList && !func->returnType().isEmpty())
	    synopsis = typified(func->returnType()) + " ";
	synopsis += name;
        if (func->metaness() != FunctionNode::MacroWithoutParams) {
            synopsis += " (";
	    if (!func->parameters().isEmpty()) {
	        synopsis += " ";
	        QList<Parameter>::ConstIterator p = func->parameters().begin();
	        while (p != func->parameters().end()) {
		    if (p != func->parameters().begin())
		        synopsis += ", ";
		    synopsis += typified((*p).leftType());
                    if (style != SeparateList && !(*p).name().isEmpty())
                        synopsis +=
                            " <@param>" + protect((*p).name()) + "</@param>";
                    synopsis += protect((*p).rightType());
		    if (style != SeparateList && !(*p).defaultValue().isEmpty())
		        synopsis += " = " + protect((*p).defaultValue());
		    ++p;
	        }
	        synopsis += " ";
	    }
	    synopsis += ")";
        }
	if (func->isConst())
	    synopsis += " const";

	if (style == Summary || style == Accessors) {
	    if (func->virtualness() != FunctionNode::NonVirtual)
		synopsis.prepend("virtual ");
	    if (func->virtualness() == FunctionNode::PureVirtual)
		synopsis.append(" = 0");
	}
        else if (style == SeparateList) {
            if (!func->returnType().isEmpty() && func->returnType() != "void")
                synopsis += " : " + typified(func->returnType());
        }
        else {
	    QStringList bracketed;
	    if (func->isStatic()) {
		bracketed += "static";
	    }
            else if (func->virtualness() != FunctionNode::NonVirtual) {
		if (func->virtualness() == FunctionNode::PureVirtual)
		    bracketed += "pure";
		bracketed += "virtual";
	    }

	    if (func->access() == Node::Protected) {
		bracketed += "protected";
	    }
            else if (func->access() == Node::Private) {
		bracketed += "private";
	    }

	    if (func->metaness() == FunctionNode::Signal) {
		bracketed += "signal";
	    }
            else if (func->metaness() == FunctionNode::Slot) {
		bracketed += "slot";
	    }
	    if (!bracketed.isEmpty())
		extra += " [" + bracketed.join(" ") + "]";
	}
	break;
    case Node::Enum:
	enume = static_cast<const EnumNode *>(node);
	synopsis = "enum " + name;
        if (style == Summary) {
            synopsis += " { ";

            QStringList documentedItems = enume->doc().enumItemNames();
            if (documentedItems.isEmpty()) {
                foreach (const EnumItem &item, enume->items())
                    documentedItems << item.name();
            }
            QStringList omitItems = enume->doc().omitEnumItemNames();
            foreach (const QString &item, omitItems)
                documentedItems.removeAll(item);

            if (documentedItems.size() <= MaxEnumValues) {
                for (int i = 0; i < documentedItems.size(); ++i) {
	            if (i != 0)
		        synopsis += ", ";
		    synopsis += documentedItems.at(i);
                }
            }
            else {
                for (int i = 0; i < documentedItems.size(); ++i) {
		    if (i < MaxEnumValues-2 || i == documentedItems.size()-1) {
	                if (i != 0)
		            synopsis += ", ";
		        synopsis += documentedItems.at(i);
		    }
                    else if (i == MaxEnumValues - 1) {
		        synopsis += ", ...";
		    }
                }
            }
	    if (!documentedItems.isEmpty())
		synopsis += " ";
	    synopsis += "}";
	}
	break;
    case Node::Typedef:
        typedeff = static_cast<const TypedefNode *>(node);
        if (typedeff->associatedEnum()) {
            synopsis = "flags " + name;
        }
        else {
            synopsis = "typedef " + name;
        }
	break;
    case Node::Property:
	property = static_cast<const PropertyNode *>(node);
	synopsis = name + " : " + typified(property->qualifiedDataType());
	break;
    case Node::Variable:
	variable = static_cast<const VariableNode *>(node);
        if (style == SeparateList) {
            synopsis = name + " : " + typified(variable->dataType());
        }
        else {
            synopsis = typified(variable->leftType()) + " " +
                name + protect(variable->rightType());
        }
	break;
    default:
	synopsis = name;
    }

    if (style == Summary) {
	if (node->status() == Node::Preliminary) {
	    extra += " (preliminary)";
	}
        else if (node->status() == Node::Deprecated) {
	    extra += " (deprecated)";
	}
        else if (node->status() == Node::Obsolete) {
	    extra += " (obsolete)";
	}
    }

    if (!extra.isEmpty()) {
	extra.prepend("<@extra>");
	extra.append("</@extra>");
    }
    return synopsis + extra;
}

#ifdef QDOC_QML
/*!
 */
QString CppCodeMarker::markedUpQmlItem(const Node* node, bool summary)
{
    QString name = taggedQmlNode(node);
    if (summary) {
	name = linkTag(node,name);
    } else if (node->type() == Node::QmlProperty) {
        const QmlPropertyNode* pn = static_cast<const QmlPropertyNode*>(node);
        if (pn->isAttached())
            name.prepend(pn->element() + QLatin1Char('.'));
    }
    name = "<@name>" + name + "</@name>";
    QString synopsis = name;
    if (node->type() == Node::QmlProperty) {
        const QmlPropertyNode* pn = static_cast<const QmlPropertyNode*>(node);
        synopsis += " : " + typified(pn->dataType());
    }

    QString extra;
    if (summary) {
	if (node->status() == Node::Preliminary) {
	    extra += " (preliminary)";
	}
        else if (node->status() == Node::Deprecated) {
	    extra += " (deprecated)";
	}
        else if (node->status() == Node::Obsolete) {
	    extra += " (obsolete)";
	}
    }

    if (!extra.isEmpty()) {
	extra.prepend("<@extra>");
	extra.append("</@extra>");
    }
    return synopsis + extra;
}
#endif

QString CppCodeMarker::markedUpName(const Node *node)
{
    QString name = linkTag(node, taggedNode(node));
    if (node->type() == Node::Function)
	name += "()";
    return name;
}

QString CppCodeMarker::markedUpFullName(const Node *node, const Node *relative)
{
    if (node->name().isEmpty()) {
	return "global";
    }
    else {
	QString fullName;
	for (;;) {
	    fullName.prepend(markedUpName(node));
	    if (node->parent() == relative || node->parent()->name().isEmpty())
		break;
	    fullName.prepend("<@op>::</@op>");
	    node = node->parent();
        }
        return fullName;
    }
}

QString CppCodeMarker::markedUpEnumValue(const QString &enumValue,
                                         const Node *relative)
{
    const Node *node = relative->parent();
    QString fullName;
    while (node->parent()) {
	fullName.prepend(markedUpName(node));
	if (node->parent() == relative || node->parent()->name().isEmpty())
	    break;
	fullName.prepend("<@op>::</@op>");
	node = node->parent();
    }
    if (!fullName.isEmpty())
        fullName.append("<@op>::</@op>");
    fullName.append(enumValue);
    return fullName;
}

QString CppCodeMarker::markedUpIncludes(const QStringList& includes)
{
    QString code;

    QStringList::ConstIterator inc = includes.begin();
    while (inc != includes.end()) {
	code += "<@preprocessor>#include &lt;<@headerfile>" + *inc + "</@headerfile>&gt;</@preprocessor>\n";
	++inc;
    }
    return code;
}

QString CppCodeMarker::functionBeginRegExp(const QString& funcName)
{
    return "^" + QRegExp::escape(funcName) + "$";

}

QString CppCodeMarker::functionEndRegExp(const QString& /* funcName */)
{
    return "^\\}$";
}

QList<Section> CppCodeMarker::sections(const InnerNode *inner,
                                       SynopsisStyle style,
                                       Status status)
{
    QList<Section> sections;

    if (inner->type() == Node::Class) {
        const ClassNode *classe = static_cast<const ClassNode *>(inner);

        if (style == Summary) {
	    FastSection privateFunctions(classe,
                                         "Private Functions",
                                         "",
                                         "private function",
				         "private functions");
	    FastSection privateSlots(classe, "Private Slots", "", "private slot", "private slots");
	    FastSection privateTypes(classe, "Private Types", "", "private type", "private types");
	    FastSection protectedFunctions(classe,
                                           "Protected Functions",
                                           "",
                                           "protected function",
				           "protected functions");
	    FastSection protectedSlots(classe,
                                       "Protected Slots",
                                       "",
                                       "protected slot",
                                       "protected slots");
	    FastSection protectedTypes(classe,
                                       "Protected Types",
                                       "",
                                       "protected type",
                                       "protected types");
	    FastSection protectedVariables(classe,
                                           "Protected Variables",
                                           "",
                                           "protected type",
                                           "protected variables");
	    FastSection publicFunctions(classe,
                                        "Public Functions",
                                        "",
                                        "public function",
                                        "public functions");
	    FastSection publicSignals(classe, "Signals", "", "signal", "signals");
	    FastSection publicSlots(classe, "Public Slots", "", "public slot", "public slots");
	    FastSection publicTypes(classe, "Public Types", "", "public type", "public types");
	    FastSection publicVariables(classe,
                                        "Public Variables",
                                        "",
                                        "public variable",
                                        "public variables");
	    FastSection properties(classe, "Properties", "", "property", "properties");
	    FastSection relatedNonMembers(classe,
                                          "Related Non-Members",
                                          "",
                                          "related non-member",
                                          "related non-members");
	    FastSection staticPrivateMembers(classe,
                                             "Static Private Members",
                                             "",
                                             "static private member",
					     "static private members");
	    FastSection staticProtectedMembers(classe,
                                               "Static Protected Members",
                                               "",
					       "static protected member",
                                               "static protected members");
	    FastSection staticPublicMembers(classe,
                                            "Static Public Members",
                                            "",
                                            "static public member",
					    "static public members");
            FastSection macros(inner, "Macros", "", "macro", "macros");

	    NodeList::ConstIterator r = classe->relatedNodes().begin();
            while (r != classe->relatedNodes().end()) {
                if ((*r)->type() == Node::Function) {
                    FunctionNode *func = static_cast<FunctionNode *>(*r);
                    if (func->isMacro())
                        insert(macros, *r, style, status);
                    else
                        insert(relatedNonMembers, *r, style, status);
                }
                else {
                    insert(relatedNonMembers, *r, style, status);
                }
	        ++r;
            }

	    QStack<const ClassNode *> stack;
	    stack.push(classe);

	    while (!stack.isEmpty()) {
	        const ClassNode *ancestorClass = stack.pop();

	        NodeList::ConstIterator c = ancestorClass->childNodes().begin();
	        while (c != ancestorClass->childNodes().end()) {
	            bool isSlot = false;
	            bool isSignal = false;
	            bool isStatic = false;
	            if ((*c)->type() == Node::Function) {
		        const FunctionNode *func = (const FunctionNode *) *c;
		        isSlot = (func->metaness() == FunctionNode::Slot);
		        isSignal = (func->metaness() == FunctionNode::Signal);
		        isStatic = func->isStatic();
	            }
                    else if ((*c)->type() == Node::Variable) {
                        const VariableNode *var = static_cast<const VariableNode *>(*c);
                        isStatic = var->isStatic();
                    }

	            switch ((*c)->access()) {
	            case Node::Public:
		        if (isSlot) {
		            insert(publicSlots, *c, style, status);
		        }
                        else if (isSignal) {
		            insert(publicSignals, *c, style, status);
		        }
                        else if (isStatic) {
                            if ((*c)->type() != Node::Variable
                                    || !(*c)->doc().isEmpty())
		                insert(staticPublicMembers,*c,style,status);
		        }
                        else if ((*c)->type() == Node::Property) {
                            insert(properties, *c, style, status);
		        }
                        else if ((*c)->type() == Node::Variable) {
                            if (!(*c)->doc().isEmpty())
                                insert(publicVariables, *c, style, status);
		        }
                        else if ((*c)->type() == Node::Function) {
                            if (!insertReimpFunc(publicFunctions,*c,status))
                                insert(publicFunctions, *c, style, status);
		        }
                        else {
		            insert(publicTypes, *c, style, status);
		        }
		        break;
	            case Node::Protected:
		        if (isSlot) {
		            insert(protectedSlots, *c, style, status);
		        }
                        else if (isStatic) {
                            if ((*c)->type() != Node::Variable
                                    || !(*c)->doc().isEmpty())
		                insert(staticProtectedMembers,*c,style,status);
		        }
                        else if ((*c)->type() == Node::Variable) {
                            if (!(*c)->doc().isEmpty())
                                insert(protectedVariables,*c,style,status);
		        }
                        else if ((*c)->type() == Node::Function) {
                            if (!insertReimpFunc(protectedFunctions,*c,status))
                                insert(protectedFunctions, *c, style, status);
		        }
                        else {
		            insert(protectedTypes, *c, style, status);
		        }
		        break;
	            case Node::Private:
		        if (isSlot) {
		            insert(privateSlots, *c, style, status);
		        }
                        else if (isStatic) {
                            if ((*c)->type() != Node::Variable
                                    || !(*c)->doc().isEmpty())
		                insert(staticPrivateMembers,*c,style,status);
		        }
                        else if ((*c)->type() == Node::Function) {
                            if (!insertReimpFunc(privateFunctions,*c,status))
                                insert(privateFunctions, *c, style, status);
		        }
                        else {
		            insert(privateTypes,*c,style,status);
		        }
	            }
	            ++c;
	        }

	        QList<RelatedClass>::ConstIterator r =
                    ancestorClass->baseClasses().begin();
	        while (r != ancestorClass->baseClasses().end()) {
		    stack.prepend((*r).node);
		    ++r;
	        }
	    }

	    append(sections, publicTypes);
	    append(sections, properties);
	    append(sections, publicFunctions);
	    append(sections, publicSlots);
	    append(sections, publicSignals);
	    append(sections, publicVariables);
	    append(sections, staticPublicMembers);
	    append(sections, protectedTypes);
	    append(sections, protectedFunctions);
	    append(sections, protectedSlots);
	    append(sections, protectedVariables);
	    append(sections, staticProtectedMembers);
	    append(sections, privateTypes);
	    append(sections, privateFunctions);
	    append(sections, privateSlots);
	    append(sections, staticPrivateMembers);
	    append(sections, relatedNonMembers);
            append(sections, macros);
        }
        else if (style == Detailed) {
	    FastSection memberFunctions(classe,"Member Function Documentation","func","member","members");
	    FastSection memberTypes(classe,"Member Type Documentation","types","member","members");
	    FastSection memberVariables(classe,"Member Variable Documentation","vars","member","members");
	    FastSection properties(classe,"Property Documentation","prop","member","members");
	    FastSection relatedNonMembers(classe,"Related Non-Members","relnonmem","member","members");
	    FastSection macros(classe,"Macro Documentation","macros","member","members");

	    NodeList::ConstIterator r = classe->relatedNodes().begin();
            while (r != classe->relatedNodes().end()) {
                if ((*r)->type() == Node::Function) {
                    FunctionNode *func = static_cast<FunctionNode *>(*r);
                    if (func->isMacro())
                        insert(macros, *r, style, status);
                    else
                        insert(relatedNonMembers, *r, style, status);
                }
                else {
                    insert(relatedNonMembers, *r, style, status);
                }
	        ++r;
            }

	    NodeList::ConstIterator c = classe->childNodes().begin();
	    while (c != classe->childNodes().end()) {
	        if ((*c)->type() == Node::Enum ||
                    (*c)->type() == Node::Typedef) {
		    insert(memberTypes, *c, style, status);
	        }
                else if ((*c)->type() == Node::Property) {
		    insert(properties, *c, style, status);
	        }
                else if ((*c)->type() == Node::Variable) {
                    if (!(*c)->doc().isEmpty())
		        insert(memberVariables, *c, style, status);
	        }
                else if ((*c)->type() == Node::Function) {
		    FunctionNode *function = static_cast<FunctionNode *>(*c);
                    if (!function->associatedProperty())
		        insert(memberFunctions, function, style, status);
	        }
	        ++c;
	    }

	    append(sections, memberTypes);
	    append(sections, properties);
	    append(sections, memberFunctions);
	    append(sections, memberVariables);
	    append(sections, relatedNonMembers);
	    append(sections, macros);
        }
        else {
	    FastSection all(classe,"","","member","members");

	    QStack<const ClassNode *> stack;
	    stack.push(classe);

	    while (!stack.isEmpty()) {
	        const ClassNode *ancestorClass = stack.pop();

	        NodeList::ConstIterator c = ancestorClass->childNodes().begin();
	        while (c != ancestorClass->childNodes().end()) {
		    if ((*c)->access() != Node::Private &&
                        (*c)->type() != Node::Property)
		        insert(all, *c, style, status);
		    ++c;
	        }

	        QList<RelatedClass>::ConstIterator r =
                    ancestorClass->baseClasses().begin();
	        while (r != ancestorClass->baseClasses().end()) {
		    stack.prepend((*r).node);
		    ++r;
	        }
	    }
	    append(sections, all);
        }
    }
    else {
        if (style == Summary || style == Detailed) {
	    FastSection namespaces(inner,
                                   "Namespaces",
                                   style == Detailed ? "nmspace" : "",
                                   "namespace",
                                   "namespaces");
            FastSection classes(inner,
                                "Classes",
                                style == Detailed ? "classes" : "",
                                "class",
                                "classes");
            FastSection types(inner,
                              style == Summary ? "Types" : "Type Documentation",
                              style == Detailed ? "types" : "",
                              "type",
			      "types");
            FastSection functions(inner,
                                  style == Summary ?
                                  "Functions" : "Function Documentation",
                                  style == Detailed ? "func" : "",
			          "function",
                                  "functions");
            FastSection macros(inner,
                               style == Summary ?
                               "Macros" : "Macro Documentation",
                               style == Detailed ? "macros" : "",
                               "macro",
                               "macros");

	    NodeList nodeList = inner->childNodes();
            nodeList += inner->relatedNodes();

	    NodeList::ConstIterator n = nodeList.begin();
            while (n != nodeList.end()) {
	        switch ((*n)->type()) {
                case Node::Namespace:
		    insert(namespaces, *n, style, status);
                    break;
	        case Node::Class:
		    insert(classes, *n, style, status);
                    break;
	        case Node::Enum:
	        case Node::Typedef:
		    insert(types, *n, style, status);
                    break;
	        case Node::Function:
                    {
                        FunctionNode *func = static_cast<FunctionNode *>(*n);
                        if (func->isMacro())
		            insert(macros, *n, style, status);
                        else
		            insert(functions, *n, style, status);
                    }
                    break;
	        default:
		    ;
	        }
	        ++n;
            }
            append(sections, namespaces);
            append(sections, classes);
            append(sections, types);
            append(sections, functions);
            append(sections, macros);
        }
    }

    return sections;
}

const Node *CppCodeMarker::resolveTarget(const QString& target,
                                         const Tree* tree,
                                         const Node* relative,
                                         const Node* self)
{
    if (target.endsWith("()")) {
        const FunctionNode *func;
        QString funcName = target;
        funcName.chop(2);

        QStringList path = funcName.split("::");
        if ((func = tree->findFunctionNode(path,
                                           relative,
                                           Tree::SearchBaseClasses))
                && func->metaness() != FunctionNode::MacroWithoutParams)
            return func;
    }
    else if (target.contains("#")) {
        // ### this doesn't belong here; get rid of TargetNode hack
        int hashAt = target.indexOf("#");
        QString link = target.left(hashAt);
        QString ref = target.mid(hashAt + 1);
        const Node *node;
        if (link.isEmpty()) {
            node = relative;
        }
        else {
            QStringList path(link);
            node = tree->findNode(path, tree->root(), Tree::SearchBaseClasses);
        }
        if (node && node->isInnerNode()) {
            const Atom *atom = node->doc().body().firstAtom();
            while (atom) {
                if (atom->type() == Atom::Target && atom->string() == ref) {
                    Node *parentNode = const_cast<Node *>(node);
                    return new TargetNode(static_cast<InnerNode*>(parentNode),
                                          ref);
                }
                atom = atom->next();
            }
        }
    }
    else {
        QStringList path = target.split("::");
        const Node *node;
        int flags = Tree::SearchBaseClasses |
            Tree::SearchEnumValues |
            Tree::NonFunction;
        if ((node = tree->findNode(path,
                                   relative,
                                   flags,
                                   self)))
            return node;
    }
    return 0;
}

static const char * const typeTable[] = {
    "bool", "char", "double", "float", "int", "long", "short",
    "signed", "unsigned", "uint", "ulong", "ushort", "uchar", "void",
    "qlonglong", "qulonglong",
    "qint", "qint8", "qint16", "qint32", "qint64",
    "quint", "quint8", "quint16", "quint32", "quint64",
    "qreal", "cond", 0
};

static const char * const keywordTable[] = {
    "and", "and_eq", "asm", "auto", "bitand", "bitor", "break",
    "case", "catch", "class", "compl", "const", "const_cast",
    "continue", "default", "delete", "do", "dynamic_cast", "else",
    "enum", "explicit", "export", "extern", "false", "for", "friend",
    "goto", "if", "include", "inline", "monitor", "mutable", "namespace",
    "new", "not", "not_eq", "operator", "or", "or_eq", "private", "protected",
    "public", "register", "reinterpret_cast", "return", "sizeof",
    "static", "static_cast", "struct", "switch", "template", "this",
    "throw", "true", "try", "typedef", "typeid", "typename", "union",
    "using", "virtual", "volatile", "wchar_t", "while", "xor",
    "xor_eq", "synchronized",
    // Qt specific
    "signals", "slots", "emit", 0
};

/*
    @char
    @class
    @comment
    @function
    @keyword
    @number
    @op
    @preprocessor
    @string
    @type
*/

QString CppCodeMarker::addMarkUp(const QString &in,
                                 const Node * /* relative */,
                                 const Location & /* location */)
{
#define readChar() \
    ch = (i < (int)code.length()) ? code[i++].cell() : EOF

    QString code = in;

    QMap<QString, int> types;
    QMap<QString, int> keywords;
    int j = 0;
    while (typeTable[j] != 0) {
	types.insert(QString(typeTable[j]), 0);
	j++;
    }
    j = 0;
    while (keywordTable[j] != 0) {
	keywords.insert(QString(keywordTable[j]), 0);
	j++;
    }

    QString out("");
    int braceDepth = 0;
    int parenDepth = 0;
    int i = 0;
    int start = 0;
    int finish = 0;
    QChar ch;
    QRegExp classRegExp("Qt?(?:[A-Z3]+[a-z][A-Za-z]*|t)");
    QRegExp functionRegExp("q([A-Z][a-z]+)+");

    readChar();

    while (ch != EOF) {
	QString tag;
        bool target = false;

	if (ch.isLetter() || ch == '_') {
	    QString ident;
	    do {
            ident += ch;
            finish = i;
            readChar();
        } while (ch.isLetterOrNumber() || ch == '_');

	    if (classRegExp.exactMatch(ident)) {
		tag = QLatin1String("type");
            } else if (functionRegExp.exactMatch(ident)) {
                tag = QLatin1String("func");
                target = true;
	    } else if (types.contains(ident)) {
		tag = QLatin1String("type");
	    } else if (keywords.contains(ident)) {
		tag = QLatin1String("keyword");
	    } else if (braceDepth == 0 && parenDepth == 0) {
		if (QString(code.unicode() + i - 1, code.length() - (i - 1))
		     .indexOf(QRegExp(QLatin1String("^\\s*\\("))) == 0)
		    tag = QLatin1String("func");
                    target = true;
	    }
	} else if (ch.isDigit()) {
	    do {
                finish = i;
		readChar();
	    } while (ch.isLetterOrNumber() || ch == '.');
	    tag = QLatin1String("number");
	} else {
	    switch (ch.unicode()) {
	    case '+':
	    case '-':
	    case '!':
	    case '%':
	    case '^':
	    case '&':
	    case '*':
	    case ',':
	    case '.':
	    case '<':
	    case '=':
	    case '>':
	    case '?':
	    case '[':
	    case ']':
	    case '|':
	    case '~':
                finish = i;
		readChar();
		tag = QLatin1String("op");
		break;
	    case '"':
                finish = i;
		readChar();

		while (ch != EOF && ch != '"') {
		    if (ch == '\\')
			readChar();
		    readChar();
		}
                finish = i;
		readChar();
		tag = QLatin1String("string");
		break;
	    case '#':
                finish = i;
		readChar();
		while (ch != EOF && ch != '\n') {
		    if (ch == '\\')
			readChar();
                    finish = i;
		    readChar();
		}
		tag = QLatin1String("preprocessor");
		break;
	    case '\'':
                finish = i;
		readChar();

		while (ch != EOF && ch != '\'') {
		    if (ch == '\\')
			readChar();
		    readChar();
		}
                finish = i;
		readChar();
		tag = QLatin1String("char");
		break;
	    case '(':
                finish = i;
		readChar();
		parenDepth++;
		break;
	    case ')':
                finish = i;
		readChar();
		parenDepth--;
		break;
	    case ':':
                finish = i;
		readChar();
		if (ch == ':') {
                    finish = i;
		    readChar();
		    tag = QLatin1String("op");
		}
		break;
	    case '/':
                finish = i;
		readChar();
		if (ch == '/') {
		    do {
                        finish = i;
			readChar();
		    } while (ch != EOF && ch != '\n');
		    tag = QLatin1String("comment");
		} else if (ch == '*') {
		    bool metAster = false;
		    bool metAsterSlash = false;

                    finish = i;
		    readChar();

		    while (!metAsterSlash) {
			if (ch == EOF)
			    break;

			if (ch == '*')
			    metAster = true;
			else if (metAster && ch == '/')
			    metAsterSlash = true;
			else
			    metAster = false;
                        finish = i;
			readChar();
		    }
		    tag = QLatin1String("comment");
		} else {
		    tag = QLatin1String("op");
		}
		break;
	    case '{':
                finish = i;
		readChar();
		braceDepth++;
		break;
	    case '}':
                finish = i;
		readChar();
		braceDepth--;
		break;
	    default:
                finish = i;
		readChar();
	    }
	}

        QString text;
        text = code.mid(start, finish - start);
        start = finish;

	if (!tag.isEmpty()) {
	    out += QLatin1String("<@") + tag;
            if (target)
                out += QLatin1String(" target=\"") + text + QLatin1String("()\"");
            out += QLatin1String(">");
        }

        out += protect(text);

	if (!tag.isEmpty())
	    out += QLatin1String("</@") + tag + QLatin1String(">");
    }

    if (start < code.length()) {
        out += protect(code.mid(start));
    }

    return out;
}

#ifdef QDOC_QML
/*!
  This function is for documenting QML properties. It returns
  the list of documentation sections for the children of the
  \a qmlClassNode.

  Currently, it only handles QML property groups.
 */
QList<Section> CppCodeMarker::qmlSections(const QmlClassNode* qmlClassNode,
                                          SynopsisStyle style,
                                          const Tree* tree)
{
    QList<Section> sections;
    if (qmlClassNode) {
        if (style == Summary) {
	    FastSection qmlproperties(qmlClassNode,
                                      "Properties",
                                      "",
                                      "property",
                                      "properties");
	    FastSection qmlattachedproperties(qmlClassNode,
                                              "Attached Properties",
                                              "",
                                              "property",
                                              "properties");
	    FastSection qmlsignals(qmlClassNode,
                                   "Signal Handlers",
                                   "",
                                   "signal handler",
                                   "signal handlers");
	    FastSection qmlattachedsignals(qmlClassNode,
                                           "Attached Signal Handlers",
                                           "",
                                           "signal handler",
                                           "signal handlers");
	    FastSection qmlmethods(qmlClassNode,
                                   "Methods",
                                   "",
                                   "method",
                                   "methods");
	    FastSection qmlattachedmethods(qmlClassNode,
                                           "Attached Methods",
                                           "",
                                           "method",
                                           "methods");

            NodeList::ConstIterator c = qmlClassNode->childNodes().begin();
            while (c != qmlClassNode->childNodes().end()) {
                if ((*c)->subType() == Node::QmlPropertyGroup) {
                    const QmlPropGroupNode* qpgn = static_cast<const QmlPropGroupNode*>(*c);
                    NodeList::ConstIterator p = qpgn->childNodes().begin();
                    while (p != qpgn->childNodes().end()) {
                        if ((*p)->type() == Node::QmlProperty) {
                            const QmlPropertyNode* pn = static_cast<const QmlPropertyNode*>(*p);
                            if (pn->isAttached())
                                insert(qmlattachedproperties,*p,style,Okay);
                            else
                                insert(qmlproperties,*p,style,Okay);
                        }
                        ++p;
                    }
                }
                else if ((*c)->type() == Node::QmlSignal) {
                    const FunctionNode* sn = static_cast<const FunctionNode*>(*c);
                    if (sn->isAttached())
                        insert(qmlattachedsignals,*c,style,Okay);
                    else
                        insert(qmlsignals,*c,style,Okay);
                }
                else if ((*c)->type() == Node::QmlMethod) {
                    const FunctionNode* mn = static_cast<const FunctionNode*>(*c);
                    if (mn->isAttached())
                        insert(qmlattachedmethods,*c,style,Okay);
                    else
                        insert(qmlmethods,*c,style,Okay);
                }
                ++c;
            }
	    append(sections,qmlproperties);
	    append(sections,qmlattachedproperties);
	    append(sections,qmlsignals);
	    append(sections,qmlattachedsignals);
	    append(sections,qmlmethods);
	    append(sections,qmlattachedmethods);
        }
        else if (style == Detailed) {
            FastSection qmlproperties(qmlClassNode, "Property Documentation","qmlprop","member","members");
	    FastSection qmlattachedproperties(qmlClassNode,"Attached Property Documentation","qmlattprop",
                                              "member","members");
            FastSection qmlsignals(qmlClassNode,"Signal Handler Documentation","qmlsig","handler","handlers");
	    FastSection qmlattachedsignals(qmlClassNode,"Attached Signal Handler Documentation","qmlattsig",
                                           "handler","handlers");
            FastSection qmlmethods(qmlClassNode,"Method Documentation","qmlmeth","member","members");
	    FastSection qmlattachedmethods(qmlClassNode,"Attached Method Documentation","qmlattmeth",
                                           "member","members");
	    NodeList::ConstIterator c = qmlClassNode->childNodes().begin();
	    while (c != qmlClassNode->childNodes().end()) {
                if ((*c)->subType() == Node::QmlPropertyGroup) {
                    const QmlPropGroupNode* pgn = static_cast<const QmlPropGroupNode*>(*c);
                    if (pgn->isAttached())
                        insert(qmlattachedproperties,*c,style,Okay);
                    else
                        insert(qmlproperties,*c,style,Okay);
	        }
                else if ((*c)->type() == Node::QmlSignal) {
                    const FunctionNode* sn = static_cast<const FunctionNode*>(*c);
                    if (sn->isAttached())
                        insert(qmlattachedsignals,*c,style,Okay);
                    else
                        insert(qmlsignals,*c,style,Okay);
                }
                else if ((*c)->type() == Node::QmlMethod) {
                    const FunctionNode* mn = static_cast<const FunctionNode*>(*c);
                    if (mn->isAttached())
                        insert(qmlattachedmethods,*c,style,Okay);
                    else
                        insert(qmlmethods,*c,style,Okay);
                }
	        ++c;
	    }
	    append(sections,qmlproperties);
	    append(sections,qmlattachedproperties);
	    append(sections,qmlsignals);
	    append(sections,qmlattachedsignals);
	    append(sections,qmlmethods);
	    append(sections,qmlattachedmethods);
        }
        else {
	    FastSection all(qmlClassNode,"","","member","members");

	    QStack<const QmlClassNode*> stack;
	    stack.push(qmlClassNode);

	    while (!stack.isEmpty()) {
	        const QmlClassNode* ancestorClass = stack.pop();

	        NodeList::ConstIterator c = ancestorClass->childNodes().begin();
	        while (c != ancestorClass->childNodes().end()) {
                    //		    if ((*c)->access() != Node::Private)
                    if ((*c)->subType() == Node::QmlPropertyGroup) {
                        const QmlPropGroupNode* qpgn = static_cast<const QmlPropGroupNode*>(*c);
                        NodeList::ConstIterator p = qpgn->childNodes().begin();
                        while (p != qpgn->childNodes().end()) {
                            if ((*p)->type() == Node::QmlProperty) {
                                insert(all,*p,style,Okay);
                            }
                            ++p;
                        }
                    }
                    else
                        insert(all,*c,style,Okay);
                    ++c;
                }

                if (!ancestorClass->links().empty()) {
                    if (ancestorClass->links().contains(Node::InheritsLink)) {
                        QPair<QString,QString> linkPair;
                        linkPair = ancestorClass->links()[Node::InheritsLink];
                        QStringList strList(linkPair.first);
                        const Node* n = tree->findNode(strList,Node::Fake);
                        if (n && n->subType() == Node::QmlClass) {
                            const QmlClassNode* qcn = static_cast<const QmlClassNode*>(n);
                            stack.prepend(qcn);
                        }
                    }
                }
	    }
	    append(sections, all);
        }
    }

    return sections;
}
#endif

QT_END_NAMESPACE
