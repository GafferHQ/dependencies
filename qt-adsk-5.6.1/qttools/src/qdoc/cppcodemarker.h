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
  cppcodemarker.h
*/

#ifndef CPPCODEMARKER_H
#define CPPCODEMARKER_H

#include "codemarker.h"

QT_BEGIN_NAMESPACE

class CppCodeMarker : public CodeMarker
{
    Q_DECLARE_TR_FUNCTIONS(QDoc::CppCodeMarker)

public:
    CppCodeMarker();
    ~CppCodeMarker();

    virtual bool recognizeCode(const QString& code) Q_DECL_OVERRIDE;
    virtual bool recognizeExtension(const QString& ext) Q_DECL_OVERRIDE;
    virtual bool recognizeLanguage(const QString& lang) Q_DECL_OVERRIDE;
    virtual Atom::AtomType atomType() const Q_DECL_OVERRIDE;
    virtual QString markedUpCode(const QString& code,
                                 const Node *relative,
                                 const Location &location) Q_DECL_OVERRIDE;
    virtual QString markedUpSynopsis(const Node *node,
                                     const Node *relative,
                                     SynopsisStyle style) Q_DECL_OVERRIDE;
    virtual QString markedUpQmlItem(const Node *node, bool summary) Q_DECL_OVERRIDE;
    virtual QString markedUpName(const Node *node) Q_DECL_OVERRIDE;
    virtual QString markedUpFullName(const Node *node, const Node *relative) Q_DECL_OVERRIDE;
    virtual QString markedUpEnumValue(const QString &enumValue, const Node *relative) Q_DECL_OVERRIDE;
    virtual QString markedUpIncludes(const QStringList& includes) Q_DECL_OVERRIDE;
    virtual QString functionBeginRegExp(const QString& funcName) Q_DECL_OVERRIDE;
    virtual QString functionEndRegExp(const QString& funcName) Q_DECL_OVERRIDE;
    virtual QList<Section> sections(const Aggregate *innerNode,
                                    SynopsisStyle style,
                                    Status status) Q_DECL_OVERRIDE;
    virtual QList<Section> qmlSections(Aggregate* aggregate,
                                       SynopsisStyle style,
                                       Status status = Okay) Q_DECL_OVERRIDE;

private:
    QString addMarkUp(const QString& protectedCode,
                      const Node *relative,
                      const Location &location);
};

QT_END_NAMESPACE

#endif
