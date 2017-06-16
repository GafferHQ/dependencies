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

#include "uniformbuffer_p.h"
#include <private/graphicscontext_p.h>

#if !defined(GL_UNIFORM_BUFFER)
#define GL_UNIFORM_BUFFER 0x8A11
#endif

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

// A UBO is created for each ShaderData Shader Pair
// That means a UBO is unique to a shader/shaderdata

UniformBuffer::UniformBuffer()
    : m_bufferId(~0)
    , m_isCreated(false)
    , m_bound(false)
{
}

void UniformBuffer::bind(GraphicsContext *ctx)
{
    ctx->openGLContext()->functions()->glBindBuffer(GL_UNIFORM_BUFFER, m_bufferId);
    m_bound = true;
}

void UniformBuffer::release(GraphicsContext *ctx)
{
    m_bound = false;
    ctx->openGLContext()->functions()->glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::create(GraphicsContext *ctx)
{
    ctx->openGLContext()->functions()->glGenBuffers(1, &m_bufferId);
    m_isCreated = true;
}

void UniformBuffer::destroy(GraphicsContext *ctx)
{
    ctx->openGLContext()->functions()->glDeleteBuffers(1, &m_bufferId);
    m_isCreated = false;
}

void UniformBuffer::allocate(GraphicsContext *ctx, uint size, bool dynamic)
{
    // Either GL_STATIC_DRAW OR GL_DYNAMIC_DRAW depending on  the use case
    // TO DO: find a way to know how a buffer/QShaderData will be used to use the right usage
    ctx->openGLContext()->functions()->glBufferData(GL_UNIFORM_BUFFER, size, NULL, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
}

void UniformBuffer::update(GraphicsContext *ctx, const void *data, uint size, int offset)
{
    ctx->openGLContext()->functions()->glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
}

void UniformBuffer::bindToUniformBlock(GraphicsContext *ctx, int bindingPoint)
{
    ctx->bindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_bufferId);
}

} // namespace Render

} // namespace Qt3DRender

QT_END_NAMESPACE
