/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QGLXINTEGRATION_H
#define QGLXINTEGRATION_H

#include "qxcbwindow.h"
#include "qxcbscreen.h"

#include <qpa/qplatformopenglcontext.h>
#include <qpa/qplatformoffscreensurface.h>
#include <QtGui/QSurfaceFormat>

#include <QtCore/QMutex>

#include <GL/glx.h>

QT_BEGIN_NAMESPACE

class QGLXContext : public QPlatformOpenGLContext
{
public:
    QGLXContext(QXcbScreen *screen, const QSurfaceFormat &format, QPlatformOpenGLContext *share,
                const QVariant &nativeHandle);
    ~QGLXContext();

    bool makeCurrent(QPlatformSurface *surface) Q_DECL_OVERRIDE;
    void doneCurrent() Q_DECL_OVERRIDE;
    void swapBuffers(QPlatformSurface *surface) Q_DECL_OVERRIDE;
    QFunctionPointer getProcAddress(const QByteArray &procName) Q_DECL_OVERRIDE;

    QSurfaceFormat format() const Q_DECL_OVERRIDE;
    bool isSharing() const Q_DECL_OVERRIDE;
    bool isValid() const Q_DECL_OVERRIDE;

    GLXContext glxContext() const { return m_context; }
    GLXFBConfig glxConfig() const { return m_config; }

    QVariant nativeHandle() const;

    static bool supportsThreading();
    static void queryDummyContext();

private:
    void init(QXcbScreen *screen, QPlatformOpenGLContext *share);
    void init(QXcbScreen *screen, QPlatformOpenGLContext *share, const QVariant &nativeHandle);

    Display *m_display;
    GLXFBConfig m_config;
    GLXContext m_context;
    GLXContext m_shareContext;
    QSurfaceFormat m_format;
    bool m_isPBufferCurrent;
    int m_swapInterval;
    bool m_ownsContext;
    static bool m_queriedDummyContext;
    static bool m_supportsThreading;
};


class QGLXPbuffer : public QPlatformOffscreenSurface
{
public:
    explicit QGLXPbuffer(QOffscreenSurface *offscreenSurface);
    ~QGLXPbuffer();

    QSurfaceFormat format() const Q_DECL_OVERRIDE { return m_format; }
    bool isValid() const Q_DECL_OVERRIDE { return m_pbuffer != 0; }

    GLXPbuffer pbuffer() const { return m_pbuffer; }

private:
    QSurfaceFormat m_format;
    QXcbScreen *m_screen;
    GLXPbuffer m_pbuffer;
};

QT_END_NAMESPACE

#endif
