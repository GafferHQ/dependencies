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

#include "rendertargetselectornode_p.h"
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DCore/private/qchangearbiter_p.h>
#include <Qt3DRender/qrendertargetselector.h>
#include <Qt3DRender/qrendertarget.h>
#include <Qt3DCore/qscenepropertychange.h>
#include <Qt3DRender/private/renderlogging_p.h>
#include <Qt3DRender/qrenderattachment.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

RenderTargetSelector::RenderTargetSelector() :
    FrameGraphNode(FrameGraphNode::RenderTarget)
{
}

void RenderTargetSelector::updateFromPeer(Qt3DCore::QNode *peer)
{
    QRenderTargetSelector *selector = static_cast<QRenderTargetSelector *>(peer);
    m_renderTargetUuid = QNodeId();
    if (selector->target() != Q_NULLPTR)
        m_renderTargetUuid = selector->target()->id();
    setEnabled(selector->isEnabled());
    m_drawBuffers = selector->drawBuffers();
}

void RenderTargetSelector::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    qCDebug(Render::Framegraph) << Q_FUNC_INFO;
    if (e->type() == NodeUpdated) {
        QScenePropertyChangePtr propertyChange = qSharedPointerCast<QScenePropertyChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("target"))
            m_renderTargetUuid = propertyChange->value().value<QNodeId>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("enabled"))
            setEnabled(propertyChange->value().toBool());
        else if (propertyChange->propertyName() == QByteArrayLiteral("drawBuffers"))
            m_drawBuffers = propertyChange->value().value<QList<Qt3DRender::QRenderAttachment::RenderAttachmentType> >();
    }
}

Qt3DCore::QNodeId RenderTargetSelector::renderTargetUuid() const
{
    return m_renderTargetUuid;
}

QList<QRenderAttachment::RenderAttachmentType> RenderTargetSelector::drawBuffers() const
{
    return m_drawBuffers;
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
