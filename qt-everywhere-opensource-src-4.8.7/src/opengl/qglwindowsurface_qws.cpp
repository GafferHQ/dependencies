/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtOpenGL module of the Qt Toolkit.
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

#include <QtGui/QPaintDevice>
#include <QtGui/QWidget>
#include <QtOpenGL/QGLWidget>
#include "private/qglwindowsurface_qws_p.h"
#include "private/qpaintengine_opengl_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QWSGLWindowSurface
    \since 4.3
    \ingroup qws
    \preliminary

    \brief The QWSGLWindowSurface class provides the drawing area for top-level
    windows with Qt for Embedded Linux on EGL/OpenGL ES. It also provides the
    drawing area for \l{QGLWidget}s whether they are top-level windows or
    children of another QWidget.

    Note that this class is only available in Qt for Embedded Linux and only
    available if Qt is configured with OpenGL support.
*/

class QWSGLWindowSurfacePrivate
{
public:
    QWSGLWindowSurfacePrivate() :
        qglContext(0), ownsContext(false) {}

    QGLContext *qglContext;
    bool ownsContext;
};

/*!
    Constructs an empty QWSGLWindowSurface for the given top-level \a window.
    The window surface is later initialized from chooseContext() and resources for it
    is typically allocated in setGeometry().
*/
QWSGLWindowSurface::QWSGLWindowSurface(QWidget *window)
    : QWSWindowSurface(window),
      d_ptr(new QWSGLWindowSurfacePrivate)
{
}

/*!
    Constructs an empty QWSGLWindowSurface.
*/
QWSGLWindowSurface::QWSGLWindowSurface()
    : d_ptr(new QWSGLWindowSurfacePrivate)
{
}

/*!
    Destroys the QWSGLWindowSurface object and frees any
    allocated resources.
 */
QWSGLWindowSurface::~QWSGLWindowSurface()
{
    Q_D(QWSGLWindowSurface);
    if (d->ownsContext)
        delete d->qglContext;
    delete d;
}

/*!
    Returns the QGLContext of the window surface.
*/
QGLContext *QWSGLWindowSurface::context() const
{
    Q_D(const QWSGLWindowSurface);
    if (!d->qglContext) {
        QWSGLWindowSurface *that = const_cast<QWSGLWindowSurface*>(this);
        that->setContext(new QGLContext(QGLFormat::defaultFormat()));
        that->d_func()->ownsContext = true;
    }
    return d->qglContext;
}

/*!
    Sets the QGLContext for this window surface to \a context.
*/
void QWSGLWindowSurface::setContext(QGLContext *context)
{
    Q_D(QWSGLWindowSurface);
    if (d->ownsContext) {
        delete d->qglContext;
        d->ownsContext = false;
    }
    d->qglContext = context;
}

QT_END_NAMESPACE
