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

#include "sortmethod_p.h"
#include <Qt3DRender/qsortcriterion.h>
#include <Qt3DCore/qscenepropertychange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

SortMethod::SortMethod()
    : FrameGraphNode(FrameGraphNode::SortMethod)
{
}

void SortMethod::updateFromPeer(Qt3DCore::QNode *peer)
{
    QSortMethod *sortMethod = static_cast<QSortMethod *>(peer);
    m_criteria.clear();
    Q_FOREACH (QSortCriterion *c, sortMethod->criteria())
        m_criteria.append(c->id());
    setEnabled(sortMethod->isEnabled());
}

void SortMethod::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    QScenePropertyChangePtr propertyChange = qSharedPointerCast<QScenePropertyChange>(e);
    if (propertyChange->propertyName() == QByteArrayLiteral("sortCriterion")) {
        const QNodeId cId = propertyChange->value().value<QNodeId>();
        if (!cId.isNull()) {
            if (e->type() == NodeAdded)
                m_criteria.append(cId);
            else if (e->type() == NodeRemoved)
                m_criteria.removeAll(cId);
        }
    } else if (propertyChange->propertyName() == QByteArrayLiteral("enabled") && e->type() == NodeUpdated) {
        setEnabled(propertyChange->value().toBool());
    }
}

QList<QNodeId> SortMethod::criteria() const
{
    return m_criteria;
}

} // namepace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
