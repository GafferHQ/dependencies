/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qsurface.h"
#include "qopenglcontext.h"

QT_BEGIN_NAMESPACE


/*!
    \class QSurface
    \inmodule QtGui
    \since 5.0
    \brief The QSurface class is an abstraction of renderable surfaces in Qt.

    The size of the surface is accessible with the size() function. The rendering
    specific attributes of the surface are accessible through the format() function.
 */


/*!
    \enum QSurface::SurfaceClass

    The SurfaceClass enum describes the actual subclass of the surface.

    \value Window The surface is an instance of QWindow.
    \value Offscreen The surface is an instance of QOffscreenSurface.
 */


/*!
    \enum QSurface::SurfaceType

    The SurfaceType enum describes what type of surface this is.

    \value RasterSurface The surface is is composed of pixels and can be rendered to using
    a software rasterizer like Qt's raster paint engine.
    \value OpenGLSurface The surface is an OpenGL compatible surface and can be used
    in conjunction with QOpenGLContext.
    \value RasterGLSurface The surface can be rendered to using a software rasterizer,
    and also supports OpenGL. This surface type is intended for internal Qt use, and
    requires the use of private API.
 */


/*!
    \fn QSurfaceFormat QSurface::format() const

    Returns the format of the surface.
 */

/*!
  Returns true if the surface is OpenGL compatible and can be used in
  conjunction with QOpenGLContext; otherwise returns false.

  \since 5.3
*/

bool QSurface::supportsOpenGL() const
{
    SurfaceType type = surfaceType();
    return type == OpenGLSurface || type == RasterGLSurface;
}

/*!
    \fn QPlatformSurface *QSurface::surfaceHandle() const

    Returns a handle to the platform-specific implementation of the surface.
 */

/*!
    \fn SurfaceType QSurface::surfaceType() const

    Returns the type of the surface.
 */

/*!
    \fn QSize QSurface::size() const

    Returns the size of the surface in pixels.
 */

/*!
    Creates a surface with the given \a type.
*/
QSurface::QSurface(SurfaceClass type)
    : m_type(type), m_reserved(0)
{
}

/*!
    Destroys the surface.
*/
QSurface::~QSurface()
{
#ifndef QT_NO_OPENGL
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (context && context->surface() == this)
        context->doneCurrent();
#endif
}

/*!
   Returns the surface class of this surface.
 */
QSurface::SurfaceClass QSurface::surfaceClass() const
{
    return m_type;
}

QT_END_NAMESPACE

