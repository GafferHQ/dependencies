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
  cppcodeparser.cpp
*/

#include <qfile.h>

#include <stdio.h>
#include <errno.h>
#include <qdebug.h>
#include "codechunk.h"
#include "config.h"
#include "cppcodeparser.h"
#include "tokenizer.h"
#include "tree.h"

QT_BEGIN_NAMESPACE

/* qmake ignore Q_OBJECT */

#define COMMAND_CLASS                   Doc::alias("class")
#define COMMAND_CONTENTSPAGE            Doc::alias("contentspage")
#define COMMAND_ENUM                    Doc::alias("enum")
#define COMMAND_EXAMPLE                 Doc::alias("example")
#define COMMAND_EXTERNALPAGE            Doc::alias("externalpage")
#define COMMAND_FILE                    Doc::alias("file") // ### don't document
#define COMMAND_FN                      Doc::alias("fn")
#define COMMAND_GROUP                   Doc::alias("group")
#define COMMAND_HEADERFILE              Doc::alias("headerfile")
#define COMMAND_INDEXPAGE               Doc::alias("indexpage")
#define COMMAND_INHEADERFILE            Doc::alias("inheaderfile") // ### don't document
#define COMMAND_MACRO                   Doc::alias("macro")
#define COMMAND_MODULE                  Doc::alias("module") // ### don't document
#define COMMAND_NAMESPACE               Doc::alias("namespace")
#define COMMAND_OVERLOAD                Doc::alias("overload")
#define COMMAND_NEXTPAGE                Doc::alias("nextpage")
#define COMMAND_PAGE                    Doc::alias("page")
#define COMMAND_PREVIOUSPAGE            Doc::alias("previouspage")
#define COMMAND_PROPERTY                Doc::alias("property")
#define COMMAND_REIMP                   Doc::alias("reimp")
#define COMMAND_RELATES                 Doc::alias("relates")
#define COMMAND_SERVICE                 Doc::alias("service")
#define COMMAND_STARTPAGE               Doc::alias("startpage")
#define COMMAND_TYPEDEF                 Doc::alias("typedef")
#define COMMAND_VARIABLE                Doc::alias("variable")

#ifdef QDOC_QML
#define COMMAND_QMLCLASS                Doc::alias("qmlclass")
#define COMMAND_QMLPROPERTY             Doc::alias("qmlproperty")
#define COMMAND_QMLATTACHEDPROPERTY     Doc::alias("qmlattachedproperty")
#define COMMAND_QMLINHERITS             Doc::alias("inherits")
#define COMMAND_QMLSIGNAL               Doc::alias("qmlsignal")
#define COMMAND_QMLATTACHEDSIGNAL       Doc::alias("qmlattachedsignal")
#define COMMAND_QMLMETHOD               Doc::alias("qmlmethod")
#define COMMAND_QMLATTACHEDMETHOD       Doc::alias("qmlattachedmethod")
#define COMMAND_QMLDEFAULT              Doc::alias("default")
#define COMMAND_QMLBASICTYPE            Doc::alias("qmlbasictype")
#endif

#define COMMAND_AUDIENCE                Doc::alias("audience")
#define COMMAND_CATEGORY                Doc::alias("category")
#define COMMAND_PRODNAME                Doc::alias("prodname")
#define COMMAND_COMPONENT               Doc::alias("component")
#define COMMAND_AUTHOR                  Doc::alias("author")
#define COMMAND_PUBLISHER               Doc::alias("publisher")
#define COMMAND_COPYRYEAR               Doc::alias("copyryear")
#define COMMAND_COPYRHOLDER             Doc::alias("copyrholder")
#define COMMAND_PERMISSIONS             Doc::alias("permissions")
#define COMMAND_LIFECYCLEVERSION        Doc::alias("lifecycleversion")
#define COMMAND_LIFECYCLEWSTATUS        Doc::alias("lifecyclestatus")
#define COMMAND_LICENSEYEAR             Doc::alias("licenseyear")
#define COMMAND_LICENSENAME             Doc::alias("licensename")
#define COMMAND_LICENSEDESCRIPTION      Doc::alias("licensedescription")
#define COMMAND_RELEASEDATE             Doc::alias("releasedate")

QStringList CppCodeParser::exampleFiles;
QStringList CppCodeParser::exampleDirs;

static void extractPageLinkAndDesc(const QString &arg,
                                   QString *link,
                                   QString *desc)
{
    QRegExp bracedRegExp("\\{([^{}]*)\\}(?:\\{([^{}]*)\\})?");

    if (bracedRegExp.exactMatch(arg)) {
        *link = bracedRegExp.cap(1);
        *desc = bracedRegExp.cap(2);
        if (desc->isEmpty())
            *desc = *link;
    }
    else {
        int spaceAt = arg.indexOf(" ");
        if (arg.contains(".html") && spaceAt != -1) {
            *link = arg.left(spaceAt).trimmed();
            *desc = arg.mid(spaceAt).trimmed();
        }
        else {
            *link = arg;
            *desc = arg;
        }
    }
}

static void setLink(Node *node, Node::LinkType linkType, const QString &arg)
{
    QString link;
    QString desc;
    extractPageLinkAndDesc(arg, &link, &desc);
    node->setLink(linkType, link, desc);
}

/*
    This is used for fuzzy matching only, which in turn is only used
    for Qt Jambi.
*/
static QString cleanType(const QString &type, const Tree *tree)
{
    QString result = type;
    result.replace("qlonglong", "long long");
    result.replace("qulonglong", "unsigned long long");
    result.replace("qreal", "double");
    result.replace(QRegExp("\\bu(int|short|char|long)\\b"), "unsigned \\1");
    result.replace("QRgb", "unsigned int");
    result.replace(" >", ">");
    result.remove(" const[]");
    result.replace("QStringList<QString>", "QStringList");
    result.replace("qint8", "char");
    result.replace("qint16", "short");
    result.replace("qint32", "int");
    result.replace("qint64", "long long");
    result.replace("quint8", "unsigned char");
    result.replace("quint16", "unsigned short");
    result.replace("quint32", "unsigned int");
    result.replace("quint64", "unsigned long long");

    if (result.contains("QFlags")) {
        QRegExp regExp("QFlags<(((?:[^<>]+::)*)([^<>:]+))>");
        int pos = 0;
        while ((pos = result.indexOf(regExp, pos)) != -1) {
            // we assume that the path for the associated enum
            // is the same as for the flag typedef
            QStringList path = regExp.cap(2).split("::",
                                                   QString::SkipEmptyParts);
            const EnumNode *enume = static_cast<const EnumNode *>(
                             tree->findNode(QStringList(path) << regExp.cap(3),
                                            Node::Enum));
            if (enume && enume->flagsType())
                result.replace(pos, regExp.matchedLength(),
                 (QStringList(path) << enume->flagsType()->name()).join("::"));
            ++pos;
        }
    }
    if (result.contains("::")) {
        // remove needless (and needful) class prefixes
        QRegExp regExp("[A-Za-z0-9_]+::");
        result.replace(regExp, "");
    }
    return result;
}

/*!
  The constructor initializes some regular expressions
  and calls reset().
 */
CppCodeParser::CppCodeParser()
    : varComment("/\\*\\s*([a-zA-Z_0-9]+)\\s*\\*/"), sep("(?:<[^>]+>)?::")
{
    reset(0);
}

/*!
  The destructor is trivial.
 */
CppCodeParser::~CppCodeParser()
{
    // nothing.
}

/*!
  The constructor initializes a map of special node types
  for identifying important nodes. And it initializes
  some filters for identifying certain kinds of files.
 */
void CppCodeParser::initializeParser(const Config &config)
{
    CodeParser::initializeParser(config);

    nodeTypeMap.insert(COMMAND_NAMESPACE, Node::Namespace);
    nodeTypeMap.insert(COMMAND_CLASS, Node::Class);
    nodeTypeMap.insert(COMMAND_SERVICE, Node::Class);
    nodeTypeMap.insert(COMMAND_ENUM, Node::Enum);
    nodeTypeMap.insert(COMMAND_TYPEDEF, Node::Typedef);
    nodeTypeMap.insert(COMMAND_PROPERTY, Node::Property);
    nodeTypeMap.insert(COMMAND_VARIABLE, Node::Variable);

    exampleFiles = config.getStringList(CONFIG_EXAMPLES);
    exampleDirs = config.getStringList(CONFIG_EXAMPLEDIRS);
    QStringList exampleFilePatterns = config.getStringList(
        CONFIG_EXAMPLES + Config::dot + CONFIG_FILEEXTENSIONS);

    if (!exampleFilePatterns.isEmpty())
        exampleNameFilter = exampleFilePatterns.join(" ");
    else
        exampleNameFilter = "*.cpp *.h *.js *.xq *.svg *.xml *.ui";

    QStringList exampleImagePatterns = config.getStringList(
        CONFIG_EXAMPLES + Config::dot + CONFIG_IMAGEEXTENSIONS);

    if (!exampleImagePatterns.isEmpty())
        exampleImageFilter = exampleImagePatterns.join(" ");
    else
        exampleImageFilter = "*.png";
}

/*!
  Clear the map of common node types and call
  the same function in the base class.
 */
void CppCodeParser::terminateParser()
{
    nodeTypeMap.clear();
    CodeParser::terminateParser();
}

/*!
  Returns "Cpp".
 */
QString CppCodeParser::language()
{
    return "Cpp";
}

/*!
  Returns a list of extensions for header files.
 */
QStringList CppCodeParser::headerFileNameFilter()
{
    return QStringList() << "*.ch" << "*.h" << "*.h++" << "*.hh" << "*.hpp" << "*.hxx";
}

/*!
  Returns a list of extensions for source files, i.e. not
  header files.
 */
QStringList CppCodeParser::sourceFileNameFilter()
{
    return QStringList() << "*.c++" << "*.cc" << "*.cpp" << "*.cxx" << "*.mm";
}

/*!
  Parse the C++ header file identified by \a filePath
  and add the parsed contents to the big \a tree. The
  \a location is used for reporting errors.
 */
void CppCodeParser::parseHeaderFile(const Location& location,
                                    const QString& filePath,
                                    Tree *tree)
{
    QFile in(filePath);
    if (!in.open(QIODevice::ReadOnly)) {
        location.error(tr("Cannot open C++ header file '%1'").arg(filePath));
        return;
    }

    reset(tree);
    Location fileLocation(filePath);
    Tokenizer fileTokenizer(fileLocation, in);
    tokenizer = &fileTokenizer;
    readToken();
    matchDeclList(tree->root());
    if (!fileTokenizer.version().isEmpty())
        tree->setVersion(fileTokenizer.version());
    in.close();

    if (fileLocation.fileName() == "qiterator.h")
        parseQiteratorDotH(location, filePath);
}

/*!
  Get ready to parse the C++ cpp file identified by \a filePath
  and add its parsed contents to the big \a tree. \a location is
  used for reporting errors.

  Call matchDocsAndStuff() to do all the parsing and tree building.
 */
void CppCodeParser::parseSourceFile(const Location& location,
                                    const QString& filePath,
                                    Tree *tree)
{
    QFile in(filePath);
    if (!in.open(QIODevice::ReadOnly)) {
        location.error(tr("Cannot open C++ source file '%1' (%2)").arg(filePath).arg(strerror(errno)));
        return;
    }

    reset(tree);
    Location fileLocation(filePath);
    Tokenizer fileTokenizer(fileLocation, in);
    tokenizer = &fileTokenizer;
    readToken();
    usedNamespaces.clear();
    matchDocsAndStuff();
    in.close();
}

/*!
  This is called after all the header files have been parsed.
  I think the most important thing it does is resolve class
  inheritance links in the tree. But it also initializes a
  bunch of stuff.
 */
void CppCodeParser::doneParsingHeaderFiles(Tree *tree)
{
    tree->resolveInheritance();

    QMapIterator<QString, QString> i(sequentialIteratorClasses);
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(),
                                 i.value(),
                                 sequentialIteratorDefinition,
                                 tree);
    }
    i = mutableSequentialIteratorClasses;
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(),
                                 i.value(),
                                 mutableSequentialIteratorDefinition,
                                 tree);
    }
    i = associativeIteratorClasses;
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(),
                                 i.value(),
                                 associativeIteratorDefinition,
                                 tree);
    }
    i = mutableAssociativeIteratorClasses;
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(),
                                 i.value(),
                                 mutableAssociativeIteratorDefinition,
                                 tree);
    }
    sequentialIteratorDefinition.clear();
    mutableSequentialIteratorDefinition.clear();
    associativeIteratorDefinition.clear();
    mutableAssociativeIteratorDefinition.clear();
    sequentialIteratorClasses.clear();
    mutableSequentialIteratorClasses.clear();
    associativeIteratorClasses.clear();
    mutableAssociativeIteratorClasses.clear();
}

/*!
  This is called after all the source files (i.e., not the
  header files) have been parsed. It traverses the tree to
  resolve property links, normalize overload signatures, and
  do other housekeeping of the tree.
 */
void CppCodeParser::doneParsingSourceFiles(Tree *tree)
{
    tree->root()->makeUndocumentedChildrenInternal();
    tree->root()->normalizeOverloads();
    tree->fixInheritance();
    tree->resolveProperties();
}

/*!
  This function searches the \a tree to find a FunctionNode
  for a function with the signature \a synopsis. If the
  \a relative node is provided, the search begins there. If
  \a fuzzy is true, base classes are searched. The function
  node is returned, if found.
 */
const FunctionNode *CppCodeParser::findFunctionNode(const QString& synopsis,
                                                    Tree *tree,
                                                    Node *relative,
                                                    bool fuzzy)
{
    QStringList parentPath;
    FunctionNode *clone;
    FunctionNode *func = 0;
    int flags = fuzzy ? int(Tree::SearchBaseClasses) : 0;

    reset(tree);
    if (makeFunctionNode(synopsis, &parentPath, &clone)) {
        func = tree->findFunctionNode(parentPath, clone, relative, flags);

        /*
            This is necessary because Roberto's parser resolves typedefs.
        */
        if (!func && fuzzy) {
            func = tre->findFunctionNode(parentPath +
                                         QStringList(clone->name()),
                                         relative,
                                         flags);
            if (!func && clone->name().contains('_')) {
                QStringList path = parentPath;
                path << clone->name().split('_');
                func = tre->findFunctionNode(path, relative, flags);
            }

            if (func) {
                NodeList overloads = func->parent()->overloads(func->name());
                NodeList candidates;
                for (int i = 0; i < overloads.count(); ++i) {
                    FunctionNode *overload = static_cast<FunctionNode *>(overloads.at(i));
                    if (overload->status() != Node::Compat
                            && overload->parameters().count() == clone->parameters().count()
                            && !overload->isConst() == !clone->isConst())
                        candidates << overload;
                }
                if (candidates.count() == 0)
                    return 0;

                /*
                    There's only one function with the correct number
                    of parameters. That must be the one.
                */
                if (candidates.count() == 1)
                    return static_cast<FunctionNode *>(candidates.first());

                overloads = candidates;
                candidates.clear();
                for (int i = 0; i < overloads.count(); ++i) {
                    FunctionNode *overload = static_cast<FunctionNode *>(overloads.at(i));
                    QList<Parameter> params1 = overload->parameters();
                    QList<Parameter> params2 = clone->parameters();

                    int j;
                    for (j = 0; j < params1.count(); ++j) {
                        if (!params2.at(j).name().startsWith(params1.at(j).name()))
                            break;
                    }
                    if (j == params1.count())
                        candidates << overload;
                }

                /*
                    There are several functions with the correct
                    parameter count, but only one has the correct
                    parameter names.
                */
                if (candidates.count() == 1)
                    return static_cast<FunctionNode *>(candidates.first());

                candidates.clear();
                for (int i = 0; i < overloads.count(); ++i) {
                    FunctionNode *overload = static_cast<FunctionNode *>(overloads.at(i));
                    QList<Parameter> params1 = overload->parameters();
                    QList<Parameter> params2 = clone->parameters();

                    int j;
                    for (j = 0; j < params1.count(); ++j) {
                        if (params1.at(j).rightType() != params2.at(j).rightType())
                            break;

                        if (cleanType(params1.at(j).leftType(), tree)
                                != cleanType(params2.at(j).leftType(), tree))
                            break;
                    }
                    if (j == params1.count())
                        candidates << overload;
                }


                /*
                    There are several functions with the correct
                    parameter count, but only one has the correct
                    types, loosely compared.
                */
                if (candidates.count() == 1)
                    return static_cast<FunctionNode *>(candidates.first());

                return 0;
            }
        }
        delete clone;
    }
    return func;
}

/*!
  Returns the set of strings reopresenting the topic commands.
 */
QSet<QString> CppCodeParser::topicCommands()
{
    return QSet<QString>() << COMMAND_CLASS
                           << COMMAND_ENUM
                           << COMMAND_EXAMPLE
                           << COMMAND_EXTERNALPAGE
                           << COMMAND_FILE
                           << COMMAND_FN
                           << COMMAND_GROUP
                           << COMMAND_HEADERFILE
                           << COMMAND_MACRO
                           << COMMAND_MODULE
                           << COMMAND_NAMESPACE
                           << COMMAND_PAGE
                           << COMMAND_PROPERTY
                           << COMMAND_SERVICE
                           << COMMAND_TYPEDEF
#ifdef QDOC_QML
                           << COMMAND_VARIABLE
                           << COMMAND_QMLCLASS
                           << COMMAND_QMLPROPERTY
                           << COMMAND_QMLATTACHEDPROPERTY
                           << COMMAND_QMLSIGNAL
                           << COMMAND_QMLATTACHEDSIGNAL
                           << COMMAND_QMLMETHOD
                           << COMMAND_QMLATTACHEDMETHOD
                           << COMMAND_QMLBASICTYPE;
#else
                           << COMMAND_VARIABLE;
#endif
}

/*!
  Process the topic \a command in context \a doc with argument \a arg.
 */
Node *CppCodeParser::processTopicCommand(const Doc& doc,
                                         const QString& command,
                                         const QString& arg)
{
    if (command == COMMAND_FN) {
        QStringList parentPath;
        FunctionNode *func = 0;
        FunctionNode *clone = 0;

        if (!makeFunctionNode(arg, &parentPath, &clone) &&
             !makeFunctionNode("void " + arg, &parentPath, &clone)) {
            doc.location().warning(tr("Invalid syntax in '\\%1'")
                                    .arg(COMMAND_FN));
        }
        else {
            if (!usedNamespaces.isEmpty()) {
                foreach (const QString &usedNamespace, usedNamespaces) {
                    QStringList newPath = usedNamespace.split("::") + parentPath;
                    func = tre->findFunctionNode(newPath, clone);
                    if (func)
                        break;
                }
            }
            // Search the root namespace if no match was found.
            if (func == 0)
                func = tre->findFunctionNode(parentPath, clone);

            if (func == 0) {
                if (parentPath.isEmpty() && !lastPath.isEmpty())
                    func = tre->findFunctionNode(lastPath, clone);
                if (func == 0) {
                    doc.location().warning(tr("Cannot find '%1' in '\\%2'")
                                           .arg(clone->name() + "(...)")
                                           .arg(COMMAND_FN),
                                           tr("I cannot find any function of that name with the "
                                              "specified signature. Make sure that the signature "
                                              "is identical to the declaration, including 'const' "
                                              "qualifiers."));
                }
                else {
                    doc.location().warning(tr("Missing '%1::' for '%2' in '\\%3'")
                                            .arg(lastPath.join("::"))
                                            .arg(clone->name() + "()")
                                            .arg(COMMAND_FN));
                }
            }
            else {
                lastPath = parentPath;
            }
            if (func) {
                func->borrowParameterNames(clone);
                func->setParentPath(clone->parentPath());
            }
            delete clone;
        }
        return func;
    }
    else if (command == COMMAND_MACRO) {
        QStringList parentPath;
        FunctionNode *func = 0;

        if (makeFunctionNode(arg, &parentPath, &func, tre->root())) {
            if (!parentPath.isEmpty()) {
                doc.location().warning(tr("Invalid syntax in '\\%1'")
                                        .arg(COMMAND_MACRO));
                delete func;
                func = 0;
            }
            else {
                func->setMetaness(FunctionNode::MacroWithParams);
                QList<Parameter> params = func->parameters();
                for (int i = 0; i < params.size(); ++i) {
                    Parameter &param = params[i];
                    if (param.name().isEmpty() && !param.leftType().isEmpty()
                            && param.leftType() != "...")
                        param = Parameter("", "", param.leftType());
                }
                func->setParameters(params);
            }
            return func;
        }
        else if (QRegExp("[A-Za-z_][A-Za-z0-9_]+").exactMatch(arg)) {
            func = new FunctionNode(tre->root(), arg);
            func->setAccess(Node::Public);
            func->setLocation(doc.location());
            func->setMetaness(FunctionNode::MacroWithoutParams);
        }
        else {
            doc.location().warning(tr("Invalid syntax in '\\%1'")
                                    .arg(COMMAND_MACRO));

        }
        return func;
    }
    else if (nodeTypeMap.contains(command)) {
        /*
          The command was neither "fn" nor "macro" .
         */
        // ### split(" ") hack is there to support header file syntax
        QStringList paths = arg.split(" ");
        QStringList path = paths[0].split("::");
        Node *node = 0;
        if (!usedNamespaces.isEmpty()) {
            foreach (const QString &usedNamespace, usedNamespaces) {
                QStringList newPath = usedNamespace.split("::") + path;
                node = tre->findNode(newPath, nodeTypeMap[command]);
                if (node) {
                    path = newPath;
                    break;
                }
            }
        }
        // Search the root namespace if no match was found.
        if (node == 0)
            node = tre->findNode(path, nodeTypeMap[command]);

        if (node == 0) {
            doc.location().warning(tr("Cannot find '%1' specified with '\\%2' in any header file")
                                   .arg(arg).arg(command));
            lastPath = path;

        }
        else if (command == COMMAND_SERVICE) {
            // If the command is "\service", then we need to tag the
            // class with the actual service name.
            QStringList args = arg.split(" ");
            if (args.size() > 1) {
                ClassNode *cnode = static_cast<ClassNode *>(node);
                cnode->setServiceName(args[1]);
                cnode->setHideFromMainList(true);
            }
        }
        else if (node->isInnerNode()) {
            if (path.size() > 1) {
                path.pop_back();
                usedNamespaces.insert(path.join("::"));
            }
        }
        return node;
    }
    else if (command == COMMAND_EXAMPLE) {
        ExampleNode* en = new ExampleNode(tre->root(), arg);
        createExampleFileNodes(en);
        return en;
    }
    else if (command == COMMAND_EXTERNALPAGE) {
        return new FakeNode(tre->root(), arg, Node::ExternalPage);
    }
    else if (command == COMMAND_FILE) {
        return new FakeNode(tre->root(), arg, Node::File);
    }
    else if (command == COMMAND_GROUP) {
        return new FakeNode(tre->root(), arg, Node::Group);
    }
    else if (command == COMMAND_HEADERFILE) {
        return new FakeNode(tre->root(), arg, Node::HeaderFile);
    }
    else if (command == COMMAND_MODULE) {
        return new FakeNode(tre->root(), arg, Node::Module);
    }
    else if (command == COMMAND_PAGE) {
        return new FakeNode(tre->root(), arg, Node::Page);
    }
#ifdef QDOC_QML
    else if (command == COMMAND_QMLCLASS) {
        const ClassNode* classNode = 0;
        QStringList names = arg.split(" ");
        if (names.size() > 1) {
            Node* n = tre->findNode(names[1].split("::"),Node::Class);
            if (n)
                classNode = static_cast<const ClassNode*>(n);
        }
        if (names[0].startsWith("Qt"))
            return new QmlClassNode(tre->root(), QLatin1String("QML:")+names[0], classNode);
        else
            return new QmlClassNode(tre->root(), names[0], classNode);
    }
    else if (command == COMMAND_QMLBASICTYPE) {
        return new QmlBasicTypeNode(tre->root(), arg);
    }
    else if ((command == COMMAND_QMLSIGNAL) ||
             (command == COMMAND_QMLMETHOD) ||
             (command == COMMAND_QMLATTACHEDSIGNAL) ||
             (command == COMMAND_QMLATTACHEDMETHOD)) {
        QString element;
        QString type;
        QmlClassNode* qmlClass = 0;
        if (splitQmlMethodArg(doc,arg,type,element)) {
            if (element.startsWith(QLatin1String("Qt")))
                element = QLatin1String("QML:") + element;
            Node* n = tre->findNode(QStringList(element),Node::Fake);
            if (n && n->subType() == Node::QmlClass) {
                qmlClass = static_cast<QmlClassNode*>(n);
                if (command == COMMAND_QMLSIGNAL)
                    return makeFunctionNode(doc,arg,qmlClass,Node::QmlSignal,false,COMMAND_QMLSIGNAL);
                else if (command == COMMAND_QMLATTACHEDSIGNAL)
                    return makeFunctionNode(doc,arg,qmlClass,Node::QmlSignal,true,COMMAND_QMLATTACHEDSIGNAL);
                else if (command == COMMAND_QMLMETHOD)
                    return makeFunctionNode(doc,arg,qmlClass,Node::QmlMethod,false,COMMAND_QMLMETHOD);
                else if (command == COMMAND_QMLATTACHEDMETHOD)
                    return makeFunctionNode(doc,arg,qmlClass,Node::QmlMethod,true,COMMAND_QMLATTACHEDMETHOD);
                else
                    return 0; // never get here.
            }
        }
    }
#endif
    return 0;
}

#ifdef QDOC_QML

/*!
  A QML property argument has the form...

  <type> <element>::<name>

  This function splits the argument into those three
  parts, sets \a type, \a element, and \a name,
  and returns true. If any of the parts isn't found,
  a qdoc warning is output and false is returned.
 */
bool CppCodeParser::splitQmlPropertyArg(const Doc& doc,
                                        const QString& arg,
                                        QString& type,
                                        QString& element,
                                        QString& name)
{
    QStringList blankSplit = arg.split(" ");
    if (blankSplit.size() > 1) {
        type = blankSplit[0];
        QStringList colonSplit(blankSplit[1].split("::"));
        if (colonSplit.size() > 1) {
            element = colonSplit[0];
            name = colonSplit[1];
            return true;
        }
        else
            doc.location().warning(tr("Missing parent QML element name"));
    }
    else
        doc.location().warning(tr("Missing property type"));
    return false;
}

/*!
  A QML signal or method argument has the form...

  <type> <element>::<name>(<param>, <param>, ...)

  This function splits the argument into those two
  parts, sets \a element, and \a name, and returns
  true. If either of the parts isn't found, a debug
  message is output and false is returned.
 */
bool CppCodeParser::splitQmlMethodArg(const Doc& doc,
                                      const QString& arg,
                                      QString& type,
                                      QString& element)
{
    QStringList colonSplit(arg.split("::"));
    if (colonSplit.size() > 1) {
        QStringList blankSplit = colonSplit[0].split(" ");
        if (blankSplit.size() > 1) {
            type = blankSplit[0];
            element = blankSplit[1];
        }
        else {
            type = QString("");
            element = colonSplit[0];
        }
        return true;
    }
    else
        doc.location().warning(tr("Missing parent QML element or method signature"));
    return false;
}

/*!
  Process the topic \a command group with arguments \a args.

  Currently, this function is called only for \e{qmlproperty}
  and \e{qmlattachedproperty}.
 */
Node *CppCodeParser::processTopicCommandGroup(const Doc& doc,
                                              const QString& command,
                                              const QStringList& args)
{
    QmlPropGroupNode* qmlPropGroup = 0;
    if ((command == COMMAND_QMLPROPERTY) ||
        (command == COMMAND_QMLATTACHEDPROPERTY)) {
        QString type;
        QString element;
        QString property;
        bool attached = (command == COMMAND_QMLATTACHEDPROPERTY);
        QStringList::ConstIterator arg = args.begin();
        if (splitQmlPropertyArg(doc,(*arg),type,element,property)) {
            Node* n = tre->findNode(QStringList(element),Node::Fake);
            if (n && n->subType() == Node::QmlClass) {
                QmlClassNode* qmlClass = static_cast<QmlClassNode*>(n);
                if (qmlClass)
                    qmlPropGroup = new QmlPropGroupNode(qmlClass,
                                                        property,
                                                        attached);
            }
        }
        if (qmlPropGroup) {
            const ClassNode *correspondingClass = static_cast<const QmlClassNode*>(qmlPropGroup->parent())->classNode();
            QmlPropertyNode *qmlPropNode = new QmlPropertyNode(qmlPropGroup,property,type,attached);

            const PropertyNode *correspondingProperty = 0;
            if (correspondingClass) {
                correspondingProperty = qmlPropNode->correspondingProperty(tre);
            }
            if (correspondingProperty) {
                bool writableList = type.startsWith("list") && correspondingProperty->dataType().endsWith('*');
                qmlPropNode->setWritable(writableList || correspondingProperty->isWritable());
            }
            ++arg;
            while (arg != args.end()) {
                if (splitQmlPropertyArg(doc,(*arg),type,element,property)) {
                    QmlPropertyNode* qmlPropNode = new QmlPropertyNode(qmlPropGroup,
                                                                       property,
                                                                       type,
                                                                       attached);
                    if (correspondingProperty) {
                        bool writableList = type.startsWith("list") && correspondingProperty->dataType().endsWith('*');
                        qmlPropNode->setWritable(writableList || correspondingProperty->isWritable());
                    }
                }
                ++arg;
            }
        }
    }
    return qmlPropGroup;
}
#endif

/*!
  Returns the set of strings representing the common metacommands
  plus some other metacommands.
 */
QSet<QString> CppCodeParser::otherMetaCommands()
{
    return commonMetaCommands() << COMMAND_INHEADERFILE
                                << COMMAND_OVERLOAD
                                << COMMAND_REIMP
                                << COMMAND_RELATES
                                << COMMAND_CONTENTSPAGE
                                << COMMAND_NEXTPAGE
                                << COMMAND_PREVIOUSPAGE
                                << COMMAND_INDEXPAGE
#ifdef QDOC_QML
                                << COMMAND_STARTPAGE
                                << COMMAND_QMLINHERITS
                                << COMMAND_QMLDEFAULT;
#else
                                << COMMAND_STARTPAGE;
#endif
}

/*!
  Process the metacommand \a command in the context of the
  \a node associated with the topic command and the \a doc.
  \a arg is the argument to the metacommand.
 */
void CppCodeParser::processOtherMetaCommand(const Doc& doc,
                                            const QString& command,
                                            const QString& arg,
                                            Node *node)
{
    if (command == COMMAND_INHEADERFILE) {
        if (node != 0 && node->isInnerNode()) {
            ((InnerNode *) node)->addInclude(arg);
        }
        else {
            doc.location().warning(tr("Ignored '\\%1'")
                                   .arg(COMMAND_INHEADERFILE));
        }
    }
    else if (command == COMMAND_OVERLOAD) {
        if (node != 0 && node->type() == Node::Function) {
            ((FunctionNode *) node)->setOverload(true);
        }
        else {
            doc.location().warning(tr("Ignored '\\%1'")
                                   .arg(COMMAND_OVERLOAD));
        }
    }
    else if (command == COMMAND_REIMP) {
        if (node != 0 && node->type() == Node::Function) {
            FunctionNode *func = (FunctionNode *) node;
            const FunctionNode *from = func->reimplementedFrom();
            if (from == 0) {
                doc.location().warning(
                    tr("Cannot find base function for '\\%1' in %2()")
                    .arg(COMMAND_REIMP).arg(node->name()),
                    tr("The function either doesn't exist in any base class "
                       "with the same signature or it exists but isn't virtual."));
            }
            /*
              Ideally, we would enable this check to warn whenever
              \reimp is used incorrectly, and only make the node
              internal if the function is a reimplementation of
              another function in a base class.
            */
            else if (from->access() == Node::Private
                     || from->parent()->access() == Node::Private) {
                doc.location().warning(tr("'\\%1' in %2() should be '\\internal' because its base function is private or internal")
                    .arg(COMMAND_REIMP).arg(node->name()));
            }

            func->setReimp(true);
        }
        else {
            doc.location().warning(tr("Ignored '\\%1' in %2")
                                   .arg(COMMAND_REIMP)
                                   .arg(node->name()));
        }
    }
    else if (command == COMMAND_RELATES) {
        InnerNode *pseudoParent;
        if (arg.startsWith("<") || arg.startsWith("\"")) {
            pseudoParent =
                static_cast<InnerNode *>(tre->findNode(QStringList(arg),
                                                       Node::Fake));
        }
        else {
            QStringList newPath = arg.split("::");
            pseudoParent =
                static_cast<InnerNode*>(tre->findNode(QStringList(newPath),
                                                      Node::Class));
            if (!pseudoParent)
                pseudoParent =
                    static_cast<InnerNode*>(tre->findNode(QStringList(newPath),
                                                          Node::Namespace));
        }
        if (!pseudoParent) {
            doc.location().warning(tr("Cannot find '%1' in '\\%2'")
                                   .arg(arg).arg(COMMAND_RELATES));
        }
        else {
            node->setRelates(pseudoParent);
        }
    }
    else if (command == COMMAND_CONTENTSPAGE) {
        setLink(node, Node::ContentsLink, arg);
    }
    else if (command == COMMAND_NEXTPAGE) {
        setLink(node, Node::NextLink, arg);
    }
    else if (command == COMMAND_PREVIOUSPAGE) {
        setLink(node, Node::PreviousLink, arg);
    }
    else if (command == COMMAND_INDEXPAGE) {
        setLink(node, Node::IndexLink, arg);
    }
    else if (command == COMMAND_STARTPAGE) {
        setLink(node, Node::StartLink, arg);
    }
#ifdef QDOC_QML
    else if (command == COMMAND_QMLINHERITS) {
        setLink(node, Node::InheritsLink, arg);
        if (node->subType() == Node::QmlClass) {
            QmlClassNode::addInheritedBy(arg,node);
        }
   }
    else if (command == COMMAND_QMLDEFAULT) {
        QmlPropGroupNode* qpgn = static_cast<QmlPropGroupNode*>(node);
        qpgn->setDefault();
    }
#endif
    else {
        processCommonMetaCommand(doc.location(),command,arg,node,tre);
    }
}

/*!
  The topic command has been processed resulting in the \a doc
  and \a node passed in here. Process the other meta commands,
  which are found in \a doc, in the context of the topic \a node.
 */
void CppCodeParser::processOtherMetaCommands(const Doc& doc, Node *node)
{
    const QSet<QString> metaCommands = doc.metaCommandsUsed();
    QSet<QString>::ConstIterator cmd = metaCommands.begin();
    while (cmd != metaCommands.end()) {
        QStringList args = doc.metaCommandArgs(*cmd);
        QStringList::ConstIterator arg = args.begin();
        while (arg != args.end()) {
            processOtherMetaCommand(doc, *cmd, *arg, node);
            ++arg;
        }
        ++cmd;
    }
}

/*!
  Resets the C++ code parser to its default initialized state.
 */
void CppCodeParser::reset(Tree *tree)
{
    tre = tree;
    tokenizer = 0;
    tok = 0;
    access = Node::Public;
    metaness = FunctionNode::Plain;
    lastPath.clear();
    moduleName = "";
}

/*!
  Get the next token from the file being parsed and store it
  in the token variable.
 */
void CppCodeParser::readToken()
{
    tok = tokenizer->getToken();
}

/*!
  Return the current location in the file being parsed,
  i.e. the file name, line number, and column number.
 */
const Location& CppCodeParser::location()
{
    return tokenizer->location();
}

/*!
  Return the previous string read from the file being parsed.
 */
QString CppCodeParser::previousLexeme()
{
    return tokenizer->previousLexeme();
}

/*!
  Return the current string string from the file being parsed.
 */
QString CppCodeParser::lexeme()
{
    return tokenizer->lexeme();
}

bool CppCodeParser::match(int target)
{
    if (tok == target) {
        readToken();
        return true;
    }
    else
        return false;
}

/*!
  Skip to \a target. If \a target is found before the end
  of input, return true. Otherwise return false.
 */
bool CppCodeParser::skipTo(int target)
{
    while ((tok != Tok_Eoi) && (tok != target))
        readToken();
    return (tok == target ? true : false);
}

/*!
  If the current token is one of the keyword thingees that
  are used in Qt, skip over it to the next token and return
  true. Otherwise just return false without reading the
  next token.
 */
bool CppCodeParser::matchCompat()
{
    switch (tok) {
    case Tok_QT_COMPAT:
    case Tok_QT_COMPAT_CONSTRUCTOR:
    case Tok_QT_DEPRECATED:
    case Tok_QT_MOC_COMPAT:
    case Tok_QT3_SUPPORT:
    case Tok_QT3_SUPPORT_CONSTRUCTOR:
    case Tok_QT3_MOC_SUPPORT:
        readToken();
        return true;
    default:
        return false;
    }
}

bool CppCodeParser::matchTemplateAngles(CodeChunk *dataType)
{
    bool matches = (tok == Tok_LeftAngle);
    if (matches) {
        int leftAngleDepth = 0;
        int parenAndBraceDepth = 0;
        do {
            if (tok == Tok_LeftAngle) {
                leftAngleDepth++;
            }
            else if (tok == Tok_RightAngle) {
                leftAngleDepth--;
            }
            else if (tok == Tok_LeftParen || tok == Tok_LeftBrace) {
                ++parenAndBraceDepth;
            }
            else if (tok == Tok_RightParen || tok == Tok_RightBrace) {
                if (--parenAndBraceDepth < 0)
                    return false;
            }

            if (dataType != 0)
                dataType->append(lexeme());
            readToken();
        } while (leftAngleDepth > 0 && tok != Tok_Eoi);
    }
    return matches;
}

bool CppCodeParser::matchTemplateHeader()
{
    readToken();
    return matchTemplateAngles();
}

bool CppCodeParser::matchDataType(CodeChunk *dataType, QString *var)
{
    /*
      This code is really hard to follow... sorry. The loop is there to match
      Alpha::Beta::Gamma::...::Omega.
    */
    for (;;) {
        bool virgin = true;

        if (tok != Tok_Ident) {
            /*
              There is special processing for 'Foo::operator int()'
              and such elsewhere. This is the only case where we
              return something with a trailing gulbrandsen ('Foo::').
            */
            if (tok == Tok_operator)
                return true;

            /*
              People may write 'const unsigned short' or
              'short unsigned const' or any other permutation.
            */
            while (match(Tok_const) || match(Tok_volatile))
                dataType->append(previousLexeme());
            while (match(Tok_signed) || match(Tok_unsigned) ||
                    match(Tok_short) || match(Tok_long) || match(Tok_int64)) {
                dataType->append(previousLexeme());
                virgin = false;
            }
            while (match(Tok_const) || match(Tok_volatile))
                dataType->append(previousLexeme());

            if (match(Tok_Tilde))
                dataType->append(previousLexeme());
        }

        if (virgin) {
            if (match(Tok_Ident))
                dataType->append(previousLexeme());
            else if (match(Tok_void) || match(Tok_int) || match(Tok_char) ||
                      match(Tok_double) || match(Tok_Ellipsis))
                dataType->append(previousLexeme());
            else
                return false;
        }
        else if (match(Tok_int) || match(Tok_char) || match(Tok_double)) {
            dataType->append(previousLexeme());
        }

        matchTemplateAngles(dataType);

        while (match(Tok_const) || match(Tok_volatile))
            dataType->append(previousLexeme());

        if (match(Tok_Gulbrandsen))
            dataType->append(previousLexeme());
        else
            break;
    }

    while (match(Tok_Ampersand) || match(Tok_Aster) || match(Tok_const) ||
            match(Tok_Caret))
        dataType->append(previousLexeme());

    if (match(Tok_LeftParenAster)) {
        /*
          A function pointer. This would be rather hard to handle without a
          tokenizer hack, because a type can be followed with a left parenthesis
          in some cases (e.g., 'operator int()'). The tokenizer recognizes '(*'
          as a single token.
        */
        dataType->append(previousLexeme());
        dataType->appendHotspot();
        if (var != 0 && match(Tok_Ident))
            *var = previousLexeme();
        if (!match(Tok_RightParen) || tok != Tok_LeftParen)
            return false;
        dataType->append(previousLexeme());

        int parenDepth0 = tokenizer->parenDepth();
        while (tokenizer->parenDepth() >= parenDepth0 && tok != Tok_Eoi) {
            dataType->append(lexeme());
            readToken();
        }
        if (match(Tok_RightParen))
            dataType->append(previousLexeme());
    }
    else {
        /*
          The common case: Look for an optional identifier, then for
          some array brackets.
        */
        dataType->appendHotspot();

        if (var != 0) {
            if (match(Tok_Ident)) {
                *var = previousLexeme();
            }
            else if (match(Tok_Comment)) {
                /*
                  A neat hack: Commented-out parameter names are
                  recognized by qdoc. It's impossible to illustrate
                  here inside a C-style comment, because it requires
                  an asterslash. It's also impossible to illustrate
                  inside a C++-style comment, because the explanation
                  does not fit on one line.
                */
                if (varComment.exactMatch(previousLexeme()))
                    *var = varComment.cap(1);
            }
        }

        if (tok == Tok_LeftBracket) {
            int bracketDepth0 = tokenizer->bracketDepth();
            while ((tokenizer->bracketDepth() >= bracketDepth0 &&
                     tok != Tok_Eoi) ||
                    tok == Tok_RightBracket) {
                dataType->append(lexeme());
                readToken();
            }
        }
    }
    return true;
}

bool CppCodeParser::matchParameter(FunctionNode *func)
{
    CodeChunk dataType;
    QString name;
    CodeChunk defaultValue;

    if (!matchDataType(&dataType, &name))
        return false;
    match(Tok_Comment);
    if (match(Tok_Equal)) {
        int parenDepth0 = tokenizer->parenDepth();

        while (tokenizer->parenDepth() >= parenDepth0 &&
                (tok != Tok_Comma ||
                 tokenizer->parenDepth() > parenDepth0) &&
                tok != Tok_Eoi) {
            defaultValue.append(lexeme());
            readToken();
        }
    }
    func->addParameter(Parameter(dataType.toString(),
                                 "",
                                 name,
                                 defaultValue.toString())); // ###
    return true;
}

bool CppCodeParser::matchFunctionDecl(InnerNode *parent,
                                      QStringList *parentPathPtr,
                                      FunctionNode **funcPtr,
                                      const QString &templateStuff,
                                      Node::Type type,
                                      bool attached)
{
    CodeChunk returnType;
    QStringList parentPath;
    QString name;

    bool compat = false;

    if (match(Tok_friend))
        return false;
    match(Tok_explicit);
    if (matchCompat())
        compat = true;
    bool sta = false;
    if (match(Tok_static)) {
        sta = true;
        if (matchCompat())
            compat = true;
    }
    FunctionNode::Virtualness vir = FunctionNode::NonVirtual;
    if (match(Tok_virtual)) {
        vir = FunctionNode::ImpureVirtual;
        if (matchCompat())
            compat = true;
    }

    if (!matchDataType(&returnType)) {
        if (tokenizer->parsingFnOrMacro()
                && (match(Tok_Q_DECLARE_FLAGS) ||
                    match(Tok_Q_PROPERTY) ||
                    match(Tok_Q_PRIVATE_PROPERTY)))
            returnType = CodeChunk(previousLexeme());
        else {
            return false;
        }
    }

    if (returnType.toString() == "QBool")
        returnType = CodeChunk("bool");

    if (matchCompat())
        compat = true;

    if (tok == Tok_operator &&
         (returnType.toString().isEmpty() ||
          returnType.toString().endsWith("::"))) {
        // 'QString::operator const char *()'
        parentPath = returnType.toString().split(sep);
        parentPath.removeAll(QString());
        returnType = CodeChunk();
        readToken();

        CodeChunk restOfName;
        if (tok != Tok_Tilde && matchDataType(&restOfName)) {
            name = "operator " + restOfName.toString();
        }
        else {
            name = previousLexeme() + lexeme();
            readToken();
            while (tok != Tok_LeftParen && tok != Tok_Eoi) {
                name += lexeme();
                readToken();
            }
        }
        if (tok != Tok_LeftParen) {
            return false;
        }
    }
    else if (tok == Tok_LeftParen) {
        // constructor or destructor
        parentPath = returnType.toString().split(sep);
        if (!parentPath.isEmpty()) {
            name = parentPath.last();
            parentPath.erase(parentPath.end() - 1);
        }
        returnType = CodeChunk();
    }
    else {
        while (match(Tok_Ident)) {
            name = previousLexeme();
            matchTemplateAngles();

            if (match(Tok_Gulbrandsen))
                parentPath.append(name);
            else
                break;
        }

        if (tok == Tok_operator) {
            name = lexeme();
            readToken();
            while (tok != Tok_Eoi) {
                name += lexeme();
                readToken();
                if (tok == Tok_LeftParen)
                    break;
            }
        }
        if (parent && (tok == Tok_Semicolon ||
                       tok == Tok_LeftBracket ||
                       tok == Tok_Colon)
                && access != Node::Private) {
            if (tok == Tok_LeftBracket) {
                returnType.appendHotspot();

                int bracketDepth0 = tokenizer->bracketDepth();
                while ((tokenizer->bracketDepth() >= bracketDepth0 &&
                         tok != Tok_Eoi) ||
                        tok == Tok_RightBracket) {
                    returnType.append(lexeme());
                    readToken();
                }
                if (tok != Tok_Semicolon) {
                    return false;
                }
            }
            else if (tok == Tok_Colon) {
                returnType.appendHotspot();

                while (tok != Tok_Semicolon && tok != Tok_Eoi) {
                    returnType.append(lexeme());
                    readToken();
                }
                if (tok != Tok_Semicolon) {
                    return false;
                }
            }

            VariableNode *var = new VariableNode(parent, name);
            var->setAccess(access);
            var->setLocation(location());
            var->setLeftType(returnType.left());
            var->setRightType(returnType.right());
            if (compat)
                var->setStatus(Node::Compat);
            var->setStatic(sta);
            return false;
        }
        if (tok != Tok_LeftParen) {
            return false;
        }
    }
    readToken();

    FunctionNode *func = new FunctionNode(type, parent, name, attached);
    func->setAccess(access);
    func->setLocation(location());
    func->setReturnType(returnType.toString());
    func->setParentPath(parentPath);
    func->setTemplateStuff(templateStuff);
    if (compat)
        func->setStatus(Node::Compat);

    func->setMetaness(metaness);
    if (parent) {
        if (name == parent->name()) {
            func->setMetaness(FunctionNode::Ctor);
        } else if (name.startsWith("~"))  {
            func->setMetaness(FunctionNode::Dtor);
        }
    }
    func->setStatic(sta);

    if (tok != Tok_RightParen) {
        do {
            if (!matchParameter(func)) {
                return false;
            }
        } while (match(Tok_Comma));
    }
    if (!match(Tok_RightParen)) {
        return false;
    }

    func->setConst(match(Tok_const));

    if (match(Tok_Equal) && match(Tok_Number))
        vir = FunctionNode::PureVirtual;
    func->setVirtualness(vir);

    if (match(Tok_Colon)) {
        while (tok != Tok_LeftBrace && tok != Tok_Eoi)
            readToken();
    }

    if (!match(Tok_Semicolon) && tok != Tok_Eoi) {
        int braceDepth0 = tokenizer->braceDepth();

        if (!match(Tok_LeftBrace)) {
            return false;
        }
        while (tokenizer->braceDepth() >= braceDepth0 && tok != Tok_Eoi)
            readToken();
        match(Tok_RightBrace);
    }
    if (parentPathPtr != 0)
        *parentPathPtr = parentPath;
    if (funcPtr != 0)
        *funcPtr = func;
    return true;
}

bool CppCodeParser::matchBaseSpecifier(ClassNode *classe, bool isClass)
{
    Node::Access access;

    switch (tok) {
    case Tok_public:
        access = Node::Public;
        readToken();
        break;
    case Tok_protected:
        access = Node::Protected;
        readToken();
        break;
    case Tok_private:
        access = Node::Private;
        readToken();
        break;
    default:
        access = isClass ? Node::Private : Node::Public;
    }

    if (tok == Tok_virtual)
        readToken();

    CodeChunk baseClass;
    if (!matchDataType(&baseClass))
        return false;

    tre->addBaseClass(classe,
                      access,
                      baseClass.toPath(),
                      baseClass.toString(),
                      classe->parent());
    return true;
}

bool CppCodeParser::matchBaseList(ClassNode *classe, bool isClass)
{
    for (;;) {
        if (!matchBaseSpecifier(classe, isClass))
            return false;
        if (tok == Tok_LeftBrace)
            return true;
        if (!match(Tok_Comma))
            return false;
    }
}

/*!
  Parse a C++ class, union, or struct declarion.
 */
bool CppCodeParser::matchClassDecl(InnerNode *parent,
                                   const QString &templateStuff)
{
    bool isClass = (tok == Tok_class);
    readToken();

    bool compat = matchCompat();

    if (tok != Tok_Ident)
        return false;
    while (tok == Tok_Ident)
        readToken();
    if (tok != Tok_Colon && tok != Tok_LeftBrace)
        return false;

    /*
      So far, so good. We have 'class Foo {' or 'class Foo :'.
      This is enough to recognize a class definition.
    */
    ClassNode *classe = new ClassNode(parent, previousLexeme());
    classe->setAccess(access);
    classe->setLocation(location());
    if (compat)
        classe->setStatus(Node::Compat);
    if (!moduleName.isEmpty())
        classe->setModuleName(moduleName);
    classe->setTemplateStuff(templateStuff);

    if (match(Tok_Colon) && !matchBaseList(classe, isClass))
        return false;
    if (!match(Tok_LeftBrace))
        return false;

    Node::Access outerAccess = access;
    access = isClass ? Node::Private : Node::Public;
    FunctionNode::Metaness outerMetaness = metaness;
    metaness = FunctionNode::Plain;

    bool matches = (matchDeclList(classe) && match(Tok_RightBrace) &&
                     match(Tok_Semicolon));
    access = outerAccess;
    metaness = outerMetaness;
    return matches;
}

bool CppCodeParser::matchNamespaceDecl(InnerNode *parent)
{
    readToken(); // skip 'namespace'
    if (tok != Tok_Ident)
        return false;
    while (tok == Tok_Ident)
        readToken();
    if (tok != Tok_LeftBrace)
        return false;

    /*
        So far, so good. We have 'namespace Foo {'.
    */
    QString namespaceName = previousLexeme();
    NamespaceNode *namespasse = 0;
    if (parent) {
        namespasse = static_cast<NamespaceNode*>(parent->findNode(namespaceName, Node::Namespace));
    }
    if (!namespasse) {
        namespasse = new NamespaceNode(parent, namespaceName);
        namespasse->setAccess(access);
        namespasse->setLocation(location());
    }

    readToken(); // skip '{'
    bool matched = matchDeclList(namespasse);

    return matched && match(Tok_RightBrace);
}

bool CppCodeParser::matchUsingDecl()
{
    readToken(); // skip 'using'

    // 'namespace'
    if (tok != Tok_namespace)
        return false;

    readToken();
    // identifier
    if (tok != Tok_Ident)
        return false;

    QString name;
    while (tok == Tok_Ident) {
        name += lexeme();
        readToken();
        if (tok == Tok_Semicolon)
            break;
        else if (tok != Tok_Gulbrandsen)
            return false;
        name += "::";
        readToken();
    }

    /*
        So far, so good. We have 'using namespace Foo;'.
    */
    usedNamespaces.insert(name);
    return true;
}

bool CppCodeParser::matchEnumItem(InnerNode *parent, EnumNode *enume)
{
    if (!match(Tok_Ident))
        return false;

    QString name = previousLexeme();
    CodeChunk val;

    if (match(Tok_Equal)) {
        while (tok != Tok_Comma && tok != Tok_RightBrace &&
                tok != Tok_Eoi) {
            val.append(lexeme());
            readToken();
        }
    }

    if (enume) {
        QString strVal = val.toString();
        if (strVal.isEmpty()) {
            if (enume->items().isEmpty()) {
                strVal = "0";
            }
            else {
                QString last = enume->items().last().value();
                bool ok;
                int n = last.toInt(&ok);
                if (ok) {
                    if (last.startsWith("0") && last.size() > 1) {
                        if (last.startsWith("0x") || last.startsWith("0X"))
                            strVal = last.left(2) + QString::number(n + 1, 16);
                        else
                            strVal = "0" + QString::number(n + 1, 8);
                    }
                    else
                        strVal = QString::number(n + 1);
                }
            }
        }

        enume->addItem(EnumItem(name, strVal));
    }
    else {
        VariableNode *var = new VariableNode(parent, name);
        var->setAccess(access);
        var->setLocation(location());
        var->setLeftType("const int");
        var->setStatic(true);
    }
    return true;
}

bool CppCodeParser::matchEnumDecl(InnerNode *parent)
{
    QString name;

    if (!match(Tok_enum))
        return false;
    if (match(Tok_Ident))
        name = previousLexeme();
    if (tok != Tok_LeftBrace)
        return false;

    EnumNode *enume = 0;

    if (!name.isEmpty()) {
        enume = new EnumNode(parent, name);
        enume->setAccess(access);
        enume->setLocation(location());
    }

    readToken();

    if (!matchEnumItem(parent, enume))
        return false;

    while (match(Tok_Comma)) {
        if (!matchEnumItem(parent, enume))
            return false;
    }
    return match(Tok_RightBrace) && match(Tok_Semicolon);
}

bool CppCodeParser::matchTypedefDecl(InnerNode *parent)
{
    CodeChunk dataType;
    QString name;

    if (!match(Tok_typedef))
        return false;
    if (!matchDataType(&dataType, &name))
        return false;
    if (!match(Tok_Semicolon))
        return false;

    if (parent && !parent->findNode(name, Node::Typedef)) {
        TypedefNode *typedeffe = new TypedefNode(parent, name);
        typedeffe->setAccess(access);
        typedeffe->setLocation(location());
    }
    return true;
}

bool CppCodeParser::matchProperty(InnerNode *parent)
{
    int expected_tok = Tok_LeftParen;
    if (match(Tok_Q_PRIVATE_PROPERTY)) {
        expected_tok = Tok_Comma;
        if (!skipTo(Tok_Comma))
            return false;
    }
    else if (!match(Tok_Q_PROPERTY) &&
             !match(Tok_Q_OVERRIDE) &&
             !match(Tok_QDOC_PROPERTY)) {
        return false;
    }

    if (!match(expected_tok))
        return false;

    QString name;
    CodeChunk dataType;
    if (!matchDataType(&dataType, &name))
        return false;

    PropertyNode *property = new PropertyNode(parent, name);
    property->setAccess(Node::Public);
    property->setLocation(location());
    property->setDataType(dataType.toString());

    while (tok != Tok_RightParen && tok != Tok_Eoi) {
        if (!match(Tok_Ident))
            return false;
        QString key = previousLexeme();
        QString value;

        if (match(Tok_Ident) || match(Tok_Number)) {
            value = previousLexeme();
        }
        else if (match(Tok_LeftParen)) {
            int depth = 1;
            while (tok != Tok_Eoi) {
                if (tok == Tok_LeftParen) {
                    readToken();
                    ++depth;
                } else if (tok == Tok_RightParen) {
                    readToken();
                    if (--depth == 0)
                        break;
                } else {
                    readToken();
                }
            }
            value = "?";
        }

        if (key == "READ")
            tre->addPropertyFunction(property, value, PropertyNode::Getter);
        else if (key == "WRITE") {
            tre->addPropertyFunction(property, value, PropertyNode::Setter);
            property->setWritable(true);
        }
        else if (key == "STORED")
            property->setStored(value.toLower() == "true");
        else if (key == "DESIGNABLE") {
            QString v = value.toLower();
            if (v == "true")
                property->setDesignable(true);
            else if (v == "false")
                property->setDesignable(false);
            else {
                property->setDesignable(false);
                property->setRuntimeDesFunc(value);
            }
        }
        else if (key == "RESET")
            tre->addPropertyFunction(property, value, PropertyNode::Resetter);
        else if (key == "NOTIFY") {
            tre->addPropertyFunction(property, value, PropertyNode::Notifier);
        } else if (key == "REVISION") {
            int revision;
            bool ok;
            revision = value.toInt(&ok);
            if (ok)
                property->setRevision(revision);
            else
                parent->doc().location().warning(tr("Invalid revision number: %1").arg(value));
        } else if (key == "SCRIPTABLE") {
            QString v = value.toLower();
            if (v == "true")
                property->setScriptable(true);
            else if (v == "false")
                property->setScriptable(false);
            else {
                property->setScriptable(false);
                property->setRuntimeScrFunc(value);
            }
        }
        else if (key == "CONSTANT")
            property->setConstant();
        else if (key == "FINAL")
            property->setFinal();
    }
    match(Tok_RightParen);
    return true;
}

/*!
  Parse a C++ declaration.
 */
bool CppCodeParser::matchDeclList(InnerNode *parent)
{
    QString templateStuff;
    int braceDepth0 = tokenizer->braceDepth();
    if (tok == Tok_RightBrace) // prevents failure on empty body
        braceDepth0++;

    while (tokenizer->braceDepth() >= braceDepth0 && tok != Tok_Eoi) {
        switch (tok) {
        case Tok_Colon:
            readToken();
            break;
        case Tok_class:
        case Tok_struct:
        case Tok_union:
            matchClassDecl(parent, templateStuff);
            break;
        case Tok_namespace:
            matchNamespaceDecl(parent);
            break;
        case Tok_using:
            matchUsingDecl();
            break;
        case Tok_template:
            templateStuff = matchTemplateHeader();
            continue;
        case Tok_enum:
            matchEnumDecl(parent);
            break;
        case Tok_typedef:
            matchTypedefDecl(parent);
            break;
        case Tok_private:
            readToken();
            access = Node::Private;
            metaness = FunctionNode::Plain;
            break;
        case Tok_protected:
            readToken();
            access = Node::Protected;
            metaness = FunctionNode::Plain;
            break;
        case Tok_public:
            readToken();
            access = Node::Public;
            metaness = FunctionNode::Plain;
            break;
        case Tok_signals:
        case Tok_Q_SIGNALS:
            readToken();
            access = Node::Public;
            metaness = FunctionNode::Signal;
            break;
        case Tok_slots:
        case Tok_Q_SLOTS:
            readToken();
            metaness = FunctionNode::Slot;
            break;
        case Tok_Q_OBJECT:
            readToken();
            break;
        case Tok_Q_OVERRIDE:
        case Tok_Q_PROPERTY:
        case Tok_Q_PRIVATE_PROPERTY:
        case Tok_QDOC_PROPERTY:
            matchProperty(parent);
            break;
        case Tok_Q_DECLARE_SEQUENTIAL_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                sequentialIteratorClasses.insert(previousLexeme(),
                                                 location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_MUTABLE_SEQUENTIAL_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                mutableSequentialIteratorClasses.insert(previousLexeme(),
                                                        location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_ASSOCIATIVE_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                associativeIteratorClasses.insert(previousLexeme(),
                                                  location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_MUTABLE_ASSOCIATIVE_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                mutableAssociativeIteratorClasses.insert(previousLexeme(),
                                                         location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_FLAGS:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident)) {
                QString flagsType = previousLexeme();
                if (match(Tok_Comma) && match(Tok_Ident)) {
                    QString enumType = previousLexeme();
                    TypedefNode *flagsNode = new TypedefNode(parent, flagsType);
                    flagsNode->setAccess(access);
                    flagsNode->setLocation(location());
                    EnumNode *enumNode =
                        static_cast<EnumNode*>(parent->findNode(enumType,
                                                                Node::Enum));
                    if (enumNode)
                        enumNode->setFlagsType(flagsNode);
                }
            }
            match(Tok_RightParen);
            break;
        case Tok_QT_MODULE:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                moduleName = previousLexeme();
            if (!moduleName.startsWith("Qt"))
                moduleName.prepend("Qt");
            match(Tok_RightParen);
            break;
        default:
            if (!matchFunctionDecl(parent, 0, 0, templateStuff)) {
                while (tok != Tok_Eoi &&
                       (tokenizer->braceDepth() > braceDepth0 ||
                        (!match(Tok_Semicolon) &&
                         tok != Tok_public && tok != Tok_protected &&
                         tok != Tok_private)))
                    readToken();
            }
        }
        templateStuff.clear();
    }
    return true;
}

/*!
  This is called by parseSourceFile() to do the actual parsing
  and tree building.
 */
bool CppCodeParser::matchDocsAndStuff()
{
    QSet<QString> topicCommandsAllowed = topicCommands();
    QSet<QString> otherMetacommandsAllowed = otherMetaCommands();
    QSet<QString> metacommandsAllowed = topicCommandsAllowed +
        otherMetacommandsAllowed;

    while (tok != Tok_Eoi) {
        if (tok == Tok_Doc) {
            /*
              lexeme() returns an entire qdoc comment.
             */
            QString comment = lexeme();
            Location start_loc(location());
            readToken();

            Doc::trimCStyleComment(start_loc,comment);
            Location end_loc(location());

            /*
              Doc parses the comment.
             */
            Doc doc(start_loc,end_loc,comment,metacommandsAllowed);

            QString topic;
            QStringList args;

            QSet<QString> topicCommandsUsed = topicCommandsAllowed &
                doc.metaCommandsUsed();

            /*
              There should be one topic command in the set,
              or none. If the set is empty, then the comment
              should be a function description.
             */
            if (topicCommandsUsed.count() > 0) {
                topic = *topicCommandsUsed.begin();
                args = doc.metaCommandArgs(topic);
            }

            NodeList nodes;
            QList<Doc> docs;

            if (topic.isEmpty()) {
                QStringList parentPath;
                FunctionNode *clone;
                FunctionNode *func = 0;

                if (matchFunctionDecl(0, &parentPath, &clone)) {
                    foreach (const QString &usedNamespace, usedNamespaces) {
                        QStringList newPath = usedNamespace.split("::") + parentPath;
                        func = tre->findFunctionNode(newPath, clone);
                        if (func)
                            break;
                    }
                    if (func == 0)
                        func = tre->findFunctionNode(parentPath, clone);

                    if (func) {
                        func->borrowParameterNames(clone);
                        nodes.append(func);
                        docs.append(doc);
                    }
                    delete clone;
                }
                else {
                    doc.location().warning(
                       tr("Cannot tie this documentation to anything"),
                       tr("I found a /*! ... */ comment, but there was no "
                          "topic command (e.g., '\\%1', '\\%2') in the "
                          "comment and no function definition following "
                          "the comment.")
                       .arg(COMMAND_FN).arg(COMMAND_PAGE));
                }
            }
            else {
                /*
                  There is a topic command. Process it.
                 */
#ifdef QDOC_QML
                if ((topic == COMMAND_QMLPROPERTY) ||
                    (topic == COMMAND_QMLATTACHEDPROPERTY)) {
                    Doc nodeDoc = doc;
                    Node *node = processTopicCommandGroup(nodeDoc,topic,args);
                    if (node != 0) {
                        nodes.append(node);
                        docs.append(nodeDoc);
                    }
                }
                else {
                    QStringList::ConstIterator a = args.begin();
                    while (a != args.end()) {
                        Doc nodeDoc = doc;
                        Node *node = processTopicCommand(nodeDoc,topic,*a);
                        if (node != 0) {
                            nodes.append(node);
                            docs.append(nodeDoc);
                        }
                        ++a;
                    }
                }
#else
                QStringList::ConstIterator a = args.begin();
                while (a != args.end()) {
                    Doc nodeDoc = doc;
                    Node *node = processTopicCommand(nodeDoc, topic, *a);
                    if (node != 0) {
                        nodes.append(node);
                        docs.append(nodeDoc);
                    }
                    ++a;
                }
#endif
            }

            NodeList::Iterator n = nodes.begin();
            QList<Doc>::Iterator d = docs.begin();
            while (n != nodes.end()) {
                processOtherMetaCommands(*d, *n);
                (*n)->setDoc(*d);
                if ((*n)->isInnerNode() &&
                    ((InnerNode *)*n)->includes().isEmpty()) {
                    InnerNode *m = static_cast<InnerNode *>(*n);
                    while (m->parent() != tre->root())
                        m = m->parent();
                    if (m == *n)
                        ((InnerNode *)*n)->addInclude((*n)->name());
                    else
                        ((InnerNode *)*n)->setIncludes(m->includes());
                }
                ++d;
                ++n;
            }
        }
        else if (tok == Tok_using) {
            matchUsingDecl();
        }
        else {
            QStringList parentPath;
            FunctionNode *clone;
            FunctionNode *node = 0;

            if (matchFunctionDecl(0, &parentPath, &clone)) {
                /*
                  The location of the definition is more interesting
                  than that of the declaration. People equipped with
                  a sophisticated text editor can respond to warnings
                  concerning undocumented functions very quickly.

                  Signals are implemented in uninteresting files
                  generated by moc.
                */
                node = tre->findFunctionNode(parentPath, clone);
                if (node != 0 && node->metaness() != FunctionNode::Signal)
                    node->setLocation(clone->location());
                delete clone;
            }
            else {
                if (tok != Tok_Doc)
                    readToken();
            }
        }
    }
    return true;
}

bool CppCodeParser::makeFunctionNode(const QString& synopsis,
                                     QStringList *parentPathPtr,
                                     FunctionNode **funcPtr,
                                     InnerNode *root,
                                     Node::Type type,
                                     bool attached)
{
    Tokenizer *outerTokenizer = tokenizer;
    int outerTok = tok;

    Location loc;
    QByteArray latin1 = synopsis.toLatin1();
    Tokenizer stringTokenizer(loc, latin1);
    stringTokenizer.setParsingFnOrMacro(true);
    tokenizer = &stringTokenizer;
    readToken();

    bool ok = matchFunctionDecl(root, parentPathPtr, funcPtr, QString(), type, attached);
    // potential memory leak with funcPtr

    tokenizer = outerTokenizer;
    tok = outerTok;
    return ok;
}

/*!
  Create a new FunctionNode for a QML method or signal, as
  specified by \a type, as a child of \a parent. \a sig is
  the complete signature, and if \a attached is true, the
  method or signal is "attached". \a qdoctag is the text of
  the \a type.
 */
FunctionNode* CppCodeParser::makeFunctionNode(const Doc& doc,
                                              const QString& sig,
                                              InnerNode* parent,
                                              Node::Type type,
                                              bool attached,
                                              QString qdoctag)
{
    QStringList pp;
    FunctionNode* fn = 0;
    if (!makeFunctionNode(sig,&pp,&fn,parent,type,attached) &&
        !makeFunctionNode("void "+sig,&pp,&fn,parent,type,attached)) {
        doc.location().warning(tr("Invalid syntax in '\\%1'").arg(qdoctag));
    }
    if (fn)
        return fn;
    return 0;
}

void CppCodeParser::parseQiteratorDotH(const Location &location,
                                       const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return;

    QString text = file.readAll();
    text.remove("\r");
    text.replace("\\\n", "");
    QStringList lines = text.split("\n");
    lines = lines.filter("Q_DECLARE");
    lines.replaceInStrings(QRegExp("#define Q[A-Z_]*\\(C\\)"), "");

    if (lines.size() == 4) {
        sequentialIteratorDefinition = lines[0];
        mutableSequentialIteratorDefinition = lines[1];
        associativeIteratorDefinition = lines[2];
        mutableAssociativeIteratorDefinition = lines[3];
    }
    else {
        location.warning(tr("The qiterator.h hack failed"));
    }
}

void CppCodeParser::instantiateIteratorMacro(const QString &container,
                                             const QString &includeFile,
                                             const QString &macroDef,
                                             Tree * /* tree */)
{
    QString resultingCode = macroDef;
    resultingCode.replace(QRegExp("\\bC\\b"), container);
    resultingCode.replace(QRegExp("\\s*##\\s*"), "");

    Location loc(includeFile);   // hack to get the include file for free
    QByteArray latin1 = resultingCode.toLatin1();
    Tokenizer stringTokenizer(loc, latin1);
    tokenizer = &stringTokenizer;
    readToken();
    matchDeclList(tre->root());
}

void CppCodeParser::createExampleFileNodes(FakeNode *fake)
{
    QString examplePath = fake->name();
    QString proFileName = examplePath + "/" + examplePath.split("/").last() + ".pro";
    QString userFriendlyFilePath;

    QString fullPath = Config::findFile(fake->doc().location(),
                                        exampleFiles,
                                        exampleDirs,
                                        proFileName,
                                        userFriendlyFilePath);

    if (fullPath.isEmpty()) {
        QString tmp = proFileName;
        proFileName = examplePath + "/" + "qbuild.pro";
        userFriendlyFilePath.clear();
        fullPath = Config::findFile(fake->doc().location(),
                                    exampleFiles,
                                    exampleDirs,
                                    proFileName,
                                    userFriendlyFilePath);
        if (fullPath.isEmpty()) {
            proFileName = examplePath + "/" + examplePath.split("/").last() + ".qmlproject";
            userFriendlyFilePath.clear();
            fullPath = Config::findFile(fake->doc().location(),
                                        exampleFiles,
                                        exampleDirs,
                                        proFileName,
                                        userFriendlyFilePath);
            if (fullPath.isEmpty()) {
                fake->doc().location().warning(tr("Cannot find file '%1' or '%2'").arg(tmp).arg(proFileName));
                fake->doc().location().warning(tr("EXAMPLE PATH DOES NOT EXIST: %1").arg(examplePath));
                return;
            }
        }
    }

    int sizeOfBoringPartOfName = fullPath.size() - proFileName.size();
    fullPath.truncate(fullPath.lastIndexOf('/'));

    QStringList exampleFiles = Config::getFilesHere(fullPath,exampleNameFilter);
    QString imagesPath = fullPath + "/images";
    QStringList imageFiles = Config::getFilesHere(imagesPath,exampleImageFilter);
    if (!exampleFiles.isEmpty()) {
        // move main.cpp and to the end, if it exists
        QString mainCpp;
        QMutableStringListIterator i(exampleFiles);
        i.toBack();
        while (i.hasPrevious()) {
            QString fileName = i.previous();
            if (fileName.endsWith("/main.cpp")) {
                mainCpp = fileName;
                i.remove();
            }
            else if (fileName.contains("/qrc_") || fileName.contains("/moc_")
                || fileName.contains("/ui_"))
                i.remove();
        }
        if (!mainCpp.isEmpty())
            exampleFiles.append(mainCpp);

        // add any qmake Qt resource files and qmake project files
        exampleFiles += Config::getFilesHere(fullPath, "*.qrc *.pro *.qmlproject qmldir");
    }

    foreach (const QString &exampleFile, exampleFiles)
        (void) new FakeNode(fake,
                            exampleFile.mid(sizeOfBoringPartOfName),
                            Node::File);
    foreach (const QString &imageFile, imageFiles) {
        new FakeNode(fake,
                     imageFile.mid(sizeOfBoringPartOfName),
                     Node::Image);
    }
}

QT_END_NAMESPACE
