/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

/*
  cppcodemarker.cpp
*/

#include "atom.h"
#include "cppcodemarker.h"
#include "node.h"
#include "text.h"
#include "tree.h"
#include <qdebug.h>
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
  Returns \c true.
 */
bool CppCodeMarker::recognizeCode(const QString & /* code */)
{
    return true;
}

/*!
  Returns \c true if \a ext is any of a list of file extensions
  for the C++ language.
 */
bool CppCodeMarker::recognizeExtension(const QString& extension)
{
    QByteArray ext = extension.toLatin1();
    return ext == "c" ||
            ext == "c++" ||
            ext == "qdoc" ||
            ext == "qtt" ||
            ext == "qtx" ||
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
  Returns \c true if \a lang is either "C" or "Cpp".
 */
bool CppCodeMarker::recognizeLanguage(const QString &lang)
{
    return lang == QLatin1String("C") || lang == QLatin1String("Cpp");
}

/*!
  Returns the type of atom used to represent C++ code in the documentation.
*/
Atom::AtomType CppCodeMarker::atomType() const
{
    return Atom::Code;
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

    if ((style == Detailed) && !node->parent()->name().isEmpty() &&
        (node->type() != Node::Property) && !node->isQmlNode() && !node->isJsNode())
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
    case Node::QmlSignalHandler:
    case Node::QmlMethod:
        func = (const FunctionNode *) node;

        if (style != Subpage && !func->returnType().isEmpty())
            synopsis = typified(func->returnType(), true);
        synopsis += name;
        if (!func->isMacroWithoutParams()) {
            synopsis += QLatin1Char('(');
            if (!func->parameters().isEmpty()) {
                QVector<Parameter>::ConstIterator p = func->parameters().constBegin();
                while (p != func->parameters().constEnd()) {
                    if (p != func->parameters().constBegin())
                        synopsis += ", ";
                    bool hasName = !(*p).name().isEmpty();
                    if (hasName)
                        synopsis += typified((*p).dataType(), true);
                    const QString &paramName = hasName ? (*p).name() : (*p).dataType();
                    if (style != Subpage || !hasName)
                        synopsis += "<@param>" + protect(paramName) + "</@param>";
                    synopsis += protect((*p).rightType());
                    if (style != Subpage && !(*p).defaultValue().isEmpty())
                        synopsis += " = " + protect((*p).defaultValue());
                    ++p;
                }
            }
            synopsis += QLatin1Char(')');
        }
        if (func->isConst())
            synopsis += " const";

        if (style == Summary || style == Accessors) {
            if (!func->isNonvirtual())
                synopsis.prepend("virtual ");
            if (func->isFinal())
                synopsis.append(" final");
            if (func->isPureVirtual())
                synopsis.append(" = 0");
            else if (func->isDeleted())
                synopsis.append(" = delete");
            else if (func->isDefaulted())
               synopsis.append(" = default");
        }
        else if (style == Subpage) {
            if (!func->returnType().isEmpty() && func->returnType() != "void")
                synopsis += " : " + typified(func->returnType());
        }
        else {
            QStringList bracketed;
            if (func->isStatic()) {
                bracketed += "static";
            } else if (func->isDeleted()) {
                bracketed += "delete";
            } else if (func->isDefaulted()) {
                bracketed += "default";
            } else if (!func->isNonvirtual()) {
                if (func->isFinal())
                    bracketed += "final";
                if (func->isPureVirtual())
                    bracketed += "pure";
                bracketed += "virtual";
            }

            if (func->access() == Node::Protected) {
                bracketed += "protected";
            }
            else if (func->access() == Node::Private) {
                bracketed += "private";
            }

            if (func->isSignal()) {
                bracketed += "signal";
            }
            else if (func->isSlot()) {
                bracketed += "slot";
            }
            if (!bracketed.isEmpty())
                extra += QLatin1Char('[') + bracketed.join(' ') + QStringLiteral("] ");
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
                synopsis += QLatin1Char(' ');
            synopsis += QLatin1Char('}');
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
        if (style == Subpage) {
            synopsis = name + " : " + typified(variable->dataType());
        }
        else {
            synopsis = typified(variable->leftType(), true) +
                    name + protect(variable->rightType());
        }
        break;
    default:
        synopsis = name;
    }

    if (style == Summary) {
        if (node->status() == Node::Preliminary) {
            extra += "(preliminary) ";
        }
        else if (node->status() == Node::Deprecated) {
            extra += "(deprecated) ";
        }
        else if (node->status() == Node::Obsolete) {
            extra += "(obsolete) ";
        }
    }

    if (!extra.isEmpty()) {
        extra.prepend("<@extra>");
        extra.append("</@extra>");
    }
    return extra + synopsis;
}

/*!
 */
QString CppCodeMarker::markedUpQmlItem(const Node* node, bool summary)
{
    QString name = taggedQmlNode(node);
    if (summary)
        name = linkTag(node,name);
    else if (node->isQmlProperty() || node->isJsProperty()) {
        const QmlPropertyNode* pn = static_cast<const QmlPropertyNode*>(node);
        if (pn->isAttached())
            name.prepend(pn->element() + QLatin1Char('.'));
    }
    name = "<@name>" + name + "</@name>";
    QString synopsis;
    if (node->isQmlProperty() || node->isJsProperty()) {
        const QmlPropertyNode* pn = static_cast<const QmlPropertyNode*>(node);
        synopsis = name + " : " + typified(pn->dataType());
    }
    else if ((node->type() == Node::QmlMethod) ||
             (node->type() == Node::QmlSignal) ||
             (node->type() == Node::QmlSignalHandler)) {
        const FunctionNode* func = static_cast<const FunctionNode*>(node);
        if (!func->returnType().isEmpty())
            synopsis = typified(func->returnType(), true) + name;
        else
            synopsis = name;
        synopsis += QLatin1Char('(');
        if (!func->parameters().isEmpty()) {
            QVector<Parameter>::ConstIterator p = func->parameters().constBegin();
            while (p != func->parameters().constEnd()) {
                if (p != func->parameters().constBegin())
                    synopsis += ", ";
                bool hasName = !(*p).name().isEmpty();
                if (hasName)
                    synopsis += typified((*p).dataType(), true);
                const QString &paramName = hasName ? (*p).name() : (*p).dataType();
                synopsis += "<@param>" + protect(paramName) + "</@param>";
                synopsis += protect((*p).rightType());
                ++p;
            }
        }
        synopsis += QLatin1Char(')');
    }
    else
        synopsis = name;

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

QString CppCodeMarker::markedUpEnumValue(const QString &enumValue, const Node *relative)
{
    if (relative->type() != Node::Enum)
        return enumValue;

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

    QStringList::ConstIterator inc = includes.constBegin();
    while (inc != includes.constEnd()) {
        code += "<@preprocessor>#include &lt;<@headerfile>" + *inc + "</@headerfile>&gt;</@preprocessor>\n";
        ++inc;
    }
    return code;
}

QString CppCodeMarker::functionBeginRegExp(const QString& funcName)
{
    return QLatin1Char('^') + QRegExp::escape(funcName) + QLatin1Char('$');

}

QString CppCodeMarker::functionEndRegExp(const QString& /* funcName */)
{
    return "^\\}$";
}

QList<Section> CppCodeMarker::sections(const Aggregate *inner,
                                       SynopsisStyle style,
                                       Status status)
{
    QList<Section> sections;

    if (inner->isClass()) {
        if (style == Summary) {
            FastSection privateFunctions(inner,
                                         "Private Functions",
                                         QString(),
                                         "private function",
                                         "private functions");
            FastSection privateSlots(inner, "Private Slots", QString(), "private slot", "private slots");
            FastSection privateTypes(inner, "Private Types", QString(), "private type", "private types");
            FastSection protectedFunctions(inner,
                                           "Protected Functions",
                                           QString(),
                                           "protected function",
                                           "protected functions");
            FastSection protectedSlots(inner,
                                       "Protected Slots",
                                       QString(),
                                       "protected slot",
                                       "protected slots");
            FastSection protectedTypes(inner,
                                       "Protected Types",
                                       QString(),
                                       "protected type",
                                       "protected types");
            FastSection protectedVariables(inner,
                                           "Protected Variables",
                                           QString(),
                                           "protected type",
                                           "protected variables");
            FastSection publicFunctions(inner,
                                        "Public Functions",
                                        QString(),
                                        "public function",
                                        "public functions");
            FastSection publicSignals(inner, "Signals", QString(), "signal", "signals");
            FastSection publicSlots(inner, "Public Slots", QString(), "public slot", "public slots");
            FastSection publicTypes(inner, "Public Types", QString(), "public type", "public types");
            FastSection publicVariables(inner,
                                        "Public Variables",
                                        QString(),
                                        "public variable",
                                        "public variables");
            FastSection properties(inner, "Properties", QString(), "property", "properties");
            FastSection relatedNonMembers(inner,
                                          "Related Non-Members",
                                          QString(),
                                          "related non-member",
                                          "related non-members");
            FastSection staticPrivateMembers(inner,
                                             "Static Private Members",
                                             QString(),
                                             "static private member",
                                             "static private members");
            FastSection staticProtectedMembers(inner,
                                               "Static Protected Members",
                                               QString(),
                                               "static protected member",
                                               "static protected members");
            FastSection staticPublicMembers(inner,
                                            "Static Public Members",
                                            QString(),
                                            "static public member",
                                            "static public members");
            FastSection macros(inner, "Macros", QString(), "macro", "macros");

            NodeList::ConstIterator r = inner->relatedNodes().constBegin();
            while (r != inner->relatedNodes().constEnd()) {
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

            QStack<const Aggregate *> stack;
            stack.push(inner);
            while (!stack.isEmpty()) {
                const Aggregate* ancestor = stack.pop();

                NodeList::ConstIterator c = ancestor->childNodes().constBegin();
                while (c != ancestor->childNodes().constEnd()) {
                    bool isSlot = false;
                    bool isSignal = false;
                    bool isStatic = false;
                    if ((*c)->type() == Node::Function) {
                        const FunctionNode *func = (const FunctionNode *) *c;
                        isSlot = (func->isSlot());
                        isSignal = (func->isSignal());
                        isStatic = func->isStatic();
                        if (func->hasAssociatedProperties() && !func->hasActiveAssociatedProperty()) {
                            ++c;
                            continue;
                        }
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
                            if ((*c)->type() != Node::Variable || !(*c)->doc().isEmpty())
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
                            if (!insertReimpFunc(publicFunctions,*c,status)) {
                                insert(publicFunctions, *c, style, status);
                            }
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
                            if ((*c)->type() != Node::Variable || !(*c)->doc().isEmpty())
                                insert(staticProtectedMembers,*c,style,status);
                        }
                        else if ((*c)->type() == Node::Variable) {
                            if (!(*c)->doc().isEmpty())
                                insert(protectedVariables,*c,style,status);
                        }
                        else if ((*c)->type() == Node::Function) {
                            if (!insertReimpFunc(protectedFunctions,*c,status)) {
                                insert(protectedFunctions, *c, style, status);
                            }
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
                            if ((*c)->type() != Node::Variable || !(*c)->doc().isEmpty())
                                insert(staticPrivateMembers,*c,style,status);
                        }
                        else if ((*c)->type() == Node::Function) {
                            if (!insertReimpFunc(privateFunctions,*c,status)) {
                                insert(privateFunctions, *c, style, status);
                            }
                        }
                        else {
                            insert(privateTypes,*c,style,status);
                        }
                    }
                    ++c;
                }

                if (ancestor->isClass()) {
                    const ClassNode* cn = static_cast<const ClassNode*>(ancestor);
                    QList<RelatedClass>::ConstIterator r = cn->baseClasses().constBegin();
                    while (r != cn->baseClasses().constEnd()) {
                        if ((*r).node_)
                            stack.prepend((*r).node_);
                        ++r;
                    }
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
            FastSection memberFunctions(inner,"Member Function Documentation","func","member","members");
            FastSection memberTypes(inner,"Member Type Documentation","types","member","members");
            FastSection memberVariables(inner,"Member Variable Documentation","vars","member","members");
            FastSection properties(inner,"Property Documentation","prop","member","members");
            FastSection relatedNonMembers(inner,"Related Non-Members","relnonmem","member","members");
            FastSection macros(inner,"Macro Documentation","macros","member","members");

            NodeList::ConstIterator r = inner->relatedNodes().constBegin();
            while (r != inner->relatedNodes().constEnd()) {
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

            NodeList::ConstIterator c = inner->childNodes().constBegin();
            while (c != inner->childNodes().constEnd()) {
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
                    if (!function->hasAssociatedProperties() || !function->doc().isEmpty())
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
            FastSection all(inner,QString(),QString(),"member","members");

            QStack<const Aggregate*> stack;
            stack.push(inner);

            while (!stack.isEmpty()) {
                const Aggregate* ancestor = stack.pop();
                NodeList::ConstIterator c = ancestor->childNodes().constBegin();
                while (c != ancestor->childNodes().constEnd()) {
                    if ((*c)->access() != Node::Private && (*c)->type() != Node::Property)
                        insert(all, *c, style, status);
                    ++c;
                }

                if (ancestor->isClass()) {
                    const ClassNode* cn = static_cast<const ClassNode*>(ancestor);
                    QList<RelatedClass>::ConstIterator r = cn->baseClasses().constBegin();
                    while (r != cn->baseClasses().constEnd()) {
                        if ((*r).node_)
                            stack.prepend((*r).node_);
                        ++r;
                    }
                }
            }
            append(sections, all);
        }
    }
    else {
        if (style == Summary || style == Detailed) {
            FastSection namespaces(inner,
                                   "Namespaces",
                                   style == Detailed ? "nmspace" : QString(),
                                   "namespace",
                                   "namespaces");
            FastSection classes(inner,
                                "Classes",
                                style == Detailed ? "classes" : QString(),
                                "class",
                                "classes");
            FastSection types(inner,
                              style == Summary ? "Types" : "Type Documentation",
                              style == Detailed ? "types" : QString(),
                              "type",
                              "types");
            FastSection variables(inner,
                                  style == Summary ? "Variables" : "Variable Documentation",
                                  style == Detailed ? "vars" : QString(),
                                  "variable",
                                  "variables");
            FastSection staticVariables(inner,
                                        "Static Variables",
                                        QString(),
                                        "static variable",
                                        "static variables");
            FastSection functions(inner,
                                  style == Summary ?
                                      "Functions" : "Function Documentation",
                                  style == Detailed ? "func" : QString(),
                                  "function",
                                  "functions");
            FastSection macros(inner,
                               style == Summary ?
                                   "Macros" : "Macro Documentation",
                               style == Detailed ? "macros" : QString(),
                               "macro",
                               "macros");

            NodeList nodeList = inner->childNodes();
            nodeList += inner->relatedNodes();

            NodeList::ConstIterator n = nodeList.constBegin();
            while (n != nodeList.constEnd()) {
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
                case Node::Variable:
                    {
                        const VariableNode* var = static_cast<const VariableNode*>(*n);
                        if (!var->doc().isEmpty()) {
                            if (var->isStatic())
                                insert(staticVariables,*n,style,status);
                            else
                                insert(variables, *n, style, status);
                        }
                    }
                    break;
                default:
                    break;
                }
                ++n;
            }
            if (inner->isNamespace()) {
                const NamespaceNode* ns = static_cast<const NamespaceNode*>(inner);
                if (!ns->orphans().isEmpty()) {
                    foreach (Node* n, ns->orphans()) {
                        // Use inner as a temporary parent when inserting orphans
                        Aggregate* p = n->parent();
                        n->setParent(const_cast<Aggregate*>(inner));
                        if (n->isClass())
                            insert(classes, n, style, status);
                        else if (n->isNamespace())
                            insert(namespaces, n, style, status);
                        n->setParent(p);
                    }
                }
            }
            append(sections, namespaces);
            append(sections, classes);
            append(sections, types);
            append(sections, variables);
            append(sections, staticVariables);
            append(sections, functions);
            append(sections, macros);
        }
    }

    return sections;
}

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
    static QSet<QString> types;
    static QSet<QString> keywords;

    if (types.isEmpty()) {
        // initialize statics
        Q_ASSERT(keywords.isEmpty());
        static const QString typeTable[] = {
            QLatin1String("bool"), QLatin1String("char"), QLatin1String("double"), QLatin1String("float"), QLatin1String("int"), QLatin1String("long"), QLatin1String("short"),
            QLatin1String("signed"), QLatin1String("unsigned"), QLatin1String("uint"), QLatin1String("ulong"), QLatin1String("ushort"), QLatin1String("uchar"), QLatin1String("void"),
            QLatin1String("qlonglong"), QLatin1String("qulonglong"),
            QLatin1String("qint"), QLatin1String("qint8"), QLatin1String("qint16"), QLatin1String("qint32"), QLatin1String("qint64"),
            QLatin1String("quint"), QLatin1String("quint8"), QLatin1String("quint16"), QLatin1String("quint32"), QLatin1String("quint64"),
            QLatin1String("qreal"), QLatin1String("cond")
        };

        static const QString keywordTable[] = {
            QLatin1String("and"), QLatin1String("and_eq"), QLatin1String("asm"), QLatin1String("auto"), QLatin1String("bitand"), QLatin1String("bitor"), QLatin1String("break"),
            QLatin1String("case"), QLatin1String("catch"), QLatin1String("class"), QLatin1String("compl"), QLatin1String("const"), QLatin1String("const_cast"),
            QLatin1String("continue"), QLatin1String("default"), QLatin1String("delete"), QLatin1String("do"), QLatin1String("dynamic_cast"), QLatin1String("else"),
            QLatin1String("enum"), QLatin1String("explicit"), QLatin1String("export"), QLatin1String("extern"), QLatin1String("false"), QLatin1String("for"), QLatin1String("friend"),
            QLatin1String("goto"), QLatin1String("if"), QLatin1String("include"), QLatin1String("inline"), QLatin1String("monitor"), QLatin1String("mutable"), QLatin1String("namespace"),
            QLatin1String("new"), QLatin1String("not"), QLatin1String("not_eq"), QLatin1String("operator"), QLatin1String("or"), QLatin1String("or_eq"), QLatin1String("private"), QLatin1String("protected"),
            QLatin1String("public"), QLatin1String("register"), QLatin1String("reinterpret_cast"), QLatin1String("return"), QLatin1String("sizeof"),
            QLatin1String("static"), QLatin1String("static_cast"), QLatin1String("struct"), QLatin1String("switch"), QLatin1String("template"), QLatin1String("this"),
            QLatin1String("throw"), QLatin1String("true"), QLatin1String("try"), QLatin1String("typedef"), QLatin1String("typeid"), QLatin1String("typename"), QLatin1String("union"),
            QLatin1String("using"), QLatin1String("virtual"), QLatin1String("volatile"), QLatin1String("wchar_t"), QLatin1String("while"), QLatin1String("xor"),
            QLatin1String("xor_eq"), QLatin1String("synchronized"),
            // Qt specific
            QLatin1String("signals"), QLatin1String("slots"), QLatin1String("emit")
        };

        types.reserve(sizeof(typeTable) / sizeof(QString));
        for (int j = sizeof(typeTable) / sizeof(QString) - 1; j; --j)
            types.insert(typeTable[j]);

        keywords.reserve(sizeof(keywordTable) / sizeof(QString));
        for (int j = sizeof(keywordTable) / sizeof(QString) - 1; j; --j)
            keywords.insert(keywordTable[j]);
    }
#define readChar() \
    ch = (i < (int)code.length()) ? code[i++].cell() : EOF

    QString code = in;
    QString out;
    QStringRef text;
    int braceDepth = 0;
    int parenDepth = 0;
    int i = 0;
    int start = 0;
    int finish = 0;
    QChar ch;
    QRegExp classRegExp("Qt?(?:[A-Z3]+[a-z][A-Za-z]*|t)");
    QRegExp functionRegExp("q([A-Z][a-z]+)+");
    QRegExp findFunctionRegExp(QStringLiteral("^\\s*\\("));

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
                tag = QStringLiteral("type");
            } else if (functionRegExp.exactMatch(ident)) {
                tag = QStringLiteral("func");
                target = true;
            } else if (types.contains(ident)) {
                tag = QStringLiteral("type");
            } else if (keywords.contains(ident)) {
                tag = QStringLiteral("keyword");
            } else if (braceDepth == 0 && parenDepth == 0) {
                if (code.indexOf(findFunctionRegExp, i - 1) == i - 1)
                    tag = QStringLiteral("func");
                target = true;
            }
        } else if (ch.isDigit()) {
            do {
                finish = i;
                readChar();
            } while (ch.isLetterOrNumber() || ch == '.');
            tag = QStringLiteral("number");
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
                tag = QStringLiteral("op");
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
                tag = QStringLiteral("string");
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
                tag = QStringLiteral("preprocessor");
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
                tag = QStringLiteral("char");
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
                    tag = QStringLiteral("op");
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
                    tag = QStringLiteral("comment");
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
                    tag = QStringLiteral("comment");
                } else {
                    tag = QStringLiteral("op");
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

        text = code.midRef(start, finish - start);
        start = finish;

        if (!tag.isEmpty()) {
            out += QStringLiteral("<@");
            out += tag;
            if (target) {
                out += QStringLiteral(" target=\"");
                out += text;
                out += QStringLiteral("()\"");
            }
            out += QStringLiteral(">");
        }

        appendProtectedString(&out, text);

        if (!tag.isEmpty()) {
            out += QStringLiteral("</@");
            out += tag;
            out += QStringLiteral(">");
        }
    }

    if (start < code.length()) {
        appendProtectedString(&out, code.midRef(start));
    }

    return out;
}

/*!
  This function is for documenting QML properties. It returns
  the list of documentation sections for the children of the
  \a aggregate.
 */
QList<Section> CppCodeMarker::qmlSections(Aggregate* aggregate, SynopsisStyle style, Status status)
{
    QList<Section> sections;
    if (aggregate) {
        if (style == Summary) {
            FastSection qmlproperties(aggregate,
                                      "Properties",
                                      QString(),
                                      "property",
                                      "properties");
            FastSection qmlattachedproperties(aggregate,
                                              "Attached Properties",
                                              QString(),
                                              "attached property",
                                              "attached properties");
            FastSection qmlsignals(aggregate,
                                   "Signals",
                                   QString(),
                                   "signal",
                                   "signals");
            FastSection qmlsignalhandlers(aggregate,
                                          "Signal Handlers",
                                          QString(),
                                          "signal handler",
                                          "signal handlers");
            FastSection qmlattachedsignals(aggregate,
                                           "Attached Signals",
                                           QString(),
                                           "attached signal",
                                           "attached signals");
            FastSection qmlmethods(aggregate,
                                   "Methods",
                                   QString(),
                                   "method",
                                   "methods");
            FastSection qmlattachedmethods(aggregate,
                                           "Attached Methods",
                                           QString(),
                                           "attached method",
                                           "attached methods");

            Aggregate* qcn = aggregate;
            while (qcn != 0) {
                NodeList::ConstIterator c = qcn->childNodes().constBegin();
                while (c != qcn->childNodes().constEnd()) {
                    if ((*c)->status() == Node::Internal) {
                        ++c;
                        continue;
                    }
                    if ((*c)->isQmlPropertyGroup() || (*c)->isJsPropertyGroup()) {
                        insert(qmlproperties, *c, style, status);
                    }
                    else if ((*c)->isQmlProperty() || (*c)->isJsProperty()) {
                        const QmlPropertyNode* pn = static_cast<const QmlPropertyNode*>(*c);
                        if (pn->isAttached())
                            insert(qmlattachedproperties,*c,style, status);
                        else {
                            insert(qmlproperties,*c,style, status);
                        }
                    }
                    else if ((*c)->isQmlSignal() || (*c)->isJsSignal()) {
                        const FunctionNode* sn = static_cast<const FunctionNode*>(*c);
                        if (sn->isAttached())
                            insert(qmlattachedsignals,*c,style, status);
                        else
                            insert(qmlsignals,*c,style, status);
                    }
                    else if ((*c)->isQmlSignalHandler() || (*c)->isJsSignalHandler()) {
                        insert(qmlsignalhandlers,*c,style, status);
                    }
                    else if ((*c)->isQmlMethod() || (*c)->isJsMethod()) {
                        const FunctionNode* mn = static_cast<const FunctionNode*>(*c);
                        if (mn->isAttached())
                            insert(qmlattachedmethods,*c,style, status);
                        else
                            insert(qmlmethods,*c,style, status);
                    }
                    ++c;
                }
                if (qcn->qmlBaseNode() != 0) {
                    qcn = static_cast<QmlTypeNode*>(qcn->qmlBaseNode());
                    if (!qcn->isAbstract())
                        qcn = 0;
                }
                else
                    qcn = 0;
            }
            append(sections,qmlproperties);
            append(sections,qmlattachedproperties);
            append(sections,qmlsignals);
            append(sections,qmlsignalhandlers);
            append(sections,qmlattachedsignals);
            append(sections,qmlmethods);
            append(sections,qmlattachedmethods);
        }
        else if (style == Detailed) {
            FastSection qmlproperties(aggregate, "Property Documentation","qmlprop","member","members");
            FastSection qmlattachedproperties(aggregate,"Attached Property Documentation","qmlattprop",
                                              "member","members");
            FastSection qmlsignals(aggregate,"Signal Documentation","qmlsig","signal","signals");
            FastSection qmlsignalhandlers(aggregate,"Signal Handler Documentation","qmlsighan","signal handler","signal handlers");
            FastSection qmlattachedsignals(aggregate,"Attached Signal Documentation","qmlattsig",
                                           "signal","signals");
            FastSection qmlmethods(aggregate,"Method Documentation","qmlmeth","member","members");
            FastSection qmlattachedmethods(aggregate,"Attached Method Documentation","qmlattmeth",
                                           "member","members");
            Aggregate* qcn = aggregate;
            while (qcn != 0) {
                NodeList::ConstIterator c = qcn->childNodes().constBegin();
                while (c != qcn->childNodes().constEnd()) {
                    if ((*c)->status() == Node::Internal) {
                        ++c;
                        continue;
                    }
                    if ((*c)->isQmlPropertyGroup() || (*c)->isJsPropertyGroup()) {
                        insert(qmlproperties,*c,style, status);
                    }
                    else if ((*c)->isQmlProperty() || (*c)->isJsProperty()) {
                        const QmlPropertyNode* pn = static_cast<const QmlPropertyNode*>(*c);
                        if (pn->isAttached())
                            insert(qmlattachedproperties,*c,style, status);
                        else
                            insert(qmlproperties,*c,style, status);
                    }
                    else if ((*c)->isQmlSignal() || (*c)->isJsSignal()) {
                        const FunctionNode* sn = static_cast<const FunctionNode*>(*c);
                        if (sn->isAttached())
                            insert(qmlattachedsignals,*c,style, status);
                        else
                            insert(qmlsignals,*c,style, status);
                    }
                    else if ((*c)->isQmlSignalHandler() || (*c)->isJsSignalHandler()) {
                        insert(qmlsignalhandlers,*c,style, status);
                    }
                    else if ((*c)->isQmlMethod() || (*c)->isJsMethod()) {
                        const FunctionNode* mn = static_cast<const FunctionNode*>(*c);
                        if (mn->isAttached())
                            insert(qmlattachedmethods,*c,style, status);
                        else
                            insert(qmlmethods,*c,style, status);
                    }
                    ++c;
                }
                if (qcn->qmlBaseNode() != 0) {
                    qcn = static_cast<QmlTypeNode*>(qcn->qmlBaseNode());
                    if (!qcn->isAbstract())
                        qcn = 0;
                }
                else
                    qcn = 0;
            }
            append(sections,qmlproperties);
            append(sections,qmlattachedproperties);
            append(sections,qmlsignals);
            append(sections,qmlsignalhandlers);
            append(sections,qmlattachedsignals);
            append(sections,qmlmethods);
            append(sections,qmlattachedmethods);
        }
        else {
            /*
              This is where the list of all members including inherited
              members is prepared.
             */
            ClassMap* classMap = 0;
            FastSection all(aggregate,QString(),QString(),"member","members");
            Aggregate* current = aggregate;
            while (current != 0) {
                /*
                  If the QML type is abstract, do not create
                  a new entry in the list for it. Instead,
                  add its members to the current entry.

                  However, if the first class is abstract,
                  there is no current entry. In that case,
                  create a new entry in the list anyway.
                  I'm not sure that is correct, but it at
                  least can prevent a crash.
                 */
                if (!current->isAbstract() || !classMap) {
                    classMap = new ClassMap;
                    classMap->first = static_cast<const QmlTypeNode*>(current);
                    all.classMapList_.append(classMap);
                }
                NodeList::ConstIterator c = current->childNodes().constBegin();
                while (c != current->childNodes().constEnd()) {
                    if ((*c)->isQmlPropertyGroup() || (*c)->isJsPropertyGroup()) {
                        const QmlPropertyGroupNode* qpgn = static_cast<const QmlPropertyGroupNode*>(*c);
                        NodeList::ConstIterator p = qpgn->childNodes().constBegin();
                        while (p != qpgn->childNodes().constEnd()) {
                            if ((*p)->isQmlProperty() || (*c)->isJsProperty()) {
                                QString key = (*p)->name();
                                key = sortName(*p, &key);
                                all.memberMap.insert(key,*p);
                                classMap->second.insert(key,*p);
                            }
                            ++p;
                        }
                    }
                    else {
                        QString key = (*c)->name();
                        key = sortName(*c, &key);
                        all.memberMap.insert(key,*c);
                        classMap->second.insert(key,*c);
                    }
                    ++c;
                }
                if (current->qmlBaseNode() == current) {
                    qDebug() << "qdoc internal error: circular type definition."
                             << "QML type" << current->name()
                             << "can't be its own base type";
                    break;
                }
                current = current->qmlBaseNode();
                while (current) {
                    if (current->isAbstract())
                        break;
                    if (current->isInternal())
                        current = current->qmlBaseNode();
                    else
                        break;
                }
            }
            append(sections, all, true);
        }
    }

    return sections;
}

QT_END_NAMESPACE
