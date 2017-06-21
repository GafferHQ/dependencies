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

#include "qwaylanddisplay_p.h"

#include "qwaylandintegration_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandcursor_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandclipboard_p.h"
#include "qwaylanddatadevicemanager_p.h"
#include "qwaylandhardwareintegration_p.h"
#include "qwaylandxdgshell_p.h"
#include "qwaylandxdgsurface_p.h"
#include "qwaylandwlshellsurface_p.h"

#include "qwaylandwindowmanagerintegration_p.h"
#include "qwaylandshellintegration_p.h"
#include "qwaylandclientbufferintegration_p.h"

#include "qwaylandextendedsurface_p.h"
#include "qwaylandsubsurface_p.h"
#include "qwaylandtouch_p.h"
#include "qwaylandqtkey_p.h"

#include <QtWaylandClient/private/qwayland-text.h>
#include <QtWaylandClient/private/qwayland-xdg-shell.h>

#include <QtCore/QAbstractEventDispatcher>
#include <QtGui/private/qguiapplication_p.h>

#include <QtCore/QDebug>

#include <errno.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

struct wl_surface *QWaylandDisplay::createSurface(void *handle)
{
    struct wl_surface *surface = mCompositor.create_surface();
    wl_surface_set_user_data(surface, handle);
    return surface;
}

QWaylandShellSurface *QWaylandDisplay::createShellSurface(QWaylandWindow *window)
{
    if (mWaylandIntegration->shellIntegration())
        return mWaylandIntegration->shellIntegration()->createShellSurface(window);

    if (shellXdg()) {
        return new QWaylandXdgSurface(shellXdg()->get_xdg_surface(window->object()), window);
    } else if (shell()) {
        return new QWaylandWlShellSurface(shell()->get_shell_surface(window->object()), window);
    }

    return Q_NULLPTR;
}

struct ::wl_region *QWaylandDisplay::createRegion(const QRegion &qregion)
{
    struct ::wl_region *region = mCompositor.create_region();

    Q_FOREACH (const QRect &rect, qregion.rects())
        wl_region_add(region, rect.x(), rect.y(), rect.width(), rect.height());

    return region;
}

::wl_subsurface *QWaylandDisplay::createSubSurface(QWaylandWindow *window, QWaylandWindow *parent)
{
    if (!mSubCompositor) {
        return NULL;
    }

    return mSubCompositor->get_subsurface(window->object(), parent->object());
}

QWaylandClientBufferIntegration * QWaylandDisplay::clientBufferIntegration() const
{
    return mWaylandIntegration->clientBufferIntegration();
}

QWaylandWindowManagerIntegration *QWaylandDisplay::windowManagerIntegration() const
{
    return mWindowManagerIntegration.data();
}

QWaylandInputDevice *QWaylandDisplay::lastKeyboardFocusInputDevice() const
{
    return mLastKeyboardFocusInputDevice;
}

void QWaylandDisplay::setLastKeyboardFocusInputDevice(QWaylandInputDevice *device)
{
    mLastKeyboardFocusInputDevice = device;
}

QWaylandDisplay::QWaylandDisplay(QWaylandIntegration *waylandIntegration)
    : mWaylandIntegration(waylandIntegration)
    , mLastKeyboardFocusInputDevice(0)
    , mDndSelectionHandler(0)
    , mWindowExtension(0)
    , mSubCompositor(0)
    , mTouchExtension(0)
    , mQtKeyExtension(0)
    , mTextInputManager(0)
    , mHardwareIntegration(0)
    , mLastInputSerial(0)
    , mLastInputDevice(0)
    , mLastInputWindow(0)
{
    qRegisterMetaType<uint32_t>("uint32_t");

    mDisplay = wl_display_connect(NULL);
    if (mDisplay == NULL) {
        qErrnoWarning(errno, "Failed to create display");
        ::exit(1);
    }

    struct ::wl_registry *registry = wl_display_get_registry(mDisplay);
    init(registry);

    mWindowManagerIntegration.reset(new QWaylandWindowManagerIntegration(this));

    forceRoundTrip();
}

QWaylandDisplay::~QWaylandDisplay(void)
{
    qDeleteAll(mInputDevices);
    mInputDevices.clear();

    foreach (QWaylandScreen *screen, mScreens) {
        mWaylandIntegration->destroyScreen(screen);
    }
    mScreens.clear();
    delete mDndSelectionHandler.take();
    wl_display_disconnect(mDisplay);
}

void QWaylandDisplay::checkError() const
{
    int ecode = wl_display_get_error(mDisplay);
    if ((ecode == EPIPE || ecode == ECONNRESET)) {
        // special case this to provide a nicer error
        qWarning("The Wayland connection broke. Did the Wayland compositor die?");
    } else {
        qErrnoWarning(ecode, "The Wayland connection experienced a fatal error");
    }
}

void QWaylandDisplay::flushRequests()
{
    if (wl_display_prepare_read(mDisplay) == 0) {
        wl_display_read_events(mDisplay);
    }

    if (wl_display_dispatch_pending(mDisplay) < 0) {
        checkError();
        exitWithError();
    }

    wl_display_flush(mDisplay);
}


void QWaylandDisplay::blockingReadEvents()
{
    if (wl_display_dispatch(mDisplay) < 0) {
        checkError();
        exitWithError();
    }
}

void QWaylandDisplay::exitWithError()
{
    ::exit(1);
}

QWaylandScreen *QWaylandDisplay::screenForOutput(struct wl_output *output) const
{
    for (int i = 0; i < mScreens.size(); ++i) {
        QWaylandScreen *screen = static_cast<QWaylandScreen *>(mScreens.at(i));
        if (screen->output() == output)
            return screen;
    }
    return 0;
}

void QWaylandDisplay::waitForScreens()
{
    flushRequests();

    while (true) {
        bool screensReady = !mScreens.isEmpty();

        for (int ii = 0; screensReady && ii < mScreens.count(); ++ii) {
            if (mScreens.at(ii)->geometry() == QRect(0, 0, 0, 0))
                screensReady = false;
        }

        if (!screensReady)
            blockingReadEvents();
        else
            return;
    }
}

void QWaylandDisplay::registry_global(uint32_t id, const QString &interface, uint32_t version)
{
    Q_UNUSED(version);

    struct ::wl_registry *registry = object();

    if (interface == QStringLiteral("wl_output")) {
        QWaylandScreen *screen = new QWaylandScreen(this, version, id);
        mScreens.append(screen);
        // We need to get the output events before creating surfaces
        forceRoundTrip();
        screen->init();
        mWaylandIntegration->screenAdded(screen);
    } else if (interface == QStringLiteral("wl_compositor")) {
        mCompositorVersion = qMin((int)version, 3);
        mCompositor.init(registry, id, mCompositorVersion);
    } else if (interface == QStringLiteral("wl_shm")) {
        mShm = static_cast<struct wl_shm *>(wl_registry_bind(registry, id, &wl_shm_interface,1));
    } else if (interface == QStringLiteral("xdg_shell")
               && qEnvironmentVariableIsSet("QT_WAYLAND_USE_XDG_SHELL")) {
        mShellXdg.reset(new QWaylandXdgShell(registry,id));
    } else if (interface == QStringLiteral("wl_shell")){
        mShell.reset(new QtWayland::wl_shell(registry, id, 1));
    } else if (interface == QStringLiteral("wl_seat")) {
        QWaylandInputDevice *inputDevice = mWaylandIntegration->createInputDevice(this, version, id);
        mInputDevices.append(inputDevice);
    } else if (interface == QStringLiteral("wl_data_device_manager")) {
        mDndSelectionHandler.reset(new QWaylandDataDeviceManager(this, id));
    } else if (interface == QStringLiteral("qt_surface_extension")) {
        mWindowExtension.reset(new QtWayland::qt_surface_extension(registry, id, 1));
    } else if (interface == QStringLiteral("wl_subcompositor")) {
        mSubCompositor.reset(new QtWayland::wl_subcompositor(registry, id, 1));
    } else if (interface == QStringLiteral("qt_touch_extension")) {
        mTouchExtension.reset(new QWaylandTouchExtension(this, id));
    } else if (interface == QStringLiteral("qt_key_extension")) {
        mQtKeyExtension.reset(new QWaylandQtKeyExtension(this, id));
    } else if (interface == QStringLiteral("wl_text_input_manager")) {
        mTextInputManager.reset(new QtWayland::wl_text_input_manager(registry, id, 1));
    } else if (interface == QStringLiteral("qt_hardware_integration")) {
        mHardwareIntegration.reset(new QWaylandHardwareIntegration(registry, id));
        // make a roundtrip here since we need to receive the events sent by
        // qt_hardware_integration before creating windows
        forceRoundTrip();
    }

    mGlobals.append(RegistryGlobal(id, interface, version, registry));

    foreach (Listener l, mRegistryListeners)
        (*l.listener)(l.data, registry, id, interface, version);
}

void QWaylandDisplay::registry_global_remove(uint32_t id)
{
    for (int i = 0, ie = mGlobals.count(); i != ie; ++i) {
        RegistryGlobal &global = mGlobals[i];
        if (global.id == id) {
            if (global.interface == QStringLiteral("wl_output")) {
                foreach (QWaylandScreen *screen, mScreens) {
                    if (screen->outputId() == id) {
                        mScreens.removeOne(screen);
                        mWaylandIntegration->destroyScreen(screen);
                        break;
                    }
                }
            }
            mGlobals.removeAt(i);
            break;
        }
    }
}

void QWaylandDisplay::addRegistryListener(RegistryListener listener, void *data)
{
    Listener l = { listener, data };
    mRegistryListeners.append(l);
    for (int i = 0, ie = mGlobals.count(); i != ie; ++i)
        (*l.listener)(l.data, mGlobals[i].registry, mGlobals[i].id, mGlobals[i].interface, mGlobals[i].version);
}

uint32_t QWaylandDisplay::currentTimeMillisec()
{
    //### we throw away the time information
    struct timeval tv;
    int ret = gettimeofday(&tv, 0);
    if (ret == 0)
        return tv.tv_sec*1000 + tv.tv_usec/1000;
    return 0;
}

static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
    Q_UNUSED(serial)
    bool *done = static_cast<bool *>(data);

    *done = true;
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
    sync_callback
};

void QWaylandDisplay::forceRoundTrip()
{
    // wl_display_roundtrip() works on the main queue only,
    // but we use a separate one, so basically reimplement it here
    int ret = 0;
    bool done = false;
    wl_callback *callback = wl_display_sync(mDisplay);
    wl_callback_add_listener(callback, &sync_listener, &done);
    flushRequests();
    if (QThread::currentThread()->eventDispatcher()) {
        while (!done && ret >= 0) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::WaitForMoreEvents);
            ret = wl_display_dispatch_pending(mDisplay);
        }
    } else {
        while (!done && ret >= 0)
            ret = wl_display_dispatch(mDisplay);
    }

    if (ret == -1 && !done)
        wl_callback_destroy(callback);
}

QtWayland::xdg_shell *QWaylandDisplay::shellXdg()
{
    return mShellXdg.data();
}

bool QWaylandDisplay::supportsWindowDecoration() const
{
    static bool disabled = qgetenv("QT_WAYLAND_DISABLE_WINDOWDECORATION").toInt();
    // Stop early when disabled via the environment. Do not try to load the integration in
    // order to play nice with SHM-only, buffer integration-less systems.
    if (disabled)
        return false;

    static bool integrationSupport = clientBufferIntegration() && clientBufferIntegration()->supportsWindowDecoration();
    return integrationSupport;
}

QWaylandWindow *QWaylandDisplay::lastInputWindow() const
{
    return mLastInputWindow.data();
}

void QWaylandDisplay::setLastInputDevice(QWaylandInputDevice *device, uint32_t serial, QWaylandWindow *win)
{
    mLastInputDevice = device;
    mLastInputSerial = serial;
    mLastInputWindow = win;
}

}

QT_END_NAMESPACE
