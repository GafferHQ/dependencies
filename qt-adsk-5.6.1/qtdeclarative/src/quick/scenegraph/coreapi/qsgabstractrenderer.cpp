/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgabstractrenderer_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QSGAbstractRenderer
    \brief QSGAbstractRenderer gives access to the scene graph nodes and rendering of a QSGEngine.
    \inmodule QtQuick
    \since 5.4

    A QSGAbstractRenderer created by a QSGEngine allows you to set your QSGNode
    tree through setRootNode() and control the rendering viewport through
    setDeviceRect(), setViewportRect() and setProjectionMatrixToRect().
    You can finally trigger the rendering to the desired framebuffer through
    renderScene().

    The QSGAbstractRenderer is only available when used with a QSGEngine
    and isn't exposed when used internally by QQuickWindow.

    \sa QSGEngine, QSGNode
 */

/*!
    \enum QSGAbstractRenderer::ClearModeBit

    Used with setClearMode() to indicate which buffer should
    be cleared before the scene render.

    \value ClearColorBuffer Clear the color buffer using clearColor().
    \value ClearDepthBuffer Clear the depth buffer.
    \value ClearStencilBuffer Clear the stencil buffer.

    \sa setClearMode(), setClearColor()
 */

/*!
    \fn void QSGAbstractRenderer::renderScene(GLuint fboId = 0)

    Render the scene to the specified \a fboId

    If \a fboId isn't specified, the scene graph will be rendered
    to the default framebuffer. You will have to call
    QOpenGLContext::swapBuffers() yourself afterward.

    The framebuffer specified by \a fboId will be bound automatically.

    \sa QOpenGLContext::swapBuffers(), QOpenGLFramebufferObject::handle()
 */

/*!
    \fn void QSGAbstractRenderer::sceneGraphChanged()

    This signal is emitted on the first modification of a node in
    the tree after the last scene render.
 */

/*!
    \internal
 */
QSGAbstractRendererPrivate::QSGAbstractRendererPrivate()
    : m_root_node(0)
    , m_clear_color(Qt::transparent)
    , m_clear_mode(QSGAbstractRenderer::ClearColorBuffer | QSGAbstractRenderer::ClearDepthBuffer)
{
}

/*!
    \internal
 */
QSGAbstractRenderer::QSGAbstractRenderer(QObject *parent)
    : QObject(*new QSGAbstractRendererPrivate, parent)
{
}

/*!
    \internal
 */
QSGAbstractRenderer::~QSGAbstractRenderer()
{
}

/*!
    Sets the \a node as the root of the QSGNode scene
    that you want to render. You need to provide a \a node
    before trying to render the scene.

    \note This doesn't take ownership of \a node.

    \sa rootNode()
*/
void QSGAbstractRenderer::setRootNode(QSGRootNode *node)
{
    Q_D(QSGAbstractRenderer);
    if (d->m_root_node == node)
        return;
    if (d->m_root_node) {
        d->m_root_node->m_renderers.removeOne(this);
        nodeChanged(d->m_root_node, QSGNode::DirtyNodeRemoved);
    }
    d->m_root_node = node;
    if (d->m_root_node) {
        Q_ASSERT(!d->m_root_node->m_renderers.contains(this));
        d->m_root_node->m_renderers << this;
        nodeChanged(d->m_root_node, QSGNode::DirtyNodeAdded);
    }
}

/*!
    Returns the root of the QSGNode scene.

    \sa setRootNode()
*/
QSGRootNode *QSGAbstractRenderer::rootNode() const
{
    Q_D(const QSGAbstractRenderer);
    return d->m_root_node;
}


/*!
    \fn void QSGAbstractRenderer::setDeviceRect(const QSize &size)
    \overload

    Sets the \a size of the surface being rendered to.

    \sa deviceRect()
 */

/*!
    Sets \a rect as the geometry of the surface being rendered to.

    \sa deviceRect()
 */
void QSGAbstractRenderer::setDeviceRect(const QRect &rect)
{
    Q_D(QSGAbstractRenderer);
    d->m_device_rect = rect;
}

/*!
    Returns the device rect of the surface being rendered to.

    \sa setDeviceRect()
 */
QRect QSGAbstractRenderer::deviceRect() const
{
    Q_D(const QSGAbstractRenderer);
    return d->m_device_rect;
}

/*!
    \fn void QSGAbstractRenderer::setViewportRect(const QSize &size)
    \overload

    Sets the \a size of the viewport to render
    on the surface.

    \sa viewportRect()
 */

/*!
    Sets \a rect as the geometry of the viewport to render
    on the surface.

    \sa viewportRect()
 */
void QSGAbstractRenderer::setViewportRect(const QRect &rect)
{
    Q_D(QSGAbstractRenderer);
    d->m_viewport_rect = rect;
}

/*!
    Returns the rect of the viewport to render.

    \sa setViewportRect()
 */
QRect QSGAbstractRenderer::viewportRect() const
{
    Q_D(const QSGAbstractRenderer);
    return d->m_viewport_rect;
}

/*!
    Convenience method that calls setProjectionMatrix() with an
    orthographic matrix generated from \a rect.

    \sa setProjectionMatrix(), projectionMatrix()
 */
void QSGAbstractRenderer::setProjectionMatrixToRect(const QRectF &rect)
{
    QMatrix4x4 matrix;
    matrix.ortho(rect.x(),
                 rect.x() + rect.width(),
                 rect.y() + rect.height(),
                 rect.y(),
                 1,
                 -1);
    setProjectionMatrix(matrix);
}

/*!
    Use \a matrix to project the QSGNode coordinates onto surface pixels.

    \sa projectionMatrix(), setProjectionMatrixToRect()
 */
void QSGAbstractRenderer::setProjectionMatrix(const QMatrix4x4 &matrix)
{
    Q_D(QSGAbstractRenderer);
    d->m_projection_matrix = matrix;
}

/*!
    Returns the projection matrix

    \sa setProjectionMatrix(), setProjectionMatrixToRect()
 */
QMatrix4x4 QSGAbstractRenderer::projectionMatrix() const
{
    Q_D(const QSGAbstractRenderer);
    return d->m_projection_matrix;
}

/*!
    Use \a color to clear the framebuffer when clearMode() is
    set to QSGAbstractRenderer::ClearColorBuffer.

    \sa clearColor(), setClearMode()
 */
void QSGAbstractRenderer::setClearColor(const QColor &color)
{
    Q_D(QSGAbstractRenderer);
    d->m_clear_color = color;
}

/*!
    Returns the color that clears the framebuffer at the beginning
    of the rendering.

    \sa setClearColor(), clearMode()
 */
QColor QSGAbstractRenderer::clearColor() const
{
    Q_D(const QSGAbstractRenderer);
    return d->m_clear_color;
}

/*!
    Defines which attachment of the framebuffer should be cleared
    before each scene render with the \a mode flag.

    \sa clearMode(), setClearColor()
 */
void QSGAbstractRenderer::setClearMode(ClearMode mode)
{
    Q_D(QSGAbstractRenderer);
    d->m_clear_mode = mode;
}

/*!
    Flags defining which attachment of the framebuffer will be cleared
    before each scene render.

    \sa setClearMode(), clearColor()
 */
QSGAbstractRenderer::ClearMode QSGAbstractRenderer::clearMode() const
{
    Q_D(const QSGAbstractRenderer);
    return d->m_clear_mode;
}

/*!
    \fn void QSGAbstractRenderer::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
    \internal
 */

QT_END_NAMESPACE
