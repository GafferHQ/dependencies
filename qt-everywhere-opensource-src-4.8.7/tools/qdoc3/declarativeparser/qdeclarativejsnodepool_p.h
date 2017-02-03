/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#ifndef QDECLARATIVEJSNODEPOOL_P_H
#define QDECLARATIVEJSNODEPOOL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qdeclarativejsglobal_p.h"
#include "qdeclarativejsmemorypool_p.h"

#include <QtCore/QHash>
#include <QtCore/QString>

QT_QML_BEGIN_NAMESPACE

namespace QDeclarativeJS {

namespace AST {
class Node;
} // namespace AST

class Code;
class CompilationUnit;
class Engine;

template <typename NodeType>
inline NodeType *makeAstNode(MemoryPool *storage)
{
    NodeType *node = new (storage->allocate(sizeof(NodeType))) NodeType();
    return node;
}

template <typename NodeType, typename Arg1>
inline NodeType *makeAstNode(MemoryPool *storage, Arg1 arg1)
{
    NodeType *node = new (storage->allocate(sizeof(NodeType))) NodeType(arg1);
    return node;
}

template <typename NodeType, typename Arg1, typename Arg2>
inline NodeType *makeAstNode(MemoryPool *storage, Arg1 arg1, Arg2 arg2)
{
    NodeType *node = new (storage->allocate(sizeof(NodeType))) NodeType(arg1, arg2);
    return node;
}

template <typename NodeType, typename Arg1, typename Arg2, typename Arg3>
inline NodeType *makeAstNode(MemoryPool *storage, Arg1 arg1, Arg2 arg2, Arg3 arg3)
{
    NodeType *node = new (storage->allocate(sizeof(NodeType))) NodeType(arg1, arg2, arg3);
    return node;
}

template <typename NodeType, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
inline NodeType *makeAstNode(MemoryPool *storage, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
{
    NodeType *node = new (storage->allocate(sizeof(NodeType))) NodeType(arg1, arg2, arg3, arg4);
    return node;
}

class QML_PARSER_EXPORT NodePool : public MemoryPool
{
public:
    NodePool(const QString &fileName, Engine *engine);
    virtual ~NodePool();

    Code *createCompiledCode(AST::Node *node, CompilationUnit &compilation);

    inline QString fileName() const { return m_fileName; }
    inline Engine *engine() const { return m_engine; }
#ifndef J_SCRIPT_NO_EVENT_NOTIFY
    inline qint64 id() const { return m_id; }
#endif

private:
    QHash<AST::Node*, Code*> m_codeCache;
    QString m_fileName;
    Engine *m_engine;
#ifndef J_SCRIPT_NO_EVENT_NOTIFY
    qint64 m_id;
#endif

private:
    Q_DISABLE_COPY(NodePool)
};

} // namespace QDeclarativeJS

QT_QML_END_NAMESPACE

#endif
