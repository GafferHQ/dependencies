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

#ifndef QEGLFSWINDOW_H
#define QEGLFSWINDOW_H

#include "qeglfsintegration.h"
#include "qeglfsscreen.h"
#include "qeglfsglobal.h"

#include <qpa/qplatformwindow.h>
#include <QtPlatformSupport/private/qopenglcompositor_p.h>
#include <EGL/egl.h>

QT_BEGIN_NAMESPACE

class QOpenGLCompositorBackingStore;
class QPlatformTextureList;

class Q_EGLFS_EXPORT QEglFSWindow : public QPlatformWindow, public QOpenGLCompositorWindow
{
public:
    QEglFSWindow(QWindow *w);
    ~QEglFSWindow();

    void create();
    void destroy();

    void setGeometry(const QRect &) Q_DECL_OVERRIDE;
    QRect geometry() const Q_DECL_OVERRIDE;
    void setVisible(bool visible) Q_DECL_OVERRIDE;
    void requestActivateWindow() Q_DECL_OVERRIDE;
    void raise() Q_DECL_OVERRIDE;
    void lower() Q_DECL_OVERRIDE;

    void propagateSizeHints() Q_DECL_OVERRIDE { }
    void setMask(const QRegion &) Q_DECL_OVERRIDE { }
    bool setKeyboardGrabEnabled(bool) Q_DECL_OVERRIDE { return false; }
    bool setMouseGrabEnabled(bool) Q_DECL_OVERRIDE { return false; }
    void setOpacity(qreal) Q_DECL_OVERRIDE;
    WId winId() const Q_DECL_OVERRIDE;

    QSurfaceFormat format() const Q_DECL_OVERRIDE;

    EGLNativeWindowType eglWindow() const;
    EGLSurface surface() const;
    QEglFSScreen *screen() const;

    bool hasNativeWindow() const { return m_flags.testFlag(HasNativeWindow); }

    virtual void invalidateSurface() Q_DECL_OVERRIDE;
    virtual void resetSurface();

    QOpenGLCompositorBackingStore *backingStore() { return m_backingStore; }
    void setBackingStore(QOpenGLCompositorBackingStore *backingStore) { m_backingStore = backingStore; }
    bool isRaster() const;

    QWindow *sourceWindow() const Q_DECL_OVERRIDE;
    const QPlatformTextureList *textures() const Q_DECL_OVERRIDE;
    void endCompositing() Q_DECL_OVERRIDE;

protected:
    QOpenGLCompositorBackingStore *m_backingStore;
    bool m_raster;
    WId m_winId;

    EGLSurface m_surface;
    EGLNativeWindowType m_window;

    EGLConfig m_config;
    QSurfaceFormat m_format;

    enum Flag {
        Created = 0x01,
        HasNativeWindow = 0x02
    };
    Q_DECLARE_FLAGS(Flags, Flag)
    Flags m_flags;
};

QT_END_NAMESPACE

#endif // QEGLFSWINDOW_H
