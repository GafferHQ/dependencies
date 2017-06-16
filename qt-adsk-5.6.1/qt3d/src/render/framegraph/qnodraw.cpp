/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "qnodraw.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
 * \class Qt3DRender::QNoDraw
 * \inmodule Qt3DRender
 *
 * \brief When a QNoDraw node is present in a FrameGraph branch, this
 * prevents the renderer from rendering any primitive.
 *
 * QNoDraw should be used when the FrameGraph needs to set up some render
 * states or clear some buffers without requiring any mesh to be drawn. It has
 * the same effect as having a Qt3DRender::QRenderPassFilter that matches none of
 * available Qt3DRender::QRenderPass instances of the scene without the overhead cost
 * of actually performing the filtering.
 *
 * \since 5.5
 */

QNoDraw::QNoDraw(QNode *parent)
    : QFrameGraphNode(parent)
{
}

QNoDraw::~QNoDraw()
{
    QNode::cleanup();
}

} // namespace Qt3DRender

QT_END_NAMESPACE
