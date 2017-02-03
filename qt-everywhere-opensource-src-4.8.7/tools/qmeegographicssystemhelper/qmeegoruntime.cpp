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

#include "qmeegoruntime.h"

#include "qmeegoswitchevent.h"

#include <QtGui/QApplication>
#include <QtGui/QWidget>

#include <private/qlibrary_p.h>
#include <private/qfactoryloader_p.h>
#include <private/qgraphicssystemplugin_p.h>
#include <stdio.h>

#define ENSURE_INITIALIZED {if (!initialized) initialize();}

bool QMeeGoRuntime::initialized = false;
bool QMeeGoRuntime::switchEventsEnabled = false;

typedef int (*QMeeGoImageToEglSharedImageFunc) (const QImage&);
typedef QPixmapData* (*QMeeGoPixmapDataFromEglSharedImageFunc) (Qt::HANDLE handle, const QImage&);
typedef QPixmapData* (*QMeeGoPixmapDataWithGLTextureFunc) (int w, int h);
typedef bool (*QMeeGoDestroyEGLSharedImageFunc) (Qt::HANDLE handle);
typedef void (*QMeeGoUpdateEglSharedImagePixmapFunc) (QPixmap*);
typedef void (*QMeeGoSetSurfaceFixedSizeFunc) (int w, int h);
typedef void (*QMeeGoSetSurfaceScalingFunc) (int x, int y, int w, int h);
typedef void (*QMeeGoSetTranslucentFunc) (bool translucent);
typedef QPixmapData* (*QMeeGoPixmapDataWithNewLiveTextureFunc) (int w, int h, QImage::Format format);
typedef QPixmapData* (*QMeeGoPixmapDataFromLiveTextureHandleFunc) (Qt::HANDLE h);
typedef QImage* (*QMeeGoLiveTextureLockFunc) (QPixmap*, void* fenceSync);
typedef bool (*QMeeGoLiveTextureReleaseFunc) (QPixmap*, QImage *i);
typedef Qt::HANDLE (*QMeeGoLiveTextureGetHandleFunc) (QPixmap*);
typedef void* (*QMeeGoCreateFenceSyncFunc) (void);
typedef void (*QMeeGoDestroyFenceSyncFunc) (void *fs);
typedef void (*QMeeGoInvalidateLiveSurfacesFunc) (void);
typedef void (*QMeeGoSwitchToRasterFunc) (void);
typedef void (*QMeeGoSwitchToMeeGoFunc) (void);
typedef void (*QMeeGoRegisterSwitchCallbackFunc) (void (*callback)(int type, const char *name));
typedef void (*QMeeGoSetSwitchPolicyFunc) (int policy);

static QMeeGoImageToEglSharedImageFunc qt_meego_image_to_egl_shared_image = NULL;
static QMeeGoPixmapDataFromEglSharedImageFunc qt_meego_pixmapdata_from_egl_shared_image = NULL;
static QMeeGoPixmapDataWithGLTextureFunc qt_meego_pixmapdata_with_gl_texture = NULL;
static QMeeGoDestroyEGLSharedImageFunc qt_meego_destroy_egl_shared_image = NULL;
static QMeeGoUpdateEglSharedImagePixmapFunc qt_meego_update_egl_shared_image_pixmap = NULL;
static QMeeGoSetSurfaceFixedSizeFunc qt_meego_set_surface_fixed_size = NULL;
static QMeeGoSetSurfaceScalingFunc qt_meego_set_surface_scaling = NULL;
static QMeeGoSetTranslucentFunc qt_meego_set_translucent = NULL;
static QMeeGoPixmapDataWithNewLiveTextureFunc qt_meego_pixmapdata_with_new_live_texture = NULL;
static QMeeGoPixmapDataFromLiveTextureHandleFunc qt_meego_pixmapdata_from_live_texture_handle = NULL;
static QMeeGoLiveTextureLockFunc qt_meego_live_texture_lock = NULL;
static QMeeGoLiveTextureReleaseFunc qt_meego_live_texture_release = NULL;
static QMeeGoLiveTextureGetHandleFunc qt_meego_live_texture_get_handle = NULL;
static QMeeGoCreateFenceSyncFunc qt_meego_create_fence_sync = NULL;
static QMeeGoDestroyFenceSyncFunc qt_meego_destroy_fence_sync = NULL;
static QMeeGoInvalidateLiveSurfacesFunc qt_meego_invalidate_live_surfaces = NULL;
static QMeeGoSwitchToRasterFunc qt_meego_switch_to_raster = NULL;
static QMeeGoSwitchToMeeGoFunc qt_meego_switch_to_meego = NULL;
static QMeeGoRegisterSwitchCallbackFunc qt_meego_register_switch_callback = NULL;
static QMeeGoSetSwitchPolicyFunc qt_meego_set_switch_policy = NULL;

extern "C" void handleSwitch(int type, const char *name)
{
    QMeeGoSwitchEvent switchEvent((QLatin1String(name)), QMeeGoSwitchEvent::State(type));
    foreach (QWidget *widget, QApplication::topLevelWidgets())
        QCoreApplication::sendEvent(widget, &switchEvent);
}

void QMeeGoRuntime::initialize()
{
    QFactoryLoader loader(QGraphicsSystemFactoryInterface_iid, QLatin1String("/graphicssystems"), Qt::CaseInsensitive);

    QLibraryPrivate *libraryPrivate = loader.library(QLatin1String("meego"));
    Q_ASSERT(libraryPrivate);

    QLibrary library(libraryPrivate->fileName, libraryPrivate->fullVersion);
    library.setLoadHints(QLibrary::ImprovedSearchHeuristics);
    bool success = library.load();

    if (success) {
        qt_meego_image_to_egl_shared_image = (QMeeGoImageToEglSharedImageFunc) library.resolve("qt_meego_image_to_egl_shared_image");
        qt_meego_pixmapdata_from_egl_shared_image = (QMeeGoPixmapDataFromEglSharedImageFunc) library.resolve("qt_meego_pixmapdata_from_egl_shared_image");
        qt_meego_pixmapdata_with_gl_texture = (QMeeGoPixmapDataWithGLTextureFunc) library.resolve("qt_meego_pixmapdata_with_gl_texture");
        qt_meego_destroy_egl_shared_image = (QMeeGoDestroyEGLSharedImageFunc) library.resolve("qt_meego_destroy_egl_shared_image");
        qt_meego_update_egl_shared_image_pixmap = (QMeeGoUpdateEglSharedImagePixmapFunc) library.resolve("qt_meego_update_egl_shared_image_pixmap");
        qt_meego_set_surface_fixed_size = (QMeeGoSetSurfaceFixedSizeFunc) library.resolve("qt_meego_set_surface_fixed_size");
        qt_meego_set_surface_scaling = (QMeeGoSetSurfaceScalingFunc) library.resolve("qt_meego_set_surface_scaling");
        qt_meego_set_translucent = (QMeeGoSetTranslucentFunc) library.resolve("qt_meego_set_translucent");
        qt_meego_pixmapdata_with_new_live_texture = (QMeeGoPixmapDataWithNewLiveTextureFunc) library.resolve("qt_meego_pixmapdata_with_new_live_texture");
        qt_meego_pixmapdata_from_live_texture_handle = (QMeeGoPixmapDataFromLiveTextureHandleFunc) library.resolve("qt_meego_pixmapdata_from_live_texture_handle");
        qt_meego_live_texture_lock = (QMeeGoLiveTextureLockFunc) library.resolve("qt_meego_live_texture_lock");
        qt_meego_live_texture_release = (QMeeGoLiveTextureReleaseFunc) library.resolve("qt_meego_live_texture_release");
        qt_meego_live_texture_get_handle = (QMeeGoLiveTextureGetHandleFunc) library.resolve("qt_meego_live_texture_get_handle");
        qt_meego_create_fence_sync = (QMeeGoCreateFenceSyncFunc) library.resolve("qt_meego_create_fence_sync");
        qt_meego_destroy_fence_sync = (QMeeGoDestroyFenceSyncFunc) library.resolve("qt_meego_destroy_fence_sync");
        qt_meego_invalidate_live_surfaces = (QMeeGoInvalidateLiveSurfacesFunc) library.resolve("qt_meego_invalidate_live_surfaces");
        qt_meego_switch_to_raster = (QMeeGoSwitchToRasterFunc) library.resolve("qt_meego_switch_to_raster");
        qt_meego_switch_to_meego = (QMeeGoSwitchToMeeGoFunc) library.resolve("qt_meego_switch_to_meego");
        qt_meego_register_switch_callback = (QMeeGoRegisterSwitchCallbackFunc) library.resolve("qt_meego_register_switch_callback");
        qt_meego_set_switch_policy = (QMeeGoSetSwitchPolicyFunc) library.resolve("qt_meego_set_switch_policy");

        if (qt_meego_image_to_egl_shared_image && qt_meego_pixmapdata_from_egl_shared_image && 
            qt_meego_pixmapdata_with_gl_texture && qt_meego_destroy_egl_shared_image && qt_meego_update_egl_shared_image_pixmap && 
            qt_meego_set_surface_fixed_size && qt_meego_set_surface_scaling && qt_meego_set_translucent && 
            qt_meego_pixmapdata_with_new_live_texture && qt_meego_pixmapdata_from_live_texture_handle &&
            qt_meego_live_texture_lock && qt_meego_live_texture_release && qt_meego_live_texture_get_handle &&
            qt_meego_create_fence_sync && qt_meego_destroy_fence_sync && qt_meego_invalidate_live_surfaces &&
            qt_meego_switch_to_raster && qt_meego_switch_to_meego && qt_meego_register_switch_callback &&
            qt_meego_set_switch_policy)
        {
            qDebug("Successfully resolved MeeGo graphics system: %s %s\n", qPrintable(libraryPrivate->fileName), qPrintable(libraryPrivate->fullVersion));
        } else {
            Q_ASSERT(false);
        }
    } else {
        Q_ASSERT(false);
    }

    initialized = true;
}

Qt::HANDLE QMeeGoRuntime::imageToEGLSharedImage(const QImage &image)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_image_to_egl_shared_image);
    return qt_meego_image_to_egl_shared_image(image);
}

QPixmapData* QMeeGoRuntime::pixmapDataFromEGLSharedImage(Qt::HANDLE handle, const QImage &softImage)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_pixmapdata_from_egl_shared_image);
    return qt_meego_pixmapdata_from_egl_shared_image(handle, softImage);
}

QPixmapData* QMeeGoRuntime::pixmapDataWithGLTexture(int w, int h)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_pixmapdata_with_gl_texture);
    return qt_meego_pixmapdata_with_gl_texture(w, h);
}

bool QMeeGoRuntime::destroyEGLSharedImage(Qt::HANDLE handle)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_destroy_egl_shared_image);
    return qt_meego_destroy_egl_shared_image(handle);
}

void QMeeGoRuntime::updateEGLSharedImagePixmap(QPixmap *p)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_update_egl_shared_image_pixmap);
    return qt_meego_update_egl_shared_image_pixmap(p);
}

void QMeeGoRuntime::setSurfaceFixedSize(int w, int h)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_set_surface_fixed_size);
    qt_meego_set_surface_fixed_size(w, h);
}

void QMeeGoRuntime::setSurfaceScaling(int x, int y, int w, int h)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_set_surface_scaling);
    qt_meego_set_surface_scaling(x, y, w, h);
}

void QMeeGoRuntime::setTranslucent(bool translucent)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_set_translucent);
    qt_meego_set_translucent(translucent);
}

QPixmapData* QMeeGoRuntime::pixmapDataWithNewLiveTexture(int w, int h, QImage::Format format)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_pixmapdata_with_new_live_texture);
    return qt_meego_pixmapdata_with_new_live_texture(w, h, format);
}

QPixmapData* QMeeGoRuntime::pixmapDataFromLiveTextureHandle(Qt::HANDLE h)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_pixmapdata_from_live_texture_handle);
    return qt_meego_pixmapdata_from_live_texture_handle(h);
}

QImage* QMeeGoRuntime::lockLiveTexture(QPixmap *p, void* fenceSync)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_live_texture_lock);
    return qt_meego_live_texture_lock(p, fenceSync);
}

bool QMeeGoRuntime::releaseLiveTexture(QPixmap *p, QImage *i)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_live_texture_release);
    return qt_meego_live_texture_release(p, i);
}

Qt::HANDLE QMeeGoRuntime::getLiveTextureHandle(QPixmap *pixmap)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_live_texture_get_handle);
    return qt_meego_live_texture_get_handle(pixmap);
}

void* QMeeGoRuntime::createFenceSync()
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_create_fence_sync);
    return qt_meego_create_fence_sync();
}

void QMeeGoRuntime::destroyFenceSync(void *fs)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_destroy_fence_sync);
    qt_meego_destroy_fence_sync(fs);
}

void QMeeGoRuntime::invalidateLiveSurfaces()
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_invalidate_live_surfaces);
    qt_meego_invalidate_live_surfaces();
}

void QMeeGoRuntime::switchToRaster()
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_switch_to_raster);
    qt_meego_switch_to_raster();
}

void QMeeGoRuntime::switchToMeeGo()
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_switch_to_meego);
    qt_meego_switch_to_meego();
}

void QMeeGoRuntime::enableSwitchEvents()
{
    ENSURE_INITIALIZED;
    if (!switchEventsEnabled) {
        Q_ASSERT(qt_meego_register_switch_callback);
        qt_meego_register_switch_callback(handleSwitch);
        switchEventsEnabled = true;
    }
}

void QMeeGoRuntime::setSwitchPolicy(QMeeGoGraphicsSystemHelper::SwitchPolicy policy)
{
    ENSURE_INITIALIZED;
    Q_ASSERT(qt_meego_set_switch_policy);
    qt_meego_set_switch_policy(int(policy));
}
