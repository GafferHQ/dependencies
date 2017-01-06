/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QEGL_P_H
#define QEGL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience of
// the QtOpenGL and QtOpenVG modules.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_INCLUDE_NAMESPACE

#ifndef QT_NO_EGL
#if defined(QT_OPENGL_ES_2)
#   include <GLES2/gl2.h>
#endif

#if defined(QT_GLES_EGL)
#   include <GLES/egl.h>
#else
#   include <EGL/egl.h>
#endif
#if !defined(EGL_VERSION_1_2)
typedef unsigned int EGLenum;
typedef void *EGLClientBuffer;
#endif
#else

//types from egltypes.h for compiling stub without EGL headers
typedef int EGLBoolean;
typedef int EGLint;
typedef int EGLenum;
typedef int    NativeDisplayType;
typedef void*  NativeWindowType;
typedef void*  NativePixmapType;
typedef int EGLDisplay;
typedef int EGLConfig;
typedef int EGLSurface;
typedef int EGLContext;
typedef int EGLClientBuffer;
#define EGL_NONE            0x3038  /* Attrib list terminator */

#endif

#if defined(Q_WS_X11)
// If <EGL/egl.h> included <X11/Xlib.h>, then the global namespace
// may have been polluted with X #define's.  The following makes sure
// the X11 headers were included properly and then cleans things up.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef Bool
#undef Status
#undef None
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef Type
#undef FontChange
#undef CursorShape
#undef Unsorted
#undef GrayScale
#endif

// Internally we use the EGL-prefixed native types which are used in EGL >= 1.3.
// For older versions of EGL, we have to define these types ourselves here:
#if !defined(EGL_VERSION_1_3) && !defined(QEGL_NATIVE_TYPES_DEFINED)
#undef EGLNativeWindowType
#undef EGLNativePixmapType
#undef EGLNativeDisplayType
typedef NativeWindowType EGLNativeWindowType;
typedef NativePixmapType EGLNativePixmapType;
typedef NativeDisplayType EGLNativeDisplayType;
#define QEGL_NATIVE_TYPES_DEFINED 1
#endif

QT_END_INCLUDE_NAMESPACE

#include <QtGui/qpaintdevice.h>
#include <QFlags>

QT_BEGIN_NAMESPACE

#define QEGL_NO_CONFIG ((EGLConfig)-1)

#ifndef EGLAPIENTRY
#define EGLAPIENTRY
#endif

// Try to get some info to debug the symbian build failues:
#ifdef Q_OS_SYMBIAN

#ifdef EGL_KHR_image
#warning "EGL_KHR_image is defined"
#else
#warning "EGL_KHR_image is NOT defined"
#endif

#ifdef EGL_KHR_image_base
#warning "EGL_KHR_image_base is defined"
#else
#warning "EGL_KHR_image_base is NOT defined"
#endif

#ifdef EGL_EGLEXT_PROTOTYPES
#warning "EGL_EGLEXT_PROTOTYPES is defined"
#else
#warning "EGL_EGLEXT_PROTOTYPES NOT not defined"
#endif

#endif


// Declare/define the bits of EGL_KHR_image_base we need:
#if !defined(EGL_KHR_image) && !defined(EGL_KHR_image_base)
#ifdef Q_OS_SYMBIAN
//symbian version of eglext.h differs from the khronos reference
typedef int EGLImageKHR;
#else
typedef void *EGLImageKHR;
#endif

#if !defined(EGL_NO_IMAGE_KHR)
#define EGL_NO_IMAGE_KHR            ((EGLImageKHR)0)
#endif
#if !defined(EGL_IMAGE_PRESERVED_KHR)
#define EGL_IMAGE_PRESERVED_KHR     0x30D2
#endif
#if !defined(EGL_KHR_image_base)
#define EGL_KHR_image_base
#endif
#endif

#if !defined(EGL_KHR_image)
#define EGL_NATIVE_PIXMAP_KHR       0x30B0
#endif
#if !defined(EGL_KHR_image_pixmap)
#define EGL_KHR_image_pixmap
#endif


class QEglProperties;

namespace QEgl {
    enum API
    {
        OpenGL,
        OpenVG
    };

    enum PixelFormatMatch
    {
        ExactPixelFormat,
        BestPixelFormat
    };

    enum ConfigOption
    {
        NoOptions   = 0,
        Translucent = 0x01,
        Renderable  = 0x02  // Config will be compatable with the paint engines (VG or GL)
    };
    Q_DECLARE_FLAGS(ConfigOptions, ConfigOption)

    // Most of the time we use the same config for things like widgets & pixmaps, so rather than
    // go through the eglChooseConfig loop every time, we use defaultConfig, which will return
    // the config for a particular device/api/option combo. This function assumes that once a
    // config is chosen for a particular combo, it's safe to always use that combo.
    Q_GUI_EXPORT EGLConfig  defaultConfig(int devType, API api, ConfigOptions options);

    Q_GUI_EXPORT EGLConfig  chooseConfig(const QEglProperties* configAttribs, QEgl::PixelFormatMatch match = QEgl::ExactPixelFormat);
    Q_GUI_EXPORT EGLSurface createSurface(QPaintDevice *device, EGLConfig cfg, const QEglProperties *surfaceAttribs = 0);

    Q_GUI_EXPORT void dumpAllConfigs();

#ifdef QT_NO_EGL
    Q_GUI_EXPORT QString errorString(EGLint code = 0);
#else
    Q_GUI_EXPORT QString errorString(EGLint code = eglGetError());
#endif

    Q_GUI_EXPORT QString extensions();
    Q_GUI_EXPORT bool hasExtension(const char* extensionName);

    Q_GUI_EXPORT EGLDisplay display();

    Q_GUI_EXPORT EGLNativeDisplayType nativeDisplay();
    Q_GUI_EXPORT EGLNativeWindowType  nativeWindow(QWidget*);
    Q_GUI_EXPORT EGLNativePixmapType  nativePixmap(QPixmap*);

    // Extension functions
    Q_GUI_EXPORT EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    Q_GUI_EXPORT EGLBoolean  eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR img);
    Q_GUI_EXPORT EGLBoolean eglSwapBuffersRegion2NOK(EGLDisplay dpy, EGLSurface surface, EGLint count, const EGLint *rects);

#ifdef Q_WS_X11
    Q_GUI_EXPORT VisualID getCompatibleVisualId(EGLConfig config);
#endif
}

Q_DECLARE_OPERATORS_FOR_FLAGS(QEgl::ConfigOptions)

QT_END_NAMESPACE

#endif //QEGL_P_H
