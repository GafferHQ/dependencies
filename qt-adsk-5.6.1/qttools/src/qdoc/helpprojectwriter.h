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

#ifndef HELPPROJECTWRITER_H
#define HELPPROJECTWRITER_H

#include <qstring.h>
#include <qxmlstream.h>

#include "config.h"
#include "node.h"

QT_BEGIN_NAMESPACE

class QDocDatabase;
class Generator;
typedef QPair<QString, const Node*> QStringNodePair;

struct SubProject
{
    QString title;
    QString indexTitle;
    QHash<Node::NodeType, QSet<DocumentNode::DocSubtype> > selectors;
    bool sortPages;
    QString type;
    QHash<QString, const Node *> nodes;
    QStringList groups;
};

struct HelpProject
{
    QString name;
    QString helpNamespace;
    QString virtualFolder;
    QString fileName;
    QString indexRoot;
    QString indexTitle;
    QList<QStringList> keywords;
    QSet<QString> files;
    QSet<QString> extraFiles;
    QSet<QString> filterAttributes;
    QHash<QString, QSet<QString> > customFilters;
    QSet<QString> excluded;
    QList<SubProject> subprojects;
    QHash<const Node *, QSet<Node::Status> > memberStatus;
    bool includeIndexNodes;
};

class HelpProjectWriter
{
    Q_DECLARE_TR_FUNCTIONS(QDoc::HelpProjectWriter)

public:
    HelpProjectWriter(const Config &config,
                      const QString &defaultFileName,
                      Generator* g);
    void reset(const Config &config,
          const QString &defaultFileName,
          Generator* g);
    void addExtraFile(const QString &file);
    void addExtraFiles(const QSet<QString> &files);
    void generate();

private:
    void generateProject(HelpProject &project);
    void generateSections(HelpProject &project, QXmlStreamWriter &writer,
                          const Node *node);
    bool generateSection(HelpProject &project, QXmlStreamWriter &writer,
                         const Node *node);
    QStringList keywordDetails(const Node *node) const;
    void writeHashFile(QFile &file);
    void writeNode(HelpProject &project, QXmlStreamWriter &writer, const Node *node);
    void readSelectors(SubProject &subproject, const QStringList &selectors);
    void addMembers(HelpProject &project, QXmlStreamWriter &writer,
                           const Node *node);
    void writeSection(QXmlStreamWriter &writer, const QString &path,
                            const QString &value);

    QDocDatabase* qdb_;
    Generator* gen_;

    QString outputDir;
    QList<HelpProject> projects;
};

QT_END_NAMESPACE

#endif
