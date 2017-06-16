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

#ifndef CODEPARSER_H
#define CODEPARSER_H

#include <qset.h>
#include "node.h"

QT_BEGIN_NAMESPACE

class Config;
class QString;
class QDocDatabase;

class CodeParser
{
    Q_DECLARE_TR_FUNCTIONS(QDoc::CodeParser)

public:
    CodeParser();
    virtual ~CodeParser();

    virtual void initializeParser(const Config& config);
    virtual void terminateParser();
    virtual QString language() = 0;
    virtual QStringList headerFileNameFilter();
    virtual QStringList sourceFileNameFilter() = 0;
    virtual void parseHeaderFile(const Location& location, const QString& filePath);
    virtual void parseSourceFile(const Location& location, const QString& filePath) = 0;
    virtual void doneParsingHeaderFiles();
    virtual void doneParsingSourceFiles() = 0;

    bool isParsingH() const;
    bool isParsingCpp() const;
    bool isParsingQdoc() const;
    const QString& currentFile() const { return currentFile_; }
    void checkModuleInclusion(Node* n);

    static void initialize(const Config& config);
    static void terminate();
    static CodeParser *parserForLanguage(const QString& language);
    static CodeParser *parserForHeaderFile(const QString &filePath);
    static CodeParser *parserForSourceFile(const QString &filePath);
    static void setLink(Node* node, Node::LinkType linkType, const QString& arg);

protected:
    const QSet<QString>& commonMetaCommands();
    void processCommonMetaCommand(const Location& location,
                                  const QString& command,
                                  const ArgLocPair& arg,
                                  Node *node);
    static void extractPageLinkAndDesc(const QString& arg,
                                       QString* link,
                                       QString* desc);
    QString currentFile_;
    QDocDatabase* qdb_;

private:
    static QList<CodeParser *> parsers;
    static bool showInternal_;
    static bool singleExec_;
};

QT_END_NAMESPACE

#endif
