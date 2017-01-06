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
  pagegenerator.h
*/

#ifndef PAGEGENERATOR_H
#define PAGEGENERATOR_H

#include <QStack>
#include <qtextstream.h>
#include "generator.h"
#include "location.h"

QT_BEGIN_NAMESPACE

class QTextCodec;
class ClassNode;
class InnerNode;
class NamespaceNode;

class PageGenerator : public Generator
{
 public:
    PageGenerator();
    ~PageGenerator();

    virtual void generateTree(const Tree *tree);

 protected:
    virtual QString fileBase(const Node* node) const;
    virtual QString fileExtension(const Node* node) const = 0;
    QString fileName(const Node* node) const;
    QString outFileName();
    virtual void beginSubPage(const Location& location, const QString& fileName);
    virtual void endSubPage();
    virtual void generateInnerNode(const InnerNode *node);
    QTextStream& out();

    QString naturalLanguage;
    QString outputEncoding;
    QTextCodec* outputCodec;
    bool parseArg(const QString& src,
                  const QString& tag,
                  int* pos,
                  int n,
                  QStringRef* contents,
                  QStringRef* par1 = 0,
                  bool debug = false);

 protected:
    QStack<QTextStream*> outStreamStack;
};

QT_END_NAMESPACE

#endif
