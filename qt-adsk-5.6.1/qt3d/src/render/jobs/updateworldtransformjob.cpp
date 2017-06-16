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

#include "updateworldtransformjob_p.h"

#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/entity_p.h>
#include <Qt3DRender/private/sphere_p.h>
#include <Qt3DRender/private/transform_p.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <QThread>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

namespace {

void updateWorldTransformAndBounds(Qt3DRender::Render::Entity *node, const QMatrix4x4 &parentTransform)
{
    QMatrix4x4 worldTransform(parentTransform);
    Transform *nodeTransform = node->renderComponent<Transform>();
    if (nodeTransform != Q_NULLPTR && nodeTransform->isEnabled())
        worldTransform = worldTransform * nodeTransform->transformMatrix();

    *(node->worldTransform()) = worldTransform;
    *(node->worldBoundingVolume()) = node->localBoundingVolume()->transformed(worldTransform);
    *(node->worldBoundingVolumeWithChildren()) = *(node->worldBoundingVolume()); // expanded in UpdateBoundingVolumeJob

    Q_FOREACH (Qt3DRender::Render::Entity *child, node->children())
        updateWorldTransformAndBounds(child, worldTransform);
}

}

UpdateWorldTransformJob::UpdateWorldTransformJob()
    : Qt3DCore::QAspectJob()
    , m_node(Q_NULLPTR)
{
}

void UpdateWorldTransformJob::setRoot(Entity *root)
{
    m_node = root;
}

void UpdateWorldTransformJob::run()
{
    // Iterate over each level of hierarchy in our scene
    // and update each node's world transform from its
    // local transform and its parent's world transform

    // TODO: Parallelise this on each level using a parallel_for
    // implementation.

    qCDebug(Jobs) << "Entering" << Q_FUNC_INFO << QThread::currentThread();

    QMatrix4x4 parentTransform;
    Entity *parent = m_node->parent();
    if (parent != Q_NULLPTR)
        parentTransform = *(parent->worldTransform());
    updateWorldTransformAndBounds(m_node, parentTransform);

    qCDebug(Jobs) << "Exiting" << Q_FUNC_INFO << QThread::currentThread();
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
