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
  cppcodeparser.cpp
*/

#include <qfile.h>
#include <stdio.h>
#include <errno.h>
#include "codechunk.h"
#include "config.h"
#include "cppcodeparser.h"
#include "tokenizer.h"
#include "qdocdatabase.h"
#include <qdebug.h>
#include "generator.h"

QT_BEGIN_NAMESPACE

/* qmake ignore Q_OBJECT */

static bool inMacroCommand_ = false;
static bool parsingHeaderFile_ = false;
QStringList CppCodeParser::exampleFiles;
QStringList CppCodeParser::exampleDirs;
CppCodeParser* CppCodeParser::cppParser_ = 0;

/*!
  The constructor initializes some regular expressions
  and calls reset().
 */
CppCodeParser::CppCodeParser()
    : varComment("/\\*\\s*([a-zA-Z_0-9]+)\\s*\\*/"), sep("(?:<[^>]+>)?::")
{
    reset();
    cppParser_ = this;
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

    /*
      All these can appear in a C++ namespace. Don't add
      anything that can't be in a C++ namespace.
     */
    nodeTypeMap.insert(COMMAND_NAMESPACE, Node::Namespace);
    nodeTypeMap.insert(COMMAND_CLASS, Node::Class);
    nodeTypeMap.insert(COMMAND_ENUM, Node::Enum);
    nodeTypeMap.insert(COMMAND_TYPEDEF, Node::Typedef);
    nodeTypeMap.insert(COMMAND_PROPERTY, Node::Property);
    nodeTypeMap.insert(COMMAND_VARIABLE, Node::Variable);

    exampleFiles = config.getCanonicalPathList(CONFIG_EXAMPLES);
    exampleDirs = config.getCanonicalPathList(CONFIG_EXAMPLEDIRS);
    QStringList exampleFilePatterns = config.getStringList(
                CONFIG_EXAMPLES + Config::dot + CONFIG_FILEEXTENSIONS);

    if (!exampleFilePatterns.isEmpty())
        exampleNameFilter = exampleFilePatterns.join(' ');
    else
        exampleNameFilter = "*.cpp *.h *.js *.xq *.svg *.xml *.dita *.ui";

    QStringList exampleImagePatterns = config.getStringList(
                CONFIG_EXAMPLES + Config::dot + CONFIG_IMAGEEXTENSIONS);

    if (!exampleImagePatterns.isEmpty())
        exampleImageFilter = exampleImagePatterns.join(' ');
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
  Parse the C++ header file identified by \a filePath and add
  the parsed contents to the database. The \a location is used
  for reporting errors.
 */
void CppCodeParser::parseHeaderFile(const Location& location, const QString& filePath)
{
    QFile in(filePath);
    currentFile_ = filePath;
    if (!in.open(QIODevice::ReadOnly)) {
        location.error(tr("Cannot open C++ header file '%1'").arg(filePath));
        currentFile_.clear();
        return;
    }

    reset();
    Location fileLocation(filePath);
    Tokenizer fileTokenizer(fileLocation, in);
    tokenizer = &fileTokenizer;
    readToken();
    parsingHeaderFile_ = true;
    matchDeclList(qdb_->primaryTreeRoot());
    parsingHeaderFile_ = false;
    if (!fileTokenizer.version().isEmpty())
        qdb_->setVersion(fileTokenizer.version());
    in.close();

    if (fileLocation.fileName() == "qiterator.h")
        parseQiteratorDotH(location, filePath);
    currentFile_.clear();
}

/*!
  Get ready to parse the C++ cpp file identified by \a filePath
  and add its parsed contents to the database. \a location is
  used for reporting errors.

  Call matchDocsAndStuff() to do all the parsing and tree building.
 */
void CppCodeParser::parseSourceFile(const Location& location, const QString& filePath)
{
    QFile in(filePath);
    currentFile_ = filePath;
    if (!in.open(QIODevice::ReadOnly)) {
        location.error(tr("Cannot open C++ source file '%1' (%2)").arg(filePath).arg(strerror(errno)));
        currentFile_.clear();
        return;
    }

    reset();
    Location fileLocation(filePath);
    Tokenizer fileTokenizer(fileLocation, in);
    tokenizer = &fileTokenizer;
    readToken();

    /*
      The set of open namespaces is cleared before parsing
      each source file. The word "source" here means cpp file.
     */
    qdb_->clearOpenNamespaces();

    matchDocsAndStuff();
    in.close();
    currentFile_.clear();
}

/*!
  This is called after all the C++ header files have been
  parsed. The most important thing it does is resolve C++
  class inheritance links in the tree. It also initializes
  a bunch of other collections.
 */
void CppCodeParser::doneParsingHeaderFiles()
{
    QMapIterator<QString, QString> i(sequentialIteratorClasses);
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(), i.value(), sequentialIteratorDefinition);
    }
    i = mutableSequentialIteratorClasses;
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(), i.value(), mutableSequentialIteratorDefinition);
    }
    i = associativeIteratorClasses;
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(), i.value(), associativeIteratorDefinition);
    }
    i = mutableAssociativeIteratorClasses;
    while (i.hasNext()) {
        i.next();
        instantiateIteratorMacro(i.key(), i.value(), mutableAssociativeIteratorDefinition);
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
  header files) have been parsed. Currently nothing to do.
 */
void CppCodeParser::doneParsingSourceFiles()
{
    // contents moved to QdocDatabase::resolveIssues()
}

static QSet<QString> topicCommands_;
/*!
  Returns the set of strings reopresenting the topic commands.
 */
const QSet<QString>& CppCodeParser::topicCommands()
{
    if (topicCommands_.isEmpty()) {
        topicCommands_ << COMMAND_CLASS
                       << COMMAND_DITAMAP
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
                       << COMMAND_TYPEDEF
                       << COMMAND_VARIABLE
                       << COMMAND_QMLTYPE
                       << COMMAND_QMLPROPERTY
                       << COMMAND_QMLPROPERTYGROUP
                       << COMMAND_QMLATTACHEDPROPERTY
                       << COMMAND_QMLSIGNAL
                       << COMMAND_QMLATTACHEDSIGNAL
                       << COMMAND_QMLMETHOD
                       << COMMAND_QMLATTACHEDMETHOD
                       << COMMAND_QMLBASICTYPE
                       << COMMAND_QMLMODULE
                       << COMMAND_JSTYPE
                       << COMMAND_JSPROPERTY
                       << COMMAND_JSPROPERTYGROUP
                       << COMMAND_JSATTACHEDPROPERTY
                       << COMMAND_JSSIGNAL
                       << COMMAND_JSATTACHEDSIGNAL
                       << COMMAND_JSMETHOD
                       << COMMAND_JSATTACHEDMETHOD
                       << COMMAND_JSBASICTYPE
                       << COMMAND_JSMODULE;
    }
    return topicCommands_;
}

/*!
  Process the topic \a command found in the \a doc with argument \a arg.
 */
Node* CppCodeParser::processTopicCommand(const Doc& doc,
                                         const QString& command,
                                         const ArgLocPair& arg)
{
    ExtraFuncData extra;
    if (command == COMMAND_FN) {
        QStringList parentPath;
        FunctionNode *func = 0;
        FunctionNode *clone = 0;

        if (!makeFunctionNode(arg.first, &parentPath, &clone, extra) &&
            !makeFunctionNode("void " + arg.first, &parentPath, &clone, extra)) {
            doc.startLocation().warning(tr("Invalid syntax in '\\%1'").arg(COMMAND_FN));
        }
        else {
            func = qdb_->findFunctionNode(parentPath, clone);
            if (func == 0) {
                if (parentPath.isEmpty() && !lastPath_.isEmpty())
                    func = qdb_->findFunctionNode(lastPath_, clone);
            }

            /*
              If the node was not found, then search for it in the
              open C++ namespaces. We don't expect this search to
              be necessary often. Nor do we expect it to succeed
              very often.
            */
            if (func == 0)
                func = qdb_->findNodeInOpenNamespace(parentPath, clone);

            if (func == 0) {
                doc.location().warning(tr("Cannot find '%1' in '\\%2' %3")
                                       .arg(clone->name() + "(...)")
                                       .arg(COMMAND_FN)
                                       .arg(arg.first),
                                       tr("I cannot find any function of that name with the "
                                          "specified signature. Make sure that the signature "
                                          "is identical to the declaration, including 'const' "
                                          "qualifiers."));
            }
            else
                lastPath_ = parentPath;
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

        extra.root = qdb_->primaryTreeRoot();
        extra.isMacro = true;
        if (makeFunctionNode(arg.first, &parentPath, &func, extra)) {
            if (!parentPath.isEmpty()) {
                doc.startLocation().warning(tr("Invalid syntax in '\\%1'").arg(COMMAND_MACRO));
                delete func;
                func = 0;
            }
            else {
                func->setMetaness(FunctionNode::MacroWithParams);
                QVector<Parameter> params = func->parameters();
                for (int i = 0; i < params.size(); ++i) {
                    Parameter &param = params[i];
                    if (param.name().isEmpty() && !param.dataType().isEmpty()
                            && param.dataType() != "...")
                        param = Parameter("", "", param.dataType());
                }
                func->setParameters(params);
            }
            return func;
        }
        else if (QRegExp("[A-Za-z_][A-Za-z0-9_]+").exactMatch(arg.first)) {
            func = new FunctionNode(qdb_->primaryTreeRoot(), arg.first);
            func->setAccess(Node::Public);
            func->setLocation(doc.startLocation());
            func->setMetaness(FunctionNode::MacroWithoutParams);
        }
        else {
            doc.location().warning(tr("Invalid syntax in '\\%1'").arg(COMMAND_MACRO));

        }
        return func;
    }
    else if (nodeTypeMap.contains(command)) {
        /*
          We should only get in here if the command refers to
          something that can appear in a C++ namespace,
          i.e. a class, another namespace, an enum, a typedef,
          a property or a variable. I think these are handled
          this way to allow the writer to refer to the entity
          without including the namespace qualifier.
         */
        Node::NodeType type =  nodeTypeMap[command];
        QStringList paths = arg.first.split(QLatin1Char(' '));
        QStringList path = paths[0].split("::");
        Node *node = 0;

        node = qdb_->findNodeInOpenNamespace(path, type);
        if (node == 0)
            node = qdb_->findNodeByNameAndType(path, type);
        if (node == 0) {
            doc.location().warning(tr("Cannot find '%1' specified with '\\%2' in any header file")
                                   .arg(arg.first).arg(command));
            lastPath_ = path;

        }
        else if (node->isAggregate()) {
            if (type == Node::Namespace) {
                NamespaceNode* ns = static_cast<NamespaceNode*>(node);
                ns->markSeen();
            }
            /*
              This treats a class as a namespace.
             */
            if ((type == Node::Class) || (type == Node::Namespace)) {
                if (path.size() > 1) {
                    path.pop_back();
                    QString ns = path.join("::");
                    qdb_->insertOpenNamespace(ns);
                }
            }
        }
        return node;
    }
    else if (command == COMMAND_EXAMPLE) {
        if (Config::generateExamples) {
            ExampleNode* en = new ExampleNode(qdb_->primaryTreeRoot(), arg.first);
            en->setLocation(doc.startLocation());
            createExampleFileNodes(en);
            return en;
        }
    }
    else if (command == COMMAND_EXTERNALPAGE) {
        DocumentNode* dn = new DocumentNode(qdb_->primaryTreeRoot(),
                                            arg.first,
                                            Node::ExternalPage,
                                            Node::ArticlePage);
        dn->setLocation(doc.startLocation());
        return dn;
    }
    else if (command == COMMAND_FILE) {
        DocumentNode* dn = new DocumentNode(qdb_->primaryTreeRoot(),
                                            arg.first,
                                            Node::File,
                                            Node::NoPageType);
        dn->setLocation(doc.startLocation());
        return dn;
    }
    else if (command == COMMAND_HEADERFILE) {
        DocumentNode* dn = new DocumentNode(qdb_->primaryTreeRoot(),
                                            arg.first,
                                            Node::HeaderFile,
                                            Node::ApiPage);
        dn->setLocation(doc.startLocation());
        return dn;
    }
    else if (command == COMMAND_GROUP) {
        CollectionNode* cn = qdb_->addGroup(arg.first);
        cn->setLocation(doc.startLocation());
        cn->markSeen();
        return cn;
    }
    else if (command == COMMAND_MODULE) {
        CollectionNode* cn = qdb_->addModule(arg.first);
        cn->setLocation(doc.startLocation());
        cn->markSeen();
        return cn;
    }
    else if (command == COMMAND_QMLMODULE) {
        QStringList blankSplit = arg.first.split(QLatin1Char(' '));
        CollectionNode* cn = qdb_->addQmlModule(blankSplit[0]);
        cn->setLogicalModuleInfo(blankSplit);
        cn->setLocation(doc.startLocation());
        cn->markSeen();
        return cn;
    }
    else if (command == COMMAND_JSMODULE) {
        QStringList blankSplit = arg.first.split(QLatin1Char(' '));
        CollectionNode* cn = qdb_->addJsModule(blankSplit[0]);
        cn->setLogicalModuleInfo(blankSplit);
        cn->setLocation(doc.startLocation());
        cn->markSeen();
        return cn;
    }
    else if (command == COMMAND_PAGE) {
        Node::PageType ptype = Node::ArticlePage;
        QStringList args = arg.first.split(QLatin1Char(' '));
        if (args.size() > 1) {
            QString t = args[1].toLower();
            if (t == "howto")
                ptype = Node::HowToPage;
            else if (t == "api")
                ptype = Node::ApiPage;
            else if (t == "example")
                ptype = Node::ExamplePage;
            else if (t == "overview")
                ptype = Node::OverviewPage;
            else if (t == "tutorial")
                ptype = Node::TutorialPage;
            else if (t == "faq")
                ptype = Node::FAQPage;
            else if (t == "ditamap")
                ptype = Node::DitaMapPage;
        }
        DocumentNode* dn = 0;
        if (ptype == Node::DitaMapPage)
            dn = new DitaMapNode(qdb_->primaryTreeRoot(), args[0]);
        else
            dn = new DocumentNode(qdb_->primaryTreeRoot(), args[0], Node::Page, ptype);
        dn->setLocation(doc.startLocation());
        return dn;
    }
    else if (command == COMMAND_DITAMAP) {
        DocumentNode* dn = new DitaMapNode(qdb_->primaryTreeRoot(), arg.first);
        dn->setLocation(doc.startLocation());
        return dn;
    }
    else if ((command == COMMAND_QMLTYPE) || (command == COMMAND_JSTYPE)) {
        QmlTypeNode* qcn = new QmlTypeNode(qdb_->primaryTreeRoot(), arg.first);
        if (command == COMMAND_JSTYPE)
            qcn->setGenus(Node::JS);
        qcn->setLocation(doc.startLocation());
        return qcn;
    }
    else if ((command == COMMAND_QMLBASICTYPE) || (command == COMMAND_JSBASICTYPE)) {
        QmlBasicTypeNode* n = new QmlBasicTypeNode(qdb_->primaryTreeRoot(), arg.first);
        if (command == COMMAND_JSBASICTYPE)
            n->setGenus(Node::JS);
        n->setLocation(doc.startLocation());
        return n;
    }
    else if ((command == COMMAND_QMLSIGNAL) ||
             (command == COMMAND_QMLMETHOD) ||
             (command == COMMAND_QMLATTACHEDSIGNAL) ||
             (command == COMMAND_QMLATTACHEDMETHOD) ||
             (command == COMMAND_JSSIGNAL) ||
             (command == COMMAND_JSMETHOD) ||
             (command == COMMAND_JSATTACHEDSIGNAL) ||
             (command == COMMAND_JSATTACHEDMETHOD)) {
        QString module;
        QString name;
        QString type;
        if (splitQmlMethodArg(arg.first, type, module, name)) {
            Aggregate* aggregate = qdb_->findQmlType(module, name);
            if (!aggregate)
                aggregate = qdb_->findQmlBasicType(module, name);
            if (aggregate) {
                bool attached = false;
                Node::NodeType nodeType = Node::QmlMethod;
                if ((command == COMMAND_QMLSIGNAL) ||
                    (command == COMMAND_JSSIGNAL))
                    nodeType = Node::QmlSignal;
                else if ((command == COMMAND_QMLATTACHEDSIGNAL) ||
                         (command == COMMAND_JSATTACHEDSIGNAL)) {
                    nodeType = Node::QmlSignal;
                    attached = true;
                }
                else if ((command == COMMAND_QMLMETHOD) ||
                         (command == COMMAND_JSMETHOD)) {
                    // do nothing
                }
                else if ((command == COMMAND_QMLATTACHEDMETHOD) ||
                         (command == COMMAND_JSATTACHEDMETHOD))
                    attached = true;
                else
                    return 0; // never get here.
                FunctionNode* fn = makeFunctionNode(doc,
                                                    arg.first,
                                                    aggregate,
                                                    nodeType,
                                                    attached,
                                                    command);
                if (fn) {
                    fn->setLocation(doc.startLocation());
                    if ((command == COMMAND_JSSIGNAL) ||
                        (command == COMMAND_JSMETHOD) ||
                        (command == COMMAND_JSATTACHEDSIGNAL) ||
                        (command == COMMAND_JSATTACHEDMETHOD))
                        fn->setGenus(Node::JS);
                }
                return fn;
            }
        }
    }
    return 0;
}

/*!
  A QML property group argument has the form...

  <QML-module>::<QML-type>::<name>

  This function splits the argument into those parts.
  A <QML-module> is the QML equivalent of a C++ namespace.
  So this function splits \a arg on "::" and stores the
  parts in \a module, \a qmlTypeName, and \a name, and returns
  true. If any part is not found, a qdoc warning is emitted
  and false is returned.
 */
bool CppCodeParser::splitQmlPropertyGroupArg(const QString& arg,
                                             QString& module,
                                             QString& qmlTypeName,
                                             QString& name)
{
    QStringList colonSplit = arg.split("::");
    if (colonSplit.size() == 3) {
        module = colonSplit[0];
        qmlTypeName = colonSplit[1];
        name = colonSplit[2];
        return true;
    }
    QString msg = "Unrecognizable QML module/component qualifier for " + arg;
    location().warning(tr(msg.toLatin1().data()));
    return false;
}

/*!
  A QML property argument has the form...

  <type> <QML-type>::<name>
  <type> <QML-module>::<QML-type>::<name>

  This function splits the argument into one of those
  two forms. The three part form is the old form, which
  was used before the creation of Qt Quick 2 and Qt
  Components. A <QML-module> is the QML equivalent of a
  C++ namespace. So this function splits \a arg on "::"
  and stores the parts in \a type, \a module, \a qmlTypeName,
  and \a name, and returns \c true. If any part other than
  \a module is not found, a qdoc warning is emitted and
  false is returned.

  \note The two QML types \e{Component} and \e{QtObject}
  never have a module qualifier.
 */
bool CppCodeParser::splitQmlPropertyArg(const QString& arg,
                                        QString& type,
                                        QString& module,
                                        QString& qmlTypeName,
                                        QString& name)
{
    QStringList blankSplit = arg.split(QLatin1Char(' '));
    if (blankSplit.size() > 1) {
        type = blankSplit[0];
        QStringList colonSplit(blankSplit[1].split("::"));
        if (colonSplit.size() == 3) {
            module = colonSplit[0];
            qmlTypeName = colonSplit[1];
            name = colonSplit[2];
            return true;
        }
        if (colonSplit.size() == 2) {
            module.clear();
            qmlTypeName = colonSplit[0];
            name = colonSplit[1];
            return true;
        }
        QString msg = "Unrecognizable QML module/component qualifier for " + arg;
        location().warning(tr(msg.toLatin1().data()));
    }
    else {
        QString msg = "Missing property type for " + arg;
        location().warning(tr(msg.toLatin1().data()));
    }
    return false;
}

/*!
  A QML signal or method argument has the form...

  <type> <QML-type>::<name>(<param>, <param>, ...)
  <type> <QML-module>::<QML-type>::<name>(<param>, <param>, ...)

  This function splits the \a{arg}ument into one of those
  two forms, sets \a type, \a module, and \a qmlTypeName,
  and returns true. If the argument doesn't match either
  form, an error message is emitted and false is returned.

  \note The two QML types \e{Component} and \e{QtObject} never
  have a module qualifier.
 */
bool CppCodeParser::splitQmlMethodArg(const QString& arg,
                                      QString& type,
                                      QString& module,
                                      QString& qmlTypeName)
{
    QString name;
    int leftParen = arg.indexOf(QChar('('));
    if (leftParen > 0)
        name = arg.left(leftParen);
    else
        name = arg;
    int firstBlank = name.indexOf(QChar(' '));
    if (firstBlank > 0) {
        type = name.left(firstBlank);
        name = name.right(name.length() - firstBlank - 1);
    }
    else
        type.clear();

    QStringList colonSplit(name.split("::"));
    if (colonSplit.size() > 1) {
        if (colonSplit.size() > 2) {
            module = colonSplit[0];
            qmlTypeName = colonSplit[1];
        }
        else {
            module.clear();
            qmlTypeName = colonSplit[0];
        }
        return true;
    }
    QString msg = "Unrecognizable QML module/component qualifier for " + arg;
    location().warning(tr(msg.toLatin1().data()));
    return false;
}

/*!
  Process the topic \a command group found in the \a doc with arguments \a args.

  Currently, this function is called only for \e{qmlproperty}
  and \e{qmlattachedproperty}.
 */
void CppCodeParser::processQmlProperties(const Doc& doc,
                                         NodeList& nodes,
                                         DocList& docs,
                                         bool jsProps)
{
    QString arg;
    QString type;
    QString topic;
    QString module;
    QString qmlTypeName;
    QString property;
    QmlPropertyNode* qpn = 0;
    QmlTypeNode* qmlType = 0;
    QmlPropertyGroupNode* qpgn = 0;

    Topic qmlPropertyGroupTopic;
    const TopicList& topics = doc.topicsUsed();
    for (int i=0; i<topics.size(); ++i) {
        if ((topics.at(i).topic == COMMAND_QMLPROPERTYGROUP) ||
            (topics.at(i).topic == COMMAND_JSPROPERTYGROUP)) {
            qmlPropertyGroupTopic = topics.at(i);
            break;
        }
    }
    if (qmlPropertyGroupTopic.isEmpty() && topics.size() > 1) {
        qmlPropertyGroupTopic = topics.at(0);
        if (jsProps)
            qmlPropertyGroupTopic.topic = COMMAND_JSPROPERTYGROUP;
        else
            qmlPropertyGroupTopic.topic = COMMAND_QMLPROPERTYGROUP;
        arg = qmlPropertyGroupTopic.args;
        if (splitQmlPropertyArg(arg, type, module, qmlTypeName, property)) {
            int i = property.indexOf('.');
            if (i != -1) {
                property = property.left(i);
                qmlPropertyGroupTopic.args = module + "::" + qmlTypeName + "::" + property;
                doc.location().warning(tr("No QML property group command found; using \\%1 %2")
                                       .arg(COMMAND_QMLPROPERTYGROUP).arg(qmlPropertyGroupTopic.args));
            }
            else {
                /*
                  Assumption: No '.' in the property name
                  means there is no property group.
                 */
                qmlPropertyGroupTopic.clear();
            }
        }
    }

    if (!qmlPropertyGroupTopic.isEmpty()) {
        arg = qmlPropertyGroupTopic.args;
        if (splitQmlPropertyGroupArg(arg, module, qmlTypeName, property)) {
            qmlType = qdb_->findQmlType(module, qmlTypeName);
            if (qmlType) {
                qpgn = new QmlPropertyGroupNode(qmlType, property);
                qpgn->setLocation(doc.startLocation());
                if (jsProps)
                    qpgn->setGenus(Node::JS);
                nodes.append(qpgn);
                docs.append(doc);
            }
        }
    }
    for (int i=0; i<topics.size(); ++i) {
        if (topics.at(i).topic == COMMAND_QMLPROPERTYGROUP) {
             continue;
        }
        topic = topics.at(i).topic;
        arg = topics.at(i).args;
        if ((topic == COMMAND_QMLPROPERTY) || (topic == COMMAND_QMLATTACHEDPROPERTY) ||
            (topic == COMMAND_JSPROPERTY) || (topic == COMMAND_JSATTACHEDPROPERTY)) {
            bool attached = ((topic == COMMAND_QMLATTACHEDPROPERTY) ||
                             (topic == COMMAND_JSATTACHEDPROPERTY));
            if (splitQmlPropertyArg(arg, type, module, qmlTypeName, property)) {
                Aggregate* aggregate = qdb_->findQmlType(module, qmlTypeName);
                if (!aggregate)
                    aggregate = qdb_->findQmlBasicType(module, qmlTypeName);
                if (aggregate) {
                    if (aggregate->hasQmlProperty(property, attached) != 0) {
                        QString msg = tr("QML property documented multiple times: '%1'").arg(arg);
                        doc.startLocation().warning(msg);
                    }
                    else if (qpgn) {
                        qpn = new QmlPropertyNode(qpgn, property, type, attached);
                        qpn->setLocation(doc.startLocation());
                        if (jsProps)
                            qpn->setGenus(Node::JS);
                    }
                    else {
                        qpn = new QmlPropertyNode(aggregate, property, type, attached);
                        qpn->setLocation(doc.startLocation());
                        if (jsProps)
                            qpn->setGenus(Node::JS);
                        nodes.append(qpn);
                        docs.append(doc);
                    }
                }
            }
        } else if (qpgn) {
            doc.startLocation().warning(
                tr("Invalid use of '\\%1'; not allowed in a '\\%2'").arg(
                    topic, qmlPropertyGroupTopic.topic));
        }
    }
}

static QSet<QString> otherMetaCommands_;
/*!
  Returns the set of strings representing the common metacommands
  plus some other metacommands.
 */
const QSet<QString>& CppCodeParser::otherMetaCommands()
{
    if (otherMetaCommands_.isEmpty()) {
        otherMetaCommands_ = commonMetaCommands();
        otherMetaCommands_ << COMMAND_INHEADERFILE
                           << COMMAND_OVERLOAD
                           << COMMAND_REIMP
                           << COMMAND_RELATES
                           << COMMAND_CONTENTSPAGE
                           << COMMAND_NEXTPAGE
                           << COMMAND_PREVIOUSPAGE
                           << COMMAND_INDEXPAGE
                           << COMMAND_STARTPAGE
                           << COMMAND_QMLINHERITS
                           << COMMAND_QMLINSTANTIATES
                           << COMMAND_QMLDEFAULT
                           << COMMAND_QMLREADONLY
                           << COMMAND_QMLABSTRACT
                           << COMMAND_ABSTRACT;
    }
    return otherMetaCommands_;
}

/*!
  Process the metacommand \a command in the context of the
  \a node associated with the topic command and the \a doc.
  \a arg is the argument to the metacommand.
 */
void CppCodeParser::processOtherMetaCommand(const Doc& doc,
                                            const QString& command,
                                            const ArgLocPair& argLocPair,
                                            Node *node)
{
    QString arg = argLocPair.first;
    if (command == COMMAND_INHEADERFILE) {
        if (node != 0 && node->isAggregate()) {
            ((Aggregate *) node)->addInclude(arg);
        }
        else {
            doc.location().warning(tr("Ignored '\\%1'").arg(COMMAND_INHEADERFILE));
        }
    }
    else if (command == COMMAND_OVERLOAD) {
        if (node && node->isFunction())
            ((FunctionNode *) node)->setOverloadFlag(true);
        else
            doc.location().warning(tr("Ignored '\\%1'").arg(COMMAND_OVERLOAD));
    }
    else if (command == COMMAND_REIMP) {
        if (node != 0 && node->parent() && !node->parent()->isInternal()) {
            if (node->type() == Node::Function) {
                FunctionNode *func = (FunctionNode *) node;
                const FunctionNode *from = func->reimplementedFrom();
                if (from == 0) {
                    doc.location().warning(tr("Cannot find base function for '\\%1' in %2()")
                                           .arg(COMMAND_REIMP).arg(node->name()),
                                           tr("The function either doesn't exist in any "
                                              "base class with the same signature or it "
                                              "exists but isn't virtual."));
                }
                /*
                  Ideally, we would enable this check to warn whenever
                  \reimp is used incorrectly, and only make the node
                  internal if the function is a reimplementation of
                  another function in a base class.
                */
                else if (from->access() == Node::Private
                         || from->parent()->access() == Node::Private) {
                    doc.location().warning(tr("'\\%1' in %2() should be '\\internal' "
                                              "because its base function is private "
                                              "or internal").arg(COMMAND_REIMP).arg(node->name()));
                }
                func->setReimplemented(true);
            }
            else {
                doc.location().warning(tr("Ignored '\\%1' in %2").arg(COMMAND_REIMP).arg(node->name()));
            }
        }
    }
    else if (command == COMMAND_RELATES) {
        QStringList path = arg.split("::");
        Node* n = qdb_->findRelatesNode(path);
        if (!n) {
            // Store just a string to write to the index file
            if (Generator::preparing())
                node->setRelates(arg);
            else
                doc.location().warning(tr("Cannot find '%1' in '\\%2'").arg(arg).arg(COMMAND_RELATES));

        }
        else if (node->parent() != n)
            node->setRelates(static_cast<Aggregate*>(n));
        else
            doc.location().warning(tr("Invalid use of '\\%1' (already a member of '%2')")
                                   .arg(COMMAND_RELATES, arg));
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
    else if (command == COMMAND_QMLINHERITS) {
        if (node->name() == arg)
            doc.location().warning(tr("%1 tries to inherit itself").arg(arg));
        else if (node->isQmlType() || node->isJsType()) {
            QmlTypeNode* qmlType = static_cast<QmlTypeNode*>(node);
            qmlType->setQmlBaseName(arg);
            QmlTypeNode::addInheritedBy(arg,node);
        }
    }
    else if (command == COMMAND_QMLINSTANTIATES) {
        if (node->isQmlType() || node->isJsType()) {
            ClassNode* classNode = qdb_->findClassNode(arg.split("::"));
            if (classNode)
                node->setClassNode(classNode);
            else
                doc.location().warning(tr("C++ class %1 not found: \\instantiates %1").arg(arg));
        }
        else
            doc.location().warning(tr("\\instantiates is only allowed in \\qmltype"));
    }
    else if (command == COMMAND_QMLDEFAULT) {
        if (node->type() == Node::QmlProperty) {
            QmlPropertyNode* qpn = static_cast<QmlPropertyNode*>(node);
            qpn->setDefault();
        }
        else if (node->type() == Node::QmlPropertyGroup) {
            QmlPropertyGroupNode* qpgn = static_cast<QmlPropertyGroupNode*>(node);
            NodeList::ConstIterator p = qpgn->childNodes().constBegin();
            while (p != qpgn->childNodes().constEnd()) {
                if ((*p)->type() == Node::QmlProperty) {
                    QmlPropertyNode* qpn = static_cast<QmlPropertyNode*>(*p);
                    qpn->setDefault();
                }
                ++p;
            }
        }
    }
    else if (command == COMMAND_QMLREADONLY) {
        if (node->type() == Node::QmlProperty) {
            QmlPropertyNode* qpn = static_cast<QmlPropertyNode*>(node);
            qpn->setReadOnly(1);
        }
        else if (node->type() == Node::QmlPropertyGroup) {
            QmlPropertyGroupNode* qpgn = static_cast<QmlPropertyGroupNode*>(node);
            NodeList::ConstIterator p = qpgn->childNodes().constBegin();
            while (p != qpgn->childNodes().constEnd()) {
                if ((*p)->type() == Node::QmlProperty) {
                    QmlPropertyNode* qpn = static_cast<QmlPropertyNode*>(*p);
                    qpn->setReadOnly(1);
                }
                ++p;
            }
        }
    }
    else if ((command == COMMAND_QMLABSTRACT) || (command == COMMAND_ABSTRACT)) {
        if (node->isQmlType() || node->isJsType())
            node->setAbstract(true);
    }
    else {
        processCommonMetaCommand(doc.location(),command,argLocPair,node);
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
    QSet<QString>::ConstIterator cmd = metaCommands.constBegin();
    while (cmd != metaCommands.constEnd()) {
        ArgList args = doc.metaCommandArgs(*cmd);
        ArgList::ConstIterator arg = args.constBegin();
        while (arg != args.constEnd()) {
            processOtherMetaCommand(doc, *cmd, *arg, node);
            ++arg;
        }
        ++cmd;
    }
}

/*!
  Resets the C++ code parser to its default initialized state.
 */
void CppCodeParser::reset()
{
    tokenizer = 0;
    tok = 0;
    access = Node::Public;
    metaness_ = FunctionNode::Plain;
    lastPath_.clear();
    physicalModuleName.clear();
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
    return tok == target;
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

bool CppCodeParser::matchModuleQualifier(QString& name)
{
    bool matches = (lexeme() == QString('.'));
    if (matches) {
        do {
            name += lexeme();
            readToken();
        } while ((tok == Tok_Ident) || (lexeme() == QString('.')));
    }
    return matches;
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

/*
  This function is no longer used.
 */
bool CppCodeParser::matchTemplateHeader()
{
    readToken();
    return matchTemplateAngles();
}

bool CppCodeParser::matchDataType(CodeChunk *dataType, QString *var, bool qProp)
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
            if (match(Tok_Ident)) {
                /*
                  This is a hack until we replace this "parser"
                  with the real one used in Qt Creator.
                 */
                if (!inMacroCommand_ && lexeme() == "(" &&
                    ((previousLexeme() == "QT_PREPEND_NAMESPACE") || (previousLexeme() == "NS"))) {
                    readToken();
                    readToken();
                    dataType->append(previousLexeme());
                    readToken();
                }
                else
                    dataType->append(previousLexeme());
            }
            else if (match(Tok_void) || match(Tok_int) || match(Tok_char) ||
                     match(Tok_double) || match(Tok_Ellipsis)) {
                dataType->append(previousLexeme());
            }
            else {
                return false;
            }
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
           match(Tok_Caret) || match(Tok_Ellipsis))
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
        if (!match(Tok_RightParen) || tok != Tok_LeftParen) {
            return false;
        }
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
            else if (qProp && (match(Tok_default) || match(Tok_final))) {
                // Hack to make 'default' and 'final' work again in Q_PROPERTY
                *var = previousLexeme();
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

/*!
  Parse the next function parameter, if there is one, and
  append it to parameter vector \a pvect. Return true if
  a parameter is parsed and appended to \a pvect.
  Otherwise return false.
 */
bool CppCodeParser::matchParameter(QVector<Parameter>& pvect, bool& isQPrivateSignal)
{
    if (match(Tok_QPrivateSignal)) {
        isQPrivateSignal = true;
        return true;
    }

    Parameter p;
    CodeChunk chunk;
    if (!matchDataType(&chunk, &p.name_)) {
        return false;
    }
    p.dataType_ = chunk.toString();
    chunk.clear();
    match(Tok_Comment);
    if (match(Tok_Equal)) {
        int pdepth = tokenizer->parenDepth();
        while (tokenizer->parenDepth() >= pdepth &&
               (tok != Tok_Comma || (tokenizer->parenDepth() > pdepth)) &&
               tok != Tok_Eoi) {
            chunk.append(lexeme());
            readToken();
        }
    }
    p.defaultValue_ = chunk.toString();
    pvect.append(p);
    return true;
}

/*!
  If the current token is any of several function modifiers,
  return that token value after reading the next token. If it
  is not one of the function modieifer tokens, return -1 but
  don\t read the next token.
 */
int CppCodeParser::matchFunctionModifier()
{
    switch (tok) {
    case Tok_friend:
    case Tok_inline:
    case Tok_explicit:
    case Tok_static:
    case Tok_QT_DEPRECATED:
        readToken();
        return tok;
    case Tok_QT_COMPAT:
    case Tok_QT_COMPAT_CONSTRUCTOR:
    case Tok_QT_MOC_COMPAT:
    case Tok_QT3_SUPPORT:
    case Tok_QT3_SUPPORT_CONSTRUCTOR:
    case Tok_QT3_MOC_SUPPORT:
        readToken();
        return Tok_QT_COMPAT;
    default:
        break;
    }
    return -1;
}

bool CppCodeParser::matchFunctionDecl(Aggregate *parent,
                                      QStringList *parentPathPtr,
                                      FunctionNode **funcPtr,
                                      const QString &templateStuff,
                                      ExtraFuncData& extra)
{
    CodeChunk returnType;
    QStringList parentPath;
    QString name;

    bool matched_QT_DEPRECATED = false;
    bool matched_friend = false;
    bool matched_static = false;
    bool matched_inline = false;
    bool matched_explicit = false;
    bool matched_compat = false;

    int token = tok;
    while (token != -1) {
        switch (token) {
        case Tok_friend:
            matched_friend = true;
            break;
        case Tok_inline:
            matched_inline = true;
            break;
        case Tok_explicit:
            matched_explicit = true;
            break;
        case Tok_static:
            matched_static = true;
            break;
        case Tok_QT_DEPRECATED:
            // no break here.
            matched_QT_DEPRECATED = true;
        case Tok_QT_COMPAT:
            matched_compat = true;
            break;
        }
        token = matchFunctionModifier();
    }

    FunctionNode::Virtualness virtuality = FunctionNode::NonVirtual;
    if (match(Tok_virtual)) {
        virtuality = FunctionNode::NormalVirtual;
        if (!matched_compat)
            matched_compat = matchCompat();
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

    if (!matched_compat)
        matched_compat = matchCompat();

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
            /*
              This is a hack to let QML module identifiers through.
             */
            matchModuleQualifier(name);
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
                       tok == Tok_Colon || tok == Tok_Equal)
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
            else if (tok == Tok_Colon || tok == Tok_Equal) {
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
            if (parsingHeaderFile_)
                var->setLocation(declLoc());
            else
                var->setLocation(location());
            var->setLeftType(returnType.left());
            var->setRightType(returnType.right());
            if (matched_compat)
                var->setStatus(Node::Compat);
            var->setStatic(matched_static);
            return false;
        }
        if (tok != Tok_LeftParen)
            return false;
    }
    readToken();

    // A left paren was seen. Parse the parameters
    bool isQPrivateSignal = false;
    QVector<Parameter> pvect;
    if (tok != Tok_RightParen) {
        do {
            if (!matchParameter(pvect, isQPrivateSignal))
                return false;
        } while (match(Tok_Comma));
    }
    // The parameters must end with a right paren
    if (!match(Tok_RightParen))
        return false;

    // look for const
    bool matchedConst = match(Tok_const);
    bool matchFinal = match(Tok_final);

    bool isDeleted = false;
    bool isDefaulted = false;
    if (match(Tok_Equal)) {
        if (match(Tok_Number)) // look for 0 indicating pure virtual
            virtuality = FunctionNode::PureVirtual;
        else if (match(Tok_delete))
            isDeleted = true;
        else if (match(Tok_default))
            isDefaulted = true;
    }
    // look for colon indicating ctors which must be skipped
    if (match(Tok_Colon)) {
        while (tok != Tok_LeftBrace && tok != Tok_Eoi)
            readToken();
    }

    // If no ';' expect a body, which must be skipped.
    bool body_expected = false;
    bool body_present = false;
    if (!match(Tok_Semicolon) && tok != Tok_Eoi) {
        body_expected = true;
        int nesting = tokenizer->braceDepth();
        if (!match(Tok_LeftBrace))
            return false;
        // skip the body
        while (tokenizer->braceDepth() >= nesting && tok != Tok_Eoi)
            readToken();
        body_present = true;
        match(Tok_RightBrace);
    }

    FunctionNode *func = 0;
    bool createFunctionNode = false;
    if (parsingHeaderFile_) {
        if (matched_friend) {
            if (matched_inline) {
                // nothing yet
            }
            if (body_present) {
                if (body_expected) {
                    // nothing yet
                }
                createFunctionNode = true;
                if (parent && parent->parent())
                    parent = parent->parent();
                else
                    return false;
            }
        }
        else
            createFunctionNode = true;
    }
    else
        createFunctionNode = true;

    if (createFunctionNode) {
        func = new FunctionNode(extra.type, parent, name, extra.isAttached);
        if (matched_friend)
            access = Node::Public;
        func->setAccess(access);
        if (parsingHeaderFile_)
            func->setLocation(declLoc());
        else
            func->setLocation(location());
        func->setReturnType(returnType.toString());
        func->setParentPath(parentPath);
        func->setTemplateStuff(templateStuff);
        if (matched_compat)
            func->setStatus(Node::Compat);
        if (matched_QT_DEPRECATED)
            func->setStatus(Node::Deprecated);
        if (matched_explicit) { /* What can be done? */ }
        if (!pvect.isEmpty()) {
            func->setParameters(pvect);
        }
        func->setMetaness(metaness_);
        if (parent && (name == parent->name())) {
            FunctionNode::Metaness m = FunctionNode::Ctor;
            if (!pvect.isEmpty()) {
                for (int i=0; i<pvect.size(); i++) {
                    const Parameter& p = pvect.at(i);
                    if (p.dataType().contains(name)) {
                        if (p.dataType().endsWith(QLatin1String("&&"))) {
                            m = FunctionNode::MCtor;
                            break;
                        }
                        if (p.dataType().endsWith(QLatin1String("&"))) {
                            m = FunctionNode::CCtor;
                            break;
                        }
                    }
                }
            }
            func->setMetaness(m);
        }
        else if (name.startsWith(QLatin1Char('~')))
            func->setMetaness(FunctionNode::Dtor);
        else if (name == QLatin1String("operator=")) {
            FunctionNode::Metaness m = FunctionNode::Plain;
            if (parent && pvect.size() == 1) {
                const Parameter& p = pvect.at(0);
                if (p.dataType().contains(parent->name())) {
                    if (p.dataType().endsWith(QLatin1String("&&"))) {
                        m = FunctionNode::MAssign;
                    }
                    else if (p.dataType().endsWith(QLatin1String("&"))) {
                        m = FunctionNode::CAssign;
                    }
                }
            }
            func->setMetaness(m);
        }
        func->setStatic(matched_static);
        func->setConst(matchedConst);
        func->setVirtualness(virtuality);
        func->setIsDeleted(isDeleted);
        func->setIsDefaulted(isDefaulted);
        func->setFinal(matchFinal);
        if (isQPrivateSignal)
            func->setPrivateSignal();
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

    classe->addUnresolvedBaseClass(access, baseClass.toPath(), baseClass.toString());
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
  Parse a C++ class, union, or struct declaration.

  This function only handles one level of class nesting, but that is
  sufficient for Qt because there are no cases of class nesting more
  than one level deep.
 */
bool CppCodeParser::matchClassDecl(Aggregate *parent,
                                   const QString &templateStuff)
{
    bool isClass = (tok == Tok_class);
    readToken();

    bool compat = matchCompat();

    if (tok != Tok_Ident)
        return false;
    while (tok == Tok_Ident)
        readToken();
    if (tok == Tok_Gulbrandsen) {
        Node* n = parent->findChildNode(previousLexeme(),Node::Class);
        if (n) {
            parent = static_cast<Aggregate*>(n);
            if (parent) {
                readToken();
                if (tok != Tok_Ident)
                    return false;
                readToken();
            }
        }
    }

    const QString className = previousLexeme();
    match(Tok_final); // ignore C++11 final class-virt-specifier
    if (tok != Tok_Colon && tok != Tok_LeftBrace)
        return false;

    /*
      So far, so good. We have 'class Foo {' or 'class Foo :'.
      This is enough to recognize a class definition.
    */
    ClassNode *classe = new ClassNode(parent, className);
    classe->setAccess(access);
    classe->setLocation(declLoc());
    if (compat)
        classe->setStatus(Node::Compat);
    if (!physicalModuleName.isEmpty())
        classe->setPhysicalModuleName(physicalModuleName);
    classe->setTemplateStuff(templateStuff);

    if (match(Tok_Colon) && !matchBaseList(classe, isClass))
        return false;
    if (!match(Tok_LeftBrace))
        return false;

    Node::Access outerAccess = access;
    access = isClass ? Node::Private : Node::Public;
    FunctionNode::Metaness outerMetaness = metaness_;
    metaness_ = FunctionNode::Plain;

    bool matches = (matchDeclList(classe) && match(Tok_RightBrace) &&
                    match(Tok_Semicolon));
    access = outerAccess;
    metaness_ = outerMetaness;
    return matches;
}

bool CppCodeParser::matchNamespaceDecl(Aggregate *parent)
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
    NamespaceNode* ns = 0;
    if (parent)
        ns = static_cast<NamespaceNode*>(parent->findChildNode(namespaceName, Node::Namespace));
    if (!ns) {
        ns = new NamespaceNode(parent, namespaceName);
        ns->setAccess(access);
        ns->setLocation(declLoc());
    }

    readToken(); // skip '{'
    bool matched = matchDeclList(ns);
    return matched && match(Tok_RightBrace);
}

/*!
  Match a C++ \c using clause. Return \c true if the match
  is successful. Otherwise false.

  If the \c using clause is for a namespace, an open namespace
  <is inserted for qdoc to look in to find things.

  If the \c using clause is a base class member function, the
  member function is added to \a parent as an unresolved
  \c using clause.
 */
bool CppCodeParser::matchUsingDecl(Aggregate* parent)
{
    bool usingNamespace = false;
    readToken(); // skip 'using'

    if (tok == Tok_namespace) {
        usingNamespace = true;
        readToken();
    }

    int openLeftAngles = 0;
    int openLeftParens = 0;
    bool usingOperator = false;
    QString name;
    while (tok != Tok_Semicolon) {
        if ((tok != Tok_Ident) && (tok != Tok_Gulbrandsen)) {
            if (tok == Tok_LeftAngle) {
                ++openLeftAngles;
            }
            else if (tok == Tok_RightAngle) {
                if (openLeftAngles <= 0)
                    return false;
                --openLeftAngles;
            }
            else if (tok == Tok_Comma) {
                if (openLeftAngles <= 0)
                    return false;
            }
            else if (tok == Tok_operator) {
                usingOperator = true;
            }
            else if (tok == Tok_SomeOperator) {
                if (!usingOperator)
                    return false;
            }
            else if (tok == Tok_LeftParen) {
                ++openLeftParens;
            }
            else if (tok == Tok_RightParen) {
                if (openLeftParens <= 0)
                    return false;
                --openLeftParens;
            }
            else {
                return false;
            }
        }
        name += lexeme();
        readToken();
    }

    if (usingNamespace) {
        // 'using namespace Foo;'.
        qdb_->insertOpenNamespace(name);
    }
    else if (parent && parent->isClass()) {
        ClassNode* cn = static_cast<ClassNode*>(parent);
        cn->addUnresolvedUsingClause(name);
    }
    return true;
}

bool CppCodeParser::matchEnumItem(Aggregate *parent, EnumNode *enume)
{
    if (!match(Tok_Ident))
        return false;

    QString name = previousLexeme();
    CodeChunk val;
    int parenLevel = 0;

    if (match(Tok_Equal)) {
        while (tok != Tok_RightBrace && tok != Tok_Eoi) {
            if (tok == Tok_LeftParen)
                parenLevel++;
            else if (tok == Tok_RightParen)
                parenLevel--;
            else if (tok == Tok_Comma) {
                if (parenLevel <= 0)
                    break;
            }
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
                    if (last.startsWith(QLatin1Char('0')) && last.size() > 1) {
                        if (last.startsWith("0x") || last.startsWith("0X"))
                            strVal = last.left(2) + QString::number(n + 1, 16);
                        else
                            strVal = QLatin1Char('0') + QString::number(n + 1, 8);
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

bool CppCodeParser::matchEnumDecl(Aggregate *parent)
{
    QString name;

    if (!match(Tok_enum))
        return false;
    if (tok == Tok_struct || tok == Tok_class)
        readToken(); // ignore C++11 struct or class attribute
    if (match(Tok_Ident))
        name = previousLexeme();
    if (match(Tok_Colon)) { // ignore C++11 enum-base
        CodeChunk dataType;
        if (!matchDataType(&dataType))
            return false;
    }
    if (tok != Tok_LeftBrace)
        return false;

    EnumNode *enume = 0;

    if (!name.isEmpty()) {
        enume = new EnumNode(parent, name);
        enume->setAccess(access);
        enume->setLocation(declLoc());
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

bool CppCodeParser::matchTypedefDecl(Aggregate *parent)
{
    CodeChunk dataType;
    QString name;

    if (!match(Tok_typedef))
        return false;
    if (!matchDataType(&dataType, &name))
        return false;
    if (!match(Tok_Semicolon))
        return false;

    if (parent && !parent->findChildNode(name, Node::Typedef)) {
        TypedefNode* td = new TypedefNode(parent, name);
        td->setAccess(access);
        td->setLocation(declLoc());
    }
    return true;
}

bool CppCodeParser::matchProperty(Aggregate *parent)
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
    if (!matchDataType(&dataType, &name, true))
        return false;

    PropertyNode *property = new PropertyNode(parent, name);
    property->setAccess(Node::Public);
    property->setLocation(declLoc());
    property->setDataType(dataType.toString());

    while (tok != Tok_RightParen && tok != Tok_Eoi) {
        if (!match(Tok_Ident) && !match(Tok_default) && !match(Tok_final))
            return false;
        QString key = previousLexeme();
        QString value;

        // Keywords with no associated values
        if (key == "CONSTANT") {
            property->setConstant();
            continue;
        }
        else if (key == "FINAL") {
            property->setFinal();
            continue;
        }

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
            qdb_->addPropertyFunction(property, value, PropertyNode::Getter);
        else if (key == "WRITE") {
            qdb_->addPropertyFunction(property, value, PropertyNode::Setter);
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
            qdb_->addPropertyFunction(property, value, PropertyNode::Resetter);
        else if (key == "NOTIFY") {
            qdb_->addPropertyFunction(property, value, PropertyNode::Notifier);
        } else if (key == "REVISION") {
            int revision;
            bool ok;
            revision = value.toInt(&ok);
            if (ok)
                property->setRevision(revision);
            else
                location().warning(tr("Invalid revision number: %1").arg(value));
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
    }
    match(Tok_RightParen);
    return true;
}

/*!
  Parse a C++ declaration.
 */
bool CppCodeParser::matchDeclList(Aggregate *parent)
{
    ExtraFuncData extra;
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
            setDeclLoc();
            matchClassDecl(parent, templateStuff);
            break;
        case Tok_namespace:
            setDeclLoc();
            matchNamespaceDecl(parent);
            break;
        case Tok_using:
            setDeclLoc();
            matchUsingDecl(parent);
            break;
        case Tok_template:
            {
                CodeChunk dataType;
                readToken();
                matchTemplateAngles(&dataType);
                templateStuff = dataType.toString();
            }
            continue;
        case Tok_enum:
            setDeclLoc();
            matchEnumDecl(parent);
            break;
        case Tok_typedef:
            setDeclLoc();
            matchTypedefDecl(parent);
            break;
        case Tok_private:
            readToken();
            access = Node::Private;
            metaness_ = FunctionNode::Plain;
            break;
        case Tok_protected:
            readToken();
            access = Node::Protected;
            metaness_ = FunctionNode::Plain;
            break;
        case Tok_public:
            readToken();
            access = Node::Public;
            metaness_ = FunctionNode::Plain;
            break;
        case Tok_signals:
        case Tok_Q_SIGNALS:
            readToken();
            access = Node::Public;
            metaness_ = FunctionNode::Signal;
            break;
        case Tok_slots:
        case Tok_Q_SLOTS:
            readToken();
            metaness_ = FunctionNode::Slot;
            break;
        case Tok_Q_OBJECT:
            readToken();
            break;
        case Tok_Q_OVERRIDE:
        case Tok_Q_PROPERTY:
        case Tok_Q_PRIVATE_PROPERTY:
        case Tok_QDOC_PROPERTY:
            setDeclLoc();
            if (!matchProperty(parent)) {
                location().warning(tr("Failed to parse token %1 in property declaration").arg(lexeme()));
                skipTo(Tok_RightParen);
                match(Tok_RightParen);
            }
            break;
        case Tok_Q_DECLARE_SEQUENTIAL_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                sequentialIteratorClasses.insert(previousLexeme(), location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_MUTABLE_SEQUENTIAL_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                mutableSequentialIteratorClasses.insert(previousLexeme(), location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_ASSOCIATIVE_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                associativeIteratorClasses.insert(previousLexeme(), location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_MUTABLE_ASSOCIATIVE_ITERATOR:
            readToken();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                mutableAssociativeIteratorClasses.insert(previousLexeme(), location().fileName());
            match(Tok_RightParen);
            break;
        case Tok_Q_DECLARE_FLAGS:
            readToken();
            setDeclLoc();
            if (match(Tok_LeftParen) && match(Tok_Ident)) {
                QString flagsType = previousLexeme();
                if (match(Tok_Comma) && match(Tok_Ident)) {
                    QString name = previousLexeme();
                    TypedefNode *flagsNode = new TypedefNode(parent, flagsType);
                    flagsNode->setAccess(access);
                    flagsNode->setLocation(declLoc());
                    EnumNode* en = static_cast<EnumNode*>(parent->findChildNode(name, Node::Enum));
                    if (en)
                        en->setFlagsType(flagsNode);
                }
            }
            match(Tok_RightParen);
            break;
        case Tok_QT_MODULE:
            readToken();
            setDeclLoc();
            if (match(Tok_LeftParen) && match(Tok_Ident))
                physicalModuleName = previousLexeme();
            if (!physicalModuleName.startsWith("Qt"))
                physicalModuleName.prepend("Qt");
            match(Tok_RightParen);
            break;
        default:
            if (parsingHeaderFile_)
                setDeclLoc();
            if (!matchFunctionDecl(parent, 0, 0, templateStuff, extra)) {
                while (tok != Tok_Eoi &&
                       (tokenizer->braceDepth() > braceDepth0 ||
                        (!match(Tok_Semicolon) &&
                         tok != Tok_public && tok != Tok_protected &&
                         tok != Tok_private))) {
                    readToken();
                }
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
    ExtraFuncData extra;
    const QSet<QString>& topicCommandsAllowed = topicCommands();
    const QSet<QString>& otherMetacommandsAllowed = otherMetaCommands();
    const QSet<QString>& metacommandsAllowed = topicCommandsAllowed + otherMetacommandsAllowed;

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
            Doc doc(start_loc,end_loc,comment,metacommandsAllowed, topicCommandsAllowed);
            QString topic;
            bool isQmlPropertyTopic = false;
            bool isJsPropertyTopic = false;

            const TopicList& topics = doc.topicsUsed();
            if (!topics.isEmpty()) {
                topic = topics[0].topic;
                if ((topic == COMMAND_QMLPROPERTY) ||
                    (topic == COMMAND_QMLPROPERTYGROUP) ||
                    (topic == COMMAND_QMLATTACHEDPROPERTY)) {
                    isQmlPropertyTopic = true;
                }
                else if ((topic == COMMAND_JSPROPERTY) ||
                         (topic == COMMAND_JSPROPERTYGROUP) ||
                         (topic == COMMAND_JSATTACHEDPROPERTY)) {
                    isJsPropertyTopic = true;
                }
            }
            NodeList nodes;
            DocList docs;

            if (topic.isEmpty()) {
                QStringList parentPath;
                FunctionNode *clone;
                FunctionNode *func = 0;

                if (matchFunctionDecl(0, &parentPath, &clone, QString(), extra)) {
                    func = qdb_->findFunctionNode(parentPath, clone);
                    /*
                      If the node was not found, then search for it in the
                      open C++ namespaces. We don't expect this search to
                      be necessary often. Nor do we expect it to succeed
                      very often.
                    */
                    if (func == 0)
                        func = qdb_->findNodeInOpenNamespace(parentPath, clone);

                    if (func) {
                        func->borrowParameterNames(clone);
                        nodes.append(func);
                        docs.append(doc);
                    }
                    delete clone;
                }
                else {
                    doc.location().warning(tr("Cannot tie this documentation to anything"),
                                tr("I found a /*! ... */ comment, but there was no "
                                   "topic command (e.g., '\\%1', '\\%2') in the "
                                   "comment and no function definition following "
                                   "the comment.")
                                .arg(COMMAND_FN).arg(COMMAND_PAGE));
                }
            }
            else if (isQmlPropertyTopic || isJsPropertyTopic) {
                Doc nodeDoc = doc;
                processQmlProperties(nodeDoc, nodes, docs, isJsPropertyTopic);
            }
            else {
                ArgList args;
                const QSet<QString>& topicCommandsUsed = topicCommandsAllowed & doc.metaCommandsUsed();
                if (topicCommandsUsed.count() > 0) {
                    topic = *topicCommandsUsed.constBegin();
                    args = doc.metaCommandArgs(topic);
                }
                if (topicCommandsUsed.count() > 1) {
                    QString topics;
                    QSet<QString>::ConstIterator t = topicCommandsUsed.constBegin();
                    while (t != topicCommandsUsed.constEnd()) {
                        topics += " \\" + *t + QLatin1Char(',');
                        ++t;
                    }
                    topics[topics.lastIndexOf(',')] = '.';
                    int i = topics.lastIndexOf(',');
                    topics[i] = ' ';
                    topics.insert(i+1,"and");
                    doc.location().warning(tr("Multiple topic commands found in comment: %1").arg(topics));
                }
                ArgList::ConstIterator a = args.constBegin();
                while (a != args.constEnd()) {
                    Doc nodeDoc = doc;
                    Node *node = processTopicCommand(nodeDoc,topic,*a);
                    if (node != 0) {
                        nodes.append(node);
                        docs.append(nodeDoc);
                    }
                    ++a;
                }
            }

            NodeList::Iterator n = nodes.begin();
            QList<Doc>::Iterator d = docs.begin();
            while (n != nodes.end()) {
                processOtherMetaCommands(*d, *n);
                (*n)->setDoc(*d);
                checkModuleInclusion(*n);
                if ((*n)->isAggregate() && ((Aggregate *)*n)->includes().isEmpty()) {
                    Aggregate *m = static_cast<Aggregate *>(*n);
                    while (m->parent() && m->physicalModuleName().isEmpty()) {
                        m = m->parent();
                    }
                    if (m == *n)
                        ((Aggregate *)*n)->addInclude((*n)->name());
                    else
                        ((Aggregate *)*n)->setIncludes(m->includes());
                }
                ++d;
                ++n;
            }
        }
        else if (tok == Tok_using) {
            matchUsingDecl(0);
        }
        else {
            QStringList parentPath;
            FunctionNode *clone;
            FunctionNode *fnode = 0;

            if (matchFunctionDecl(0, &parentPath, &clone, QString(), extra)) {
                /*
                  The location of the definition is more interesting
                  than that of the declaration. People equipped with
                  a sophisticated text editor can respond to warnings
                  concerning undocumented functions very quickly.

                  Signals are implemented in uninteresting files
                  generated by moc.
                */
                fnode = qdb_->findFunctionNode(parentPath, clone);
                if (fnode != 0 && !fnode->isSignal())
                    fnode->setLocation(clone->location());
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

/*!
  This function uses a Tokenizer to parse the function \a signature
  in an attempt to match it to the signature of a child node of \a root.
  If a match is found, \a funcPtr is set to point to the matching node
  and true is returned.
 */
bool CppCodeParser::makeFunctionNode(const QString& signature,
                                     QStringList* parentPathPtr,
                                     FunctionNode** funcPtr,
                                     ExtraFuncData& extra)
{
    Tokenizer* outerTokenizer = tokenizer;
    int outerTok = tok;

    QByteArray latin1 = signature.toLatin1();
    Tokenizer stringTokenizer(location(), latin1);
    stringTokenizer.setParsingFnOrMacro(true);
    tokenizer = &stringTokenizer;
    readToken();

    inMacroCommand_ = extra.isMacro;
    bool ok = matchFunctionDecl(extra.root, parentPathPtr, funcPtr, QString(), extra);
    inMacroCommand_ = false;
    // potential memory leak with funcPtr

    tokenizer = outerTokenizer;
    tok = outerTok;
    return ok;
}

/*!
  This function uses a Tokenizer to parse the \a parameters of a
  function into the parameter vector \a {pvect}.
 */
bool CppCodeParser::parseParameters(const QString& parameters,
                                    QVector<Parameter>& pvect,
                                    bool& isQPrivateSignal)
{
    Tokenizer* outerTokenizer = tokenizer;
    int outerTok = tok;

    QByteArray latin1 = parameters.toLatin1();
    Tokenizer stringTokenizer(Location(), latin1);
    stringTokenizer.setParsingFnOrMacro(true);
    tokenizer = &stringTokenizer;
    readToken();

    inMacroCommand_ = false;
    do {
        if (!matchParameter(pvect, isQPrivateSignal))
            return false;
    } while (match(Tok_Comma));

    tokenizer = outerTokenizer;
    tok = outerTok;
    return true;
}

/*!
  Create a new FunctionNode for a QML method or signal, as
  specified by \a type, as a child of \a parent. \a sig is
  the complete signature, and if \a attached is true, the
  method or signal is "attached". \a qdoctag is the text of
  the \a type.

  \a parent is the QML class node. The QML module and QML
  type names have already been consumed to find \a parent.
  What remains in \a sig is the method signature. The method
  must be a child of \a parent.
 */
FunctionNode* CppCodeParser::makeFunctionNode(const Doc& doc,
                                              const QString& sig,
                                              Aggregate* parent,
                                              Node::NodeType type,
                                              bool attached,
                                              QString qdoctag)
{
    QStringList pp;
    FunctionNode* fn = 0;
    ExtraFuncData extra(parent, type, attached);
    if (!makeFunctionNode(sig, &pp, &fn, extra) && !makeFunctionNode("void " + sig, &pp, &fn, extra)) {
        doc.location().warning(tr("Invalid syntax in '\\%1'").arg(qdoctag));
    }
    return fn;
}

void CppCodeParser::parseQiteratorDotH(const Location &location, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
        return;

    QString text = file.readAll();
    text.remove("\r");
    text.remove("\\\n");
    QStringList lines = text.split(QLatin1Char('\n'));
    lines = lines.filter("Q_DECLARE");
    lines.replaceInStrings(QRegExp("#define Q[A-Z_]*\\(C\\)"), QString());

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
                                             const QString &macroDef)
{
    QString resultingCode = macroDef;
    resultingCode.replace(QRegExp("\\bC\\b"), container);
    resultingCode.remove(QRegExp("\\s*##\\s*"));

    Location loc(includeFile);   // hack to get the include file for free
    QByteArray latin1 = resultingCode.toLatin1();
    Tokenizer stringTokenizer(loc, latin1);
    tokenizer = &stringTokenizer;
    readToken();
    matchDeclList(QDocDatabase::qdocDB()->primaryTreeRoot());
}

void CppCodeParser::createExampleFileNodes(DocumentNode *dn)
{
    QString examplePath = dn->name();
    QString proFileName = examplePath + QLatin1Char('/') + examplePath.split(QLatin1Char('/')).last() + ".pro";
    QString userFriendlyFilePath;

    QString fullPath = Config::findFile(dn->doc().location(),
                                        exampleFiles,
                                        exampleDirs,
                                        proFileName,
                                        userFriendlyFilePath);

    if (fullPath.isEmpty()) {
        QString tmp = proFileName;
        proFileName = examplePath + QLatin1Char('/') + "qbuild.pro";
        userFriendlyFilePath.clear();
        fullPath = Config::findFile(dn->doc().location(),
                                    exampleFiles,
                                    exampleDirs,
                                    proFileName,
                                    userFriendlyFilePath);
        if (fullPath.isEmpty()) {
            proFileName = examplePath + QLatin1Char('/') + examplePath.split(QLatin1Char('/')).last() + ".qmlproject";
            userFriendlyFilePath.clear();
            fullPath = Config::findFile(dn->doc().location(),
                                        exampleFiles,
                                        exampleDirs,
                                        proFileName,
                                        userFriendlyFilePath);
            if (fullPath.isEmpty()) {
                QString details = QLatin1String("Example directories: ") + exampleDirs.join(QLatin1Char(' '));
                if (!exampleFiles.isEmpty())
                    details += QLatin1String(", example files: ") + exampleFiles.join(QLatin1Char(' '));
                dn->location().warning(tr("Cannot find file '%1' or '%2'").arg(tmp).arg(proFileName), details);
                dn->location().warning(tr("  EXAMPLE PATH DOES NOT EXIST: %1").arg(examplePath), details);
                return;
            }
        }
    }

    int sizeOfBoringPartOfName = fullPath.size() - proFileName.size();
    if (fullPath.startsWith("./"))
        sizeOfBoringPartOfName = sizeOfBoringPartOfName - 2;
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

    foreach (const QString &exampleFile, exampleFiles) {
        new DocumentNode(dn,
                    exampleFile.mid(sizeOfBoringPartOfName),
                    Node::File,
                    Node::NoPageType);
    }
    foreach (const QString &imageFile, imageFiles) {
        new DocumentNode(dn,
                    imageFile.mid(sizeOfBoringPartOfName),
                    Node::Image,
                    Node::NoPageType);
    }
}

QT_END_NAMESPACE
