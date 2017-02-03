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

#ifndef QMEEGOGRAPHICSSYSTEMHELPER_H
#define QMEEGOGRAPHICSSYSTEMHELPER_H

#include <QtGui/QPixmap>
#include <QtGui/QImage>
#include "QtMeeGoGraphicsSystemHelper/qmeegolivepixmap.h"

class QLibrary;

//! The base class for accressing special meego graphics system features. 
/*!
 This class is a helper class with static-only methods for accessing various
 meego graphics system functionalities. The way it works is that the helper
 dynamically calls-in to the loaded graphicssystem plugin... therefore, you're 
 expected to make sure that you're indeed running with 'meego' before using any
 of the specialized methods. 

 In example:

 \code
 QPixmap p;
 if (QMeeGoGraphicsSystemHelper::isRunningMeeGo()) {
    p = QMeeGoGraphicsSystemHelper::pixmapWithGLTexture(64, 64);
 } else {
    p = QPixmap(64, 64);
 }
 \endcode

 Calling any of the meego-specific features while not running meego might
 give unpredictable results. The only functions safe to call at all times are:

 \code
 QMeeGoGraphicsSystemHelper::isRunningMeeGo();
 QMeeGoGraphicsSystemHelper::runningGraphicsSystemName(); 
 QMeeGoGraphicsSystemHelper::switchToMeeGo();
 QMeeGoGraphicsSystemHelper::switchToRaster();
 \endcode
*/

class Q_DECL_EXPORT QMeeGoGraphicsSystemHelper 
{
public:
    //! Returns true if running meego.
    /*! 
     Returns true if the currently active (running) system is 'meego' with OpenGL. 
     This returns both true if the app was started with 'meego' or was started with
     'runtime' graphics system and the currently active system through the runtime
     switching is 'meego'.
    */
    static bool isRunningMeeGo();
    
    //! Returns true if running with a 'runtime' graphicssystem.
    /*!
     This function can be used in combination with ::runningGraphicsSystemName to figure out
     the existing situation. 
    */
    static bool isRunningRuntime();

    //! Enables the sending of QMeeGoSwitchEvent's when the graphicssystem switches.
    /*!
      An application that wishes to start receive QMeegoSwitchEvents must call this function.
    */
    static void enableSwitchEvents();

    //! Switches to meego graphics system. 
    /*!
     When running with the 'runtime' graphics system, sets the currently active 
     system to 'meego'. The window surface and all the resources are automatically
     migrated to OpenGL. Will fail if the active graphics system is not 'runtime'.
     Calling this function will emit QMeeGoSwitchEvent to the top level widgets.
     If switch events are enabled, two events will be emitted for each switch --
     one before the switch (QMeeGoSwitchEvent::WillSwitch) and one after the
     switch (QMeeGoSwitchEvent::DidSwitch).
     If the switch policy is set to NoSwitch, this function has no effect.
    */
    static void switchToMeeGo();

    //! Switches to raster graphics system
    /*!
     When running with the 'runtime' graphics system, sets the currently active
     system to 'raster'. The window surface and the graphics resources (including the 
     EGL shared image resources) are automatically migrated back to the CPU. All OpenGL 
     resources (surface, context, cache, font cache) are automaticall anihilated.
     Calling this function will emit QMeeGoSwitchEvent to the top level widgets. If switch
     events are enabled, two events will be emitted for each switch -- one before the
     switch (QMeeGoSwitchEvent::WillSwitch) and one after the switch (QMeeGoSwitchEvent::DidSwitch).
     If the switch policy is set to NoSwitch, this function has no effect.
    */
    static void switchToRaster();

    //! Used to specify the policy for graphics system switching.
    enum SwitchPolicy {
        AutomaticSwitch,    /**< Automatic switching */
        ManualSwitch,       /**< Switching is controleld by the application */
        NoSwitch            /**< Switching is disabled completely */
    };

    //! Sets the policy of graphicssystem switching
    /*!
     By default, the switch to raster happens automatically when all windows are either
     minimized or when the last window is destroyed. This function lets the application
     change the graphicssystem switching policy to prevent the switching from happening
     automatically (that is if the application doesn't want switching at all or wishes
     to control the switching manually).
    */
    static void setSwitchPolicy(SwitchPolicy policy);

    //! Returns the name of the active graphics system
    /*!
     Returns the name of the currently active system. If running with 'runtime' graphics 
     system, returns the name of the active system inside the runtime graphics system
    */
    static QString runningGraphicsSystemName();

    //! Creates a new EGL shared image.
    /*! 
     Creates a new EGL shared image from the given image. The EGL shared image wraps
     a GL texture in the native format and can be easily accessed from other processes.
    */
    static Qt::HANDLE imageToEGLSharedImage(const QImage &image);

    //! Creates a QPixmap from an EGL shared image
    /*!
     Creates a new QPixmap from the given EGL shared image handle. The QPixmap can be 
     used for painting like any other pixmap. The softImage should point to an alternative, 
     software version of the graphical resource -- ie. obtained from theme daemon. The 
     softImage can be allocated on a QSharedMemory slice for easy sharing across processes
     too. When the application is migrated ToRaster, this softImage will replace the
     contents of the sharedImage.
     
     It's ok to call this function too when not running 'meego' graphics system. In this
     case it'll create a QPixmap backed with a raster data (from softImage)... but when 
     the system is switched back to 'meego', the QPixmap will be migrated to a EGL-shared image
     backed storage (handle).
    */
    static QPixmap pixmapFromEGLSharedImage(Qt::HANDLE handle, const QImage &softImage);

    //! Destroys an EGL shared image. 
    /*!
     Destroys an EGLSharedImage previously created with an ::imageToEGLSharedImage call.
     Returns true if the image was found and the destruction was successful. Notice that
     this destroys the image for all processes using it. 
    */
    static bool destroyEGLSharedImage(Qt::HANDLE handle);
    
    //! Updates the QPixmap backed with an EGLShared image. 
    /*! 
     This function re-reads the softImage that was specified when creating the pixmap with
     ::pixmapFromEGLSharedImage and updates the EGL Shared image contents. It can be used
     to share cross-proccess mutable EGLShared images. 
    */
    static void updateEGLSharedImagePixmap(QPixmap *p);

    //! Create a new QPixmap with a GL texture.
    /*!
     Creates a new QPixmap which is backed by an OpenGL local texture. Drawing to this
     QPixmap will be accelerated by hardware -- unlike the normal (new QPixmap()) pixmaps, 
     which are backed by a software engine and only migrated to GPU when used. Migrating those
     GL-backed pixmaps when going ToRaster is expsensive (they need to be downloaded from 
     GPU to CPU) so use wisely. 
    */
    static QPixmap pixmapWithGLTexture(int w, int h);
    
    //! Sets translucency (alpha) on the base window surface.
    /*!
     This function needs to be called *before* any widget/content is created. 
     When called with true, the base window surface will be translucent and initialized
     with QGLFormat.alpha == true.

     This function is *deprecated*. Set Qt::WA_TranslucentBackground attribute
     on the top-level widget *before* you show it instead.
    */
    static void setTranslucent(bool translucent);

    //! Used to specify the mode for swapping buffers in double-buffered GL rendering.
    enum SwapMode {
        AutomaticSwap,      /**< Automatically choose netween full and partial updates (25% threshold) */
        AlwaysFullSwap,     /**< Always do a full swap even if partial updates support present */
        AlwaysPartialSwap,  /**< Always do a partial swap (if support present) no matter what threshold */
        KillSwap            /**< Do not perform buffer swapping at all (no picture) */
    };

    //! Sets the buffer swapping mode.
    /*!
     This can be only called when running with the meego graphics system.
     The KillSwap mode can be specififed to effectively block painting.

     This functionality should be used only by applications counting on a specific behavior.
     Most applications should use the default automatic behavior.
    */
    static void setSwapBehavior(SwapMode mode);
};

#endif
