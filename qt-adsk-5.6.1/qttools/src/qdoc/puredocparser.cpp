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
  puredocparser.cpp
*/

#include <qfile.h>
#include <stdio.h>
#include <errno.h>
#include "codechunk.h"
#include "config.h"
#include "tokenizer.h"
#include <qdebug.h>
#include "qdocdatabase.h"
#include "puredocparser.h"

QT_BEGIN_NAMESPACE

/*!
  Constructs the pure doc parser.
*/
PureDocParser::PureDocParser()
{
}

/*!
  Destroys the pure doc parser.
 */
PureDocParser::~PureDocParser()
{
}

/*!
  Returns a list of the kinds of files that the pure doc
  parser is meant to parse. The elements of the list are
  file suffixes.
 */
QStringList PureDocParser::sourceFileNameFilter()
{
    return QStringList() << "*.qdoc" << "*.qtx" << "*.qtt" << "*.js";
}

/*!
  Parses the source file identified by \a filePath and adds its
  parsed contents to the database. The \a location is used for
  reporting errors.
 */
void PureDocParser::parseSourceFile(const Location& location, const QString& filePath)
{
    QFile in(filePath);
    currentFile_ = filePath;
    if (!in.open(QIODevice::ReadOnly)) {
        location.error(tr("Can't open source file '%1' (%2)").arg(filePath).arg(strerror(errno)));
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

    processQdocComments();
    in.close();
    currentFile_.clear();
}

/*!
  This is called by parseSourceFile() to do the actual parsing
  and tree building. It only processes qdoc comments. It skips
  everything else.
 */
bool PureDocParser::processQdocComments()
{
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
            Doc doc(start_loc, end_loc, comment, metacommandsAllowed, topicCommandsAllowed);

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
                doc.location().warning(tr("This qdoc comment contains no topic command "
                                          "(e.g., '\\%1', '\\%2').")
                                       .arg(COMMAND_MODULE).arg(COMMAND_PAGE));
            }
            else if (isQmlPropertyTopic || isJsPropertyTopic) {
                Doc nodeDoc = doc;
                processQmlProperties(nodeDoc, nodes, docs, isJsPropertyTopic);
            }
            else {
                ArgList args;
                QSet<QString> topicCommandsUsed = topicCommandsAllowed & doc.metaCommandsUsed();
                if (topicCommandsUsed.count() > 0) {
                    topic = *topicCommandsUsed.begin();
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
                ArgList::ConstIterator a = args.cbegin();
                while (a != args.cend()) {
                    Doc nodeDoc = doc;
                    Node* node = processTopicCommand(nodeDoc,topic,*a);
                    if (node != 0) {
                        nodes.append(node);
                        docs.append(nodeDoc);
                    }
                    ++a;
                }
            }

            Node* treeRoot = QDocDatabase::qdocDB()->primaryTreeRoot();
            NodeList::Iterator n = nodes.begin();
            QList<Doc>::Iterator d = docs.begin();
            while (n != nodes.end()) {
                processOtherMetaCommands(*d, *n);
                (*n)->setDoc(*d);
                checkModuleInclusion(*n);
                if ((*n)->isAggregate() && ((Aggregate *)*n)->includes().isEmpty()) {
                    Aggregate *m = static_cast<Aggregate *>(*n);
                    while (m->parent() && m->parent() != treeRoot)
                        m = m->parent();
                    if (m == *n)
                        ((Aggregate *)*n)->addInclude((*n)->name());
                    else
                        ((Aggregate *)*n)->setIncludes(m->includes());
                }
                ++d;
                ++n;
            }
        }
        else {
            readToken();
        }
    }
    return true;
}


QT_END_NAMESPACE
