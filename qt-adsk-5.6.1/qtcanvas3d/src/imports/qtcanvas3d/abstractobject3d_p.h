/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtCanvas3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef ABSTRACTOBJECT3D_P_H
#define ABSTRACTOBJECT3D_P_H

#include "canvas3dcommon_p.h"
#include "glcommandqueue_p.h"
#include <QObject>
#include <QThread>

QT_BEGIN_NAMESPACE
QT_CANVAS3D_BEGIN_NAMESPACE

class CanvasAbstractObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool invalidated READ invalidated NOTIFY invalidatedChanged)

public:
    explicit CanvasAbstractObject(CanvasGlCommandQueue *queue, QObject *parent);
    virtual ~CanvasAbstractObject();

    void setName(const QString &name);
    const QString &name() const;
    bool hasSpecificName() const;
    bool invalidated() const;
    void setInvalidated(bool invalidated); // Internal

signals:
    void nameChanged(const QString &name);
    void invalidatedChanged(bool invalidated);

protected:
    void queueCommand(CanvasGlCommandQueue::GlCommandId id, GLint p1, GLint p2 = 0);
    void queueCommand(CanvasGlCommandQueue::GlCommandId id, QByteArray *data,
                      GLint p1, GLint p2 = 0);
    CanvasGlCommandQueue *commandQueue() const { return m_commandQueue; }

private:
    QString m_name;
    bool m_hasName;
    bool m_invalidated;
    // Not owned. Can be null pointer if the object type doesn't need OpenGL commands
    CanvasGlCommandQueue *m_commandQueue;
};

QT_CANVAS3D_END_NAMESPACE
QT_END_NAMESPACE

#endif // ABSTRACTOBJECT3D_P_H
