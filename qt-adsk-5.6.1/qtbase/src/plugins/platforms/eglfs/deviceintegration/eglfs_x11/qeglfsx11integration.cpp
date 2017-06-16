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

#include "qeglfsx11integration.h"
#include <QThread>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

/* Make no mistake: This is not a replacement for the xcb platform plugin.
   This here is barely an extremely useful tool for developing eglfs itself because
   it allows to do so without any requirements for devices or drivers. */

QT_BEGIN_NAMESPACE

class EventReader : public QThread
{
public:
    EventReader(QEglFSX11Integration *integration)
        : m_integration(integration) { }

    void run();

private:
    QEglFSX11Integration *m_integration;
};

QAtomicInt running;

static Qt::MouseButtons translateMouseButtons(int s)
{
    Qt::MouseButtons ret = 0;
    if (s & XCB_BUTTON_MASK_1)
        ret |= Qt::LeftButton;
    if (s & XCB_BUTTON_MASK_2)
        ret |= Qt::MidButton;
    if (s & XCB_BUTTON_MASK_3)
        ret |= Qt::RightButton;
    return ret;
}

static Qt::MouseButton translateMouseButton(xcb_button_t s)
{
    switch (s) {
    case 1: return Qt::LeftButton;
    case 2: return Qt::MidButton;
    case 3: return Qt::RightButton;
    // Button values 4-7 were already handled as Wheel events, and won't occur here.
    case 8: return Qt::BackButton;      // Also known as Qt::ExtraButton1
    case 9: return Qt::ForwardButton;   // Also known as Qt::ExtraButton2
    case 10: return Qt::ExtraButton3;
    case 11: return Qt::ExtraButton4;
    case 12: return Qt::ExtraButton5;
    case 13: return Qt::ExtraButton6;
    case 14: return Qt::ExtraButton7;
    case 15: return Qt::ExtraButton8;
    case 16: return Qt::ExtraButton9;
    case 17: return Qt::ExtraButton10;
    case 18: return Qt::ExtraButton11;
    case 19: return Qt::ExtraButton12;
    case 20: return Qt::ExtraButton13;
    case 21: return Qt::ExtraButton14;
    case 22: return Qt::ExtraButton15;
    case 23: return Qt::ExtraButton16;
    case 24: return Qt::ExtraButton17;
    case 25: return Qt::ExtraButton18;
    case 26: return Qt::ExtraButton19;
    case 27: return Qt::ExtraButton20;
    case 28: return Qt::ExtraButton21;
    case 29: return Qt::ExtraButton22;
    case 30: return Qt::ExtraButton23;
    case 31: return Qt::ExtraButton24;
    default: return Qt::NoButton;
    }
}

void EventReader::run()
{
    Qt::MouseButtons buttons;

    xcb_generic_event_t *event;
    while (running.load() && (event = xcb_wait_for_event(m_integration->connection()))) {
        uint response_type = event->response_type & ~0x80;
        switch (response_type) {
        case XCB_BUTTON_PRESS: {
            xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
            QPoint p(press->event_x, press->event_y);
            buttons = (buttons & ~0x7) | translateMouseButtons(press->state);
            buttons |= translateMouseButton(press->detail);
            QWindowSystemInterface::handleMouseEvent(0, press->time, p, p, buttons);
            break;
            }
        case XCB_BUTTON_RELEASE: {
            xcb_button_release_event_t *release = (xcb_button_release_event_t *)event;
            QPoint p(release->event_x, release->event_y);
            buttons = (buttons & ~0x7) | translateMouseButtons(release->state);
            buttons &= ~translateMouseButton(release->detail);
            QWindowSystemInterface::handleMouseEvent(0, release->time, p, p, buttons);
            break;
            }
        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
            QPoint p(motion->event_x, motion->event_y);
            QWindowSystemInterface::handleMouseEvent(0, motion->time, p, p, buttons);
            break;
            }
        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t *client = (xcb_client_message_event_t *) event;
            const xcb_atom_t *atoms = m_integration->atoms();
            if (client->format == 32
                && client->type == atoms[Atoms::WM_PROTOCOLS]
                && client->data.data32[0] == atoms[Atoms::WM_DELETE_WINDOW]) {
                QWindow *window = m_integration->platformWindow() ? m_integration->platformWindow()->window() : 0;
                if (window)
                    QWindowSystemInterface::handleCloseEvent(window);
            }
            break;
            }
        default:
            break;
        }
    }
}

void QEglFSX11Integration::sendConnectionEvent(xcb_atom_t a)
{
    xcb_client_message_event_t event;
    memset(&event, 0, sizeof(event));

    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.sequence = 0;
    event.window = m_connectionEventListener;
    event.type = a;

    xcb_send_event(m_connection, false, m_connectionEventListener, XCB_EVENT_MASK_NO_EVENT, (const char *)&event);
    xcb_flush(m_connection);
}

#define DISPLAY ((Display *) m_display)

void QEglFSX11Integration::platformInit()
{
    m_display = XOpenDisplay(0);
    if (!m_display)
        qFatal("Could not open display");

    XSetEventQueueOwner(DISPLAY, XCBOwnsEventQueue);
    m_connection = XGetXCBConnection(DISPLAY);

    running.ref();

    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(m_connection));

    m_connectionEventListener = xcb_generate_id(m_connection);
    xcb_create_window(m_connection, XCB_COPY_FROM_PARENT,
                      m_connectionEventListener, it.data->root,
                      0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
                      it.data->root_visual, 0, 0);

    m_eventReader = new EventReader(this);
    m_eventReader->start();
}

void QEglFSX11Integration::platformDestroy()
{
    running.deref();

    sendConnectionEvent(XCB_ATOM_NONE);

    m_eventReader->wait();
    delete m_eventReader;
    m_eventReader = 0;

    XCloseDisplay(DISPLAY);
    m_display = 0;
    m_connection = 0;
}

EGLNativeDisplayType QEglFSX11Integration::platformDisplay() const
{
    return DISPLAY;
}

QSize QEglFSX11Integration::screenSize() const
{
    if (m_screenSize.isEmpty()) {
        QList<QByteArray> env = qgetenv("EGLFS_X11_SIZE").split('x');
        if (env.length() == 2) {
            m_screenSize = QSize(env.at(0).toInt(), env.at(1).toInt());
        } else {
            XWindowAttributes a;
            if (XGetWindowAttributes(DISPLAY, DefaultRootWindow(DISPLAY), &a))
                m_screenSize = QSize(a.width, a.height);
        }
    }
    return m_screenSize;
}

EGLNativeWindowType QEglFSX11Integration::createNativeWindow(QPlatformWindow *platformWindow,
                                                       const QSize &size,
                                                       const QSurfaceFormat &format)
{
    Q_UNUSED(format);

    m_platformWindow = platformWindow;

    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(m_connection));
    m_window = xcb_generate_id(m_connection);
    xcb_create_window(m_connection, XCB_COPY_FROM_PARENT, m_window, it.data->root,
                      0, 0, size.width(), size.height(), 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, it.data->root_visual,
                      0, 0);

    xcb_intern_atom_cookie_t cookies[Atoms::N_ATOMS];
    static const char *atomNames[Atoms::N_ATOMS] = {
        "_NET_WM_NAME",
        "UTF8_STRING",
        "WM_PROTOCOLS",
        "WM_DELETE_WINDOW",
        "_NET_WM_STATE",
        "_NET_WM_STATE_FULLSCREEN"
    };

    for (int i = 0; i < Atoms::N_ATOMS; ++i) {
        cookies[i] = xcb_intern_atom(m_connection, false, strlen(atomNames[i]), atomNames[i]);
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(m_connection, cookies[i], 0);
        m_atoms[i] = reply->atom;
        free(reply);
    }

    // Set window title
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_window,
                        m_atoms[Atoms::_NET_WM_NAME], m_atoms[Atoms::UTF8_STRING], 8, 5, "EGLFS");

    // Enable WM_DELETE_WINDOW
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_window,
                        m_atoms[Atoms::WM_PROTOCOLS], XCB_ATOM_ATOM, 32, 1, &m_atoms[Atoms::WM_DELETE_WINDOW]);

    // Go fullscreen.
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_window,
                        m_atoms[Atoms::_NET_WM_STATE], XCB_ATOM_ATOM, 32, 1, &m_atoms[Atoms::_NET_WM_STATE_FULLSCREEN]);

    xcb_map_window(m_connection, m_window);

    xcb_flush(m_connection);

    return m_window;
}

void QEglFSX11Integration::destroyNativeWindow(EGLNativeWindowType window)
{
    xcb_destroy_window(m_connection, window);
}

bool QEglFSX11Integration::hasCapability(QPlatformIntegration::Capability cap) const
{
    Q_UNUSED(cap);
    return false;
}

QT_END_NAMESPACE
