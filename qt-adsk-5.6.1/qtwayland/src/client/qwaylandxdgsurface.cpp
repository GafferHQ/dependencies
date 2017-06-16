/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the config.tests of the Qt Toolkit.
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

#include "qwaylandxdgsurface_p.h"

#include "qwaylanddisplay_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandabstractdecoration_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandextendedsurface_p.h"


QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandXdgSurface::QWaylandXdgSurface(struct ::xdg_surface *xdg_surface, QWaylandWindow *window)
    : QWaylandShellSurface(window)
    , QtWayland::xdg_surface(xdg_surface)
    , m_window(window)
    , m_maximized(false)
    , m_minimized(false)
    , m_fullscreen(false)
    , m_extendedWindow(Q_NULLPTR)
{
    if (window->display()->windowExtension())
        m_extendedWindow = new QWaylandExtendedSurface(window);
    m_size = m_window->window()->geometry().size();
}

QWaylandXdgSurface::~QWaylandXdgSurface()
{
    xdg_surface_destroy(object());
    delete m_extendedWindow;
}

void QWaylandXdgSurface::resize(QWaylandInputDevice *inputDevice, enum wl_shell_surface_resize edges)
{
    // May need some conversion if types get incompatibles, ATM they're identical
    enum resize_edge const * const arg = reinterpret_cast<enum resize_edge const * const>(&edges);
    resize(inputDevice, *arg);
}

void QWaylandXdgSurface::resize(QWaylandInputDevice *inputDevice, enum resize_edge edges)
{
    resize(inputDevice->wl_seat(),
           inputDevice->serial(),
           edges);
}

void QWaylandXdgSurface::move(QWaylandInputDevice *inputDevice)
{
    move(inputDevice->wl_seat(),
         inputDevice->serial());
}

void QWaylandXdgSurface::setMaximized()
{
    if (!m_maximized)
        set_maximized();
}

void QWaylandXdgSurface::setFullscreen()
{
    if (!m_fullscreen)
        set_fullscreen(Q_NULLPTR);
}

void QWaylandXdgSurface::setNormal()
{
    if (m_fullscreen || m_maximized  || m_minimized) {
        if (m_maximized) {
            unset_maximized();
        }
        if (m_fullscreen) {
            unset_fullscreen();
        }

        m_fullscreen = m_maximized = m_minimized = false;
    }
}

void QWaylandXdgSurface::setMinimized()
{
    m_minimized = true;
    set_minimized();
}

void QWaylandXdgSurface::setTopLevel()
{
    // There's no xdg_shell_surface API for this, ignoring
}

void QWaylandXdgSurface::updateTransientParent(QWindow *parent)
{
    QWaylandWindow *parent_wayland_window = static_cast<QWaylandWindow *>(parent->handle());
    if (!parent_wayland_window)
        return;
    QtWayland::xdg_shell *shell = parent_wayland_window->display()->shellXdg();
    set_parent(shell->get_xdg_surface(parent_wayland_window->object()));
}

void QWaylandXdgSurface::setTitle(const QString & title)
{
    return QtWayland::xdg_surface::set_title(title);
}

void QWaylandXdgSurface::setAppId(const QString & appId)
{
    return QtWayland::xdg_surface::set_app_id(appId);
}

void QWaylandXdgSurface::raise()
{
    if (m_extendedWindow)
        m_extendedWindow->raise();
}

void QWaylandXdgSurface::lower()
{
    if (m_extendedWindow)
        m_extendedWindow->lower();
}

void QWaylandXdgSurface::setContentOrientationMask(Qt::ScreenOrientations orientation)
{
    if (m_extendedWindow)
        m_extendedWindow->setContentOrientationMask(orientation);
}

void QWaylandXdgSurface::setWindowFlags(Qt::WindowFlags flags)
{
    if (m_extendedWindow)
        m_extendedWindow->setWindowFlags(flags);
}

void QWaylandXdgSurface::sendProperty(const QString &name, const QVariant &value)
{
    if (m_extendedWindow)
        m_extendedWindow->updateGenericProperty(name, value);
}

void QWaylandXdgSurface::xdg_surface_configure(int32_t width, int32_t height, struct wl_array *states,uint32_t serial)
{
    uint32_t *state = reinterpret_cast<uint32_t*>(states->data);
    size_t numStates = states->size / sizeof(uint32_t);
    bool aboutToMaximize = false;
    bool aboutToFullScreen = false;

    for (size_t i = 0; i < numStates; i++) {
        switch (state[i]) {
        case XDG_SURFACE_STATE_MAXIMIZED:
            aboutToMaximize = ((width > 0) && (height > 0));
            break;
        case XDG_SURFACE_STATE_FULLSCREEN:
            aboutToFullScreen = true;
            break;
        case XDG_SURFACE_STATE_RESIZING:
            m_margins = m_window->frameMargins();
            width -= m_margins.left() + m_margins.right();
            height -= m_margins.top() + m_margins.bottom();
            m_size = m_window->window()->geometry().size();
            break;
        case XDG_SURFACE_STATE_ACTIVATED:
            // TODO: here about the missing window activation
            break;
        default:
            break;
        }
    }

    if (!m_fullscreen && aboutToFullScreen) {
        m_fullscreen = true;
        m_window->window()->showFullScreen();
    } else if (m_fullscreen && !aboutToFullScreen) {
        m_fullscreen = false;
        if ( m_maximized ) {
            m_window->window()->showMaximized();
        } else {
            m_window->window()->showNormal();
        }
    } else if (!m_maximized && aboutToMaximize) {
        m_maximized = true;
        m_window->window()->showMaximized();
    } else if (m_maximized && !aboutToMaximize) {
        m_maximized = false;
        m_window->window()->showNormal();
    }

    if (width == 0 || height == 0) {
        width = m_size.width();
        height = m_size.height();
    }

    if (width > 0 && height > 0) {
        m_margins = m_window->frameMargins();
        m_window->configure(0, width + m_margins.left() + m_margins.right(), height + m_margins.top() + m_margins.bottom());
    }

    ack_configure(serial);
}

void QWaylandXdgSurface::xdg_surface_close()
{
}

}

QT_END_NAMESPACE
