/****************************************************************************
**
** Copyright (C) 2012 BogDan Vatra <bogdan@kde.org>
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

#ifndef QANDROIDPLATFORMINTERATION_H
#define QANDROIDPLATFORMINTERATION_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformmenu.h>
#include <qpa/qplatformnativeinterface.h>

#include <EGL/egl.h>
#include <jni.h>
#include "qandroidinputcontext.h"

#include "qandroidplatformscreen.h"

#include <memory>

QT_BEGIN_NAMESPACE

class QDesktopWidget;
class QAndroidPlatformServices;
class QAndroidSystemLocale;
class QPlatformAccessibility;

struct AndroidStyle;
class QAndroidPlatformNativeInterface: public QPlatformNativeInterface
{
public:
    void *nativeResourceForIntegration(const QByteArray &resource);
    std::shared_ptr<AndroidStyle> m_androidStyle;
};

class QAndroidPlatformIntegration : public QPlatformIntegration
{
    friend class QAndroidPlatformScreen;

public:
    QAndroidPlatformIntegration(const QStringList &paramList);
    ~QAndroidPlatformIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const;

    QPlatformWindow *createPlatformWindow(QWindow *window) const;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const;
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const;
    QAbstractEventDispatcher *createEventDispatcher() const;
    QAndroidPlatformScreen *screen() { return m_primaryScreen; }
    QPlatformOffscreenSurface *createPlatformOffscreenSurface(QOffscreenSurface *surface) const;

    virtual void setDesktopSize(int width, int height);
    virtual void setDisplayMetrics(int width, int height);
    void setScreenSize(int width, int height);
    bool isVirtualDesktop() { return true; }

    QPlatformFontDatabase *fontDatabase() const;

#ifndef QT_NO_CLIPBOARD
    QPlatformClipboard *clipboard() const;
#endif

    QPlatformInputContext *inputContext() const;
    QPlatformNativeInterface *nativeInterface() const;
    QPlatformServices *services() const;

#ifndef QT_NO_ACCESSIBILITY
    virtual QPlatformAccessibility *accessibility() const;
#endif

    QVariant styleHint(StyleHint hint) const;
    Qt::WindowState defaultWindowState(Qt::WindowFlags flags) const;

    QStringList themeNames() const;
    QPlatformTheme *createPlatformTheme(const QString &name) const;

    static void setDefaultDisplayMetrics(int gw, int gh, int sw, int sh, int width, int height);
    static void setDefaultDesktopSize(int gw, int gh);
    static void setScreenOrientation(Qt::ScreenOrientation currentOrientation,
                                     Qt::ScreenOrientation nativeOrientation);

    static QSize defaultDesktopSize()
    {
        return QSize(m_defaultGeometryWidth, m_defaultGeometryHeight);
    }

    QTouchDevice *touchDevice() const { return m_touchDevice; }
    void setTouchDevice(QTouchDevice *touchDevice) { m_touchDevice = touchDevice; }
    static void setDefaultApplicationState(Qt::ApplicationState applicationState) { m_defaultApplicationState = applicationState; }

private:
    EGLDisplay m_eglDisplay;
    QTouchDevice *m_touchDevice;

    QAndroidPlatformScreen *m_primaryScreen;

    QThread *m_mainThread;

    static int m_defaultGeometryWidth;
    static int m_defaultGeometryHeight;
    static int m_defaultPhysicalSizeWidth;
    static int m_defaultPhysicalSizeHeight;
    static int m_defaultScreenWidth;
    static int m_defaultScreenHeight;

    static Qt::ScreenOrientation m_orientation;
    static Qt::ScreenOrientation m_nativeOrientation;

    static Qt::ApplicationState m_defaultApplicationState;

    QPlatformFontDatabase *m_androidFDB;
    QAndroidPlatformNativeInterface *m_androidPlatformNativeInterface;
    QAndroidPlatformServices *m_androidPlatformServices;

#ifndef QT_NO_CLIPBOARD
    QPlatformClipboard *m_androidPlatformClipboard;
#endif

    QAndroidSystemLocale *m_androidSystemLocale;
#ifndef QT_NO_ACCESSIBILITY
    mutable QPlatformAccessibility *m_accessibility;
#endif

    mutable QAndroidInputContext m_platformInputContext;
};

QT_END_NAMESPACE

#endif
