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

#ifndef CPPCODEPARSER_H
#define CPPCODEPARSER_H

#include <qregexp.h>

#include "codeparser.h"

QT_BEGIN_NAMESPACE

class ClassNode;
class CodeChunk;
class CppCodeParserPrivate;
class FunctionNode;
class Aggregate;
class Tokenizer;

class CppCodeParser : public CodeParser
{
    Q_DECLARE_TR_FUNCTIONS(QDoc::CppCodeParser)

    struct ExtraFuncData {
        Aggregate* root; // Used as the parent.
        Node::NodeType type; // The node type: Function, etc.
        bool isAttached; // If true, the method is attached.
        bool isMacro;    // If true, we are parsing a macro signature.
        ExtraFuncData() : root(0), type(Node::Function), isAttached(false), isMacro(false) { }
        ExtraFuncData(Aggregate* r, Node::NodeType t, bool a)
          : root(r), type(t), isAttached(a), isMacro(false) { }
    };

public:
    CppCodeParser();
    ~CppCodeParser();
    static CppCodeParser* cppParser() { return cppParser_; }

    virtual void initializeParser(const Config& config) Q_DECL_OVERRIDE;
    virtual void terminateParser() Q_DECL_OVERRIDE;
    virtual QString language() Q_DECL_OVERRIDE;
    virtual QStringList headerFileNameFilter() Q_DECL_OVERRIDE;
    virtual QStringList sourceFileNameFilter() Q_DECL_OVERRIDE;
    virtual void parseHeaderFile(const Location& location, const QString& filePath) Q_DECL_OVERRIDE;
    virtual void parseSourceFile(const Location& location, const QString& filePath) Q_DECL_OVERRIDE;
    virtual void doneParsingHeaderFiles() Q_DECL_OVERRIDE;
    virtual void doneParsingSourceFiles() Q_DECL_OVERRIDE;
    bool parseParameters(const QString& parameters, QVector<Parameter>& pvect, bool& isQPrivateSignal);
    const Location& declLoc() const { return declLoc_; }
    void setDeclLoc() { declLoc_ = location(); }

protected:
    const QSet<QString>& topicCommands();
    const QSet<QString>& otherMetaCommands();
    virtual Node* processTopicCommand(const Doc& doc,
                                      const QString& command,
                                      const ArgLocPair& arg);
    void processQmlProperties(const Doc& doc, NodeList& nodes, DocList& docs, bool jsProps);
    bool splitQmlPropertyGroupArg(const QString& arg,
                                  QString& module,
                                  QString& element,
                                  QString& name);
    bool splitQmlPropertyArg(const QString& arg,
                             QString& type,
                             QString& module,
                             QString& element,
                             QString& name);
    bool splitQmlMethodArg(const QString& arg,
                           QString& type,
                           QString& module,
                           QString& element);
    virtual void processOtherMetaCommand(const Doc& doc,
                                         const QString& command,
                                         const ArgLocPair& argLocPair,
                                         Node *node);
    void processOtherMetaCommands(const Doc& doc, Node *node);

 protected:
    void reset();
    void readToken();
    const Location& location();
    QString previousLexeme();
    QString lexeme();

 private:
    bool match(int target);
    bool skipTo(int target);
    bool matchCompat();
    bool matchModuleQualifier(QString& name);
    bool matchTemplateAngles(CodeChunk *type = 0);
    bool matchTemplateHeader();
    bool matchDataType(CodeChunk *type, QString *var = 0, bool qProp = false);
    bool matchParameter(QVector<Parameter>& pvect, bool& isQPrivateSignal);
    bool matchFunctionDecl(Aggregate *parent,
                           QStringList *parentPathPtr,
                           FunctionNode **funcPtr,
                           const QString &templateStuff,
                           ExtraFuncData& extra);
    bool matchBaseSpecifier(ClassNode *classe, bool isClass);
    bool matchBaseList(ClassNode *classe, bool isClass);
    bool matchClassDecl(Aggregate *parent,
                        const QString &templateStuff = QString());
    bool matchNamespaceDecl(Aggregate *parent);
    bool matchUsingDecl(Aggregate* parent);
    bool matchEnumItem(Aggregate *parent, EnumNode *enume);
    bool matchEnumDecl(Aggregate *parent);
    bool matchTypedefDecl(Aggregate *parent);
    bool matchProperty(Aggregate *parent);
    bool matchDeclList(Aggregate *parent);
    bool matchDocsAndStuff();
    bool makeFunctionNode(const QString &synopsis,
                          QStringList *parentPathPtr,
                          FunctionNode **funcPtr,
                          ExtraFuncData& params);
    FunctionNode* makeFunctionNode(const Doc& doc,
                                   const QString& sig,
                                   Aggregate* parent,
                                   Node::NodeType type,
                                   bool attached,
                                   QString qdoctag);
    void parseQiteratorDotH(const Location &location, const QString &filePath);
    void instantiateIteratorMacro(const QString &container,
                                  const QString &includeFile,
                                  const QString &macroDef);
    void createExampleFileNodes(DocumentNode *dn);
    int matchFunctionModifier();

 protected:
    QMap<QString, Node::NodeType> nodeTypeMap;
    Tokenizer *tokenizer;
    int tok;
    Node::Access access;
    FunctionNode::Metaness metaness_;
    QString physicalModuleName;
    QStringList lastPath_;
    QRegExp varComment;
    QRegExp sep;
    Location declLoc_;

 private:
    QString sequentialIteratorDefinition;
    QString mutableSequentialIteratorDefinition;
    QString associativeIteratorDefinition;
    QString mutableAssociativeIteratorDefinition;
    QMap<QString, QString> sequentialIteratorClasses;
    QMap<QString, QString> mutableSequentialIteratorClasses;
    QMap<QString, QString> associativeIteratorClasses;
    QMap<QString, QString> mutableAssociativeIteratorClasses;

    static QStringList exampleFiles;
    static QStringList exampleDirs;
    static CppCodeParser* cppParser_;
    QString exampleNameFilter;
    QString exampleImageFilter;
};

#define COMMAND_ABSTRACT                Doc::alias("abstract")
#define COMMAND_CLASS                   Doc::alias("class")
#define COMMAND_CONTENTSPAGE            Doc::alias("contentspage")
#define COMMAND_DITAMAP                 Doc::alias("ditamap")
#define COMMAND_ENUM                    Doc::alias("enum")
#define COMMAND_EXAMPLE                 Doc::alias("example")
#define COMMAND_EXTERNALPAGE            Doc::alias("externalpage")
#define COMMAND_FILE                    Doc::alias("file")
#define COMMAND_FN                      Doc::alias("fn")
#define COMMAND_GROUP                   Doc::alias("group")
#define COMMAND_HEADERFILE              Doc::alias("headerfile")
#define COMMAND_INDEXPAGE               Doc::alias("indexpage")
#define COMMAND_INHEADERFILE            Doc::alias("inheaderfile")
#define COMMAND_MACRO                   Doc::alias("macro")
#define COMMAND_MODULE                  Doc::alias("module")
#define COMMAND_NAMESPACE               Doc::alias("namespace")
#define COMMAND_OVERLOAD                Doc::alias("overload")
#define COMMAND_NEXTPAGE                Doc::alias("nextpage")
#define COMMAND_PAGE                    Doc::alias("page")
#define COMMAND_PREVIOUSPAGE            Doc::alias("previouspage")
#define COMMAND_PROPERTY                Doc::alias("property")
#define COMMAND_REIMP                   Doc::alias("reimp")
#define COMMAND_RELATES                 Doc::alias("relates")
#define COMMAND_STARTPAGE               Doc::alias("startpage")
#define COMMAND_TYPEDEF                 Doc::alias("typedef")
#define COMMAND_VARIABLE                Doc::alias("variable")
#define COMMAND_QMLABSTRACT             Doc::alias("qmlabstract")
#define COMMAND_QMLTYPE                 Doc::alias("qmltype")
#define COMMAND_QMLPROPERTY             Doc::alias("qmlproperty")
#define COMMAND_QMLPROPERTYGROUP        Doc::alias("qmlpropertygroup")
#define COMMAND_QMLATTACHEDPROPERTY     Doc::alias("qmlattachedproperty")
#define COMMAND_QMLINHERITS             Doc::alias("inherits")
#define COMMAND_QMLINSTANTIATES         Doc::alias("instantiates")
#define COMMAND_QMLSIGNAL               Doc::alias("qmlsignal")
#define COMMAND_QMLATTACHEDSIGNAL       Doc::alias("qmlattachedsignal")
#define COMMAND_QMLMETHOD               Doc::alias("qmlmethod")
#define COMMAND_QMLATTACHEDMETHOD       Doc::alias("qmlattachedmethod")
#define COMMAND_QMLDEFAULT              Doc::alias("default")
#define COMMAND_QMLREADONLY             Doc::alias("readonly")
#define COMMAND_QMLBASICTYPE            Doc::alias("qmlbasictype")
#define COMMAND_QMLMODULE               Doc::alias("qmlmodule")
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
#define COMMAND_QTVARIABLE              Doc::alias("qtvariable")
// Some of these are not used currenmtly, but they are included now for completeness.
#define COMMAND_JSTYPE                 Doc::alias("jstype")
#define COMMAND_JSPROPERTY             Doc::alias("jsproperty")
#define COMMAND_JSPROPERTYGROUP        Doc::alias("jspropertygroup")
#define COMMAND_JSATTACHEDPROPERTY     Doc::alias("jsattachedproperty")
#define COMMAND_JSSIGNAL               Doc::alias("jssignal")
#define COMMAND_JSATTACHEDSIGNAL       Doc::alias("jsattachedsignal")
#define COMMAND_JSMETHOD               Doc::alias("jsmethod")
#define COMMAND_JSATTACHEDMETHOD       Doc::alias("jsattachedmethod")
#define COMMAND_JSBASICTYPE            Doc::alias("jsbasictype")
#define COMMAND_JSMODULE               Doc::alias("jsmodule")

QT_END_NAMESPACE

#endif
