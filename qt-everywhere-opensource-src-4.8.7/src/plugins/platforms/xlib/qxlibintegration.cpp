/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qxlibintegration.h"
#include "qxlibwindowsurface.h"
#include <QtGui/private/qpixmap_raster_p.h>
#include <QtCore/qdebug.h>

#include "qxlibwindow.h"
#include "qgenericunixfontdatabase.h"
#include "qxlibscreen.h"
#include "qxlibclipboard.h"
#include "qxlibdisplay.h"
#include "qxlibnativeinterface.h"

#if !defined(QT_NO_OPENGL)
#if !defined(QT_OPENGL_ES_2)
#include <GL/glx.h>
#else
#include <EGL/egl.h>
#endif //!defined(QT_OPENGL_ES_2)
#include <private/qwindowsurface_gl_p.h>
#include <private/qpixmapdata_gl_p.h>
#endif //QT_NO_OPENGL

QT_BEGIN_NAMESPACE

QXlibIntegration::QXlibIntegration(bool useOpenGL)
    : mUseOpenGL(useOpenGL)
    , mFontDb(new QGenericUnixFontDatabase())
    , mClipboard(0)
    , mNativeInterface(new QXlibNativeInterface)
{
    mPrimaryScreen = new QXlibScreen();
    mScreens.append(mPrimaryScreen);
}

bool QXlibIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case OpenGL: return hasOpenGL();
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPixmapData *QXlibIntegration::createPixmapData(QPixmapData::PixelType type) const
{
#ifndef QT_NO_OPENGL
    if (mUseOpenGL)
        return new QGLPixmapData(type);
#endif
    return new QRasterPixmapData(type);
}

QWindowSurface *QXlibIntegration::createWindowSurface(QWidget *widget, WId) const
{
#ifndef QT_NO_OPENGL
    if (mUseOpenGL)
        return new QGLWindowSurface(widget);
#endif
    return new QXlibWindowSurface(widget);
}


QPlatformWindow *QXlibIntegration::createPlatformWindow(QWidget *widget, WId /*winId*/) const
{
    return new QXlibWindow(widget);
}



QPixmap QXlibIntegration::grabWindow(WId window, int x, int y, int width, int height) const
{
    QImage image;
    QWidget *widget = QWidget::find(window);
    if (widget) {
        QXlibScreen *screen = QXlibScreen::testLiteScreenForWidget(widget);
        image = screen->grabWindow(window,x,y,width,height);
    } else {
        for (int i = 0; i < mScreens.size(); i++) {
            QXlibScreen *screen = static_cast<QXlibScreen *>(mScreens[i]);
            if (screen->rootWindow() == window) {
                image = screen->grabWindow(window,x,y,width,height);
            }
        }
    }
    return QPixmap::fromImage(image);
}

QPlatformFontDatabase *QXlibIntegration::fontDatabase() const
{
    return mFontDb;
}

QPlatformClipboard * QXlibIntegration::clipboard() const
{
    //Use lazy init since clipboard needs QTestliteScreen
    if (!mClipboard) {
        QXlibIntegration *that = const_cast<QXlibIntegration *>(this);
        that->mClipboard = new QXlibClipboard(mPrimaryScreen);
    }
    return mClipboard;
}

QPlatformNativeInterface * QXlibIntegration::nativeInterface() const
{
    return mNativeInterface;
}

bool QXlibIntegration::hasOpenGL() const
{
#if !defined(QT_NO_OPENGL)
#if !defined(QT_OPENGL_ES_2)
    QXlibScreen *screen = static_cast<QXlibScreen *>(mScreens.at(0));
    return glXQueryExtension(screen->display()->nativeDisplay(), 0, 0) != 0;
#else
    static bool eglHasbeenInitialized = false;
    static bool wasEglInitialized = false;
    if (!eglHasbeenInitialized) {
        eglHasbeenInitialized = true;
        QXlibScreen *screen = static_cast<QXlibScreen *>(mScreens.at(0));
        EGLint major, minor;
        eglBindAPI(EGL_OPENGL_ES_API);
        EGLDisplay disp = eglGetDisplay(screen->display()->nativeDisplay());
        wasEglInitialized = eglInitialize(disp,&major,&minor);
        screen->setEglDisplay(disp);
    }
    return wasEglInitialized;
#endif
#endif
    return false;
}

QT_END_NAMESPACE
