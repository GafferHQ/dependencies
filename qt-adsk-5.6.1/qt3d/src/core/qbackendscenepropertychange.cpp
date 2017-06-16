/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qbackendscenepropertychange.h"
#include "qbackendscenepropertychange_p.h"
#include <Qt3DCore/qbackendnode.h>
#include <Qt3DCore/private/qbackendnode_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

/*!
    \class Qt3DCore::QBackendScenePropertyChange
    \inmodule Qt3DCore
*/

QBackendScenePropertyChangePrivate::QBackendScenePropertyChangePrivate()
    : QScenePropertyChangePrivate()
{
}

QBackendScenePropertyChangePrivate::~QBackendScenePropertyChangePrivate()
{
}

QBackendScenePropertyChange::QBackendScenePropertyChange(ChangeFlag type, const QNodeId &subjectId, QSceneChange::Priority priority)
    : QScenePropertyChange(*new QBackendScenePropertyChangePrivate, type, Observable, subjectId, priority)
{
}

QBackendScenePropertyChange::~QBackendScenePropertyChange()
{
}

// TO DO get rid off setTargetNode, use the subject instead ??
void QBackendScenePropertyChange::setTargetNode(const QNodeId &id)
{
    Q_D(QBackendScenePropertyChange);
    d->m_targetUuid = id;
}

QNodeId QBackendScenePropertyChange::targetNode() const
{
    Q_D(const QBackendScenePropertyChange);
    return d->m_targetUuid;
}

/*!
    \typedef Qt3DCore::QBackendScenePropertyChangePtr
    \relates Qt3DCore::QBackendScenePropertyChange

    A shared pointer for QBackendScenePropertyChange.
*/

/*! \internal */
QBackendScenePropertyChange::QBackendScenePropertyChange(QBackendScenePropertyChangePrivate &dd)
    : QScenePropertyChange(dd)
{
}

/*! \internal */
QBackendScenePropertyChange::QBackendScenePropertyChange(QBackendScenePropertyChangePrivate &dd, ChangeFlag type, const QNodeId &subjectId, QSceneChange::Priority priority)
    : QScenePropertyChange(dd, type, Observable, subjectId, priority)
{
}

} // Qt3D

QT_END_NAMESPACE
