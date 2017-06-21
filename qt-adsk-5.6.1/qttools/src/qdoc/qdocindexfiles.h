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

#ifndef QDOCINDEXFILES_H
#define QDOCINDEXFILES_H

#include "node.h"
#include "tree.h"

QT_BEGIN_NAMESPACE

class Atom;
class Generator;
class QStringList;
class QDocDatabase;
class QXmlStreamWriter;
class QXmlStreamAttributes;

class QDocIndexFiles
{
    friend class QDocDatabase;

 private:
    static QDocIndexFiles* qdocIndexFiles();
    static void destroyQDocIndexFiles();

    QDocIndexFiles();
    ~QDocIndexFiles();

    void readIndexes(const QStringList& indexFiles);
    void generateIndex(const QString& fileName,
                       const QString& url,
                       const QString& title,
                       Generator* g,
                       bool generateInternalNodes = false);

    void readIndexFile(const QString& path);
    void readIndexSection(QXmlStreamReader &reader, Node* current, const QString& indexUrl);
    void insertTarget(TargetRec::TargetType type, const QXmlStreamAttributes &attributes, Node *node);
    void resolveIndex();
    void resolveRelates();
    bool generateIndexSection(QXmlStreamWriter& writer, Node* node, bool generateInternalNodes = false);
    void generateIndexSections(QXmlStreamWriter& writer, Node* node, bool generateInternalNodes = false);

 private:
    static QDocIndexFiles* qdocIndexFiles_;
    QDocDatabase* qdb_;
    Generator* gen_;
    QString project_;
    QVector<QPair<ClassNode*,QString> > basesList_;
    QVector<QPair<FunctionNode*,QString> > relatedList_;
};

QT_END_NAMESPACE

#endif
