/****************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion <blackberry-qt@qnx.com>
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

#include "qbbrootwindow.h"

#include "qbbscreen.h"

#include <QtCore/QUuid>

#if defined(QBBROOTWINDOW_DEBUG)
#include <QtCore/QDebug>
#endif

#include <errno.h>
#include <unistd.h>

static const int MAGIC_ZORDER_FOR_NO_NAV = 10;

QBBRootWindow::QBBRootWindow(QBBScreen *screen)
    : m_screen(screen),
      m_window(0),
      m_windowGroupName()
{
#if defined(QBBROOTWINDOW_DEBUG)
    qDebug() << Q_FUNC_INFO;
#endif
    // Create one top-level QNX window to act as a container for child windows
    // since navigator only supports one application window
    errno = 0;
    int result = screen_create_window(&m_window, m_screen->nativeContext());
    int val[2];
    if (result != 0) {
        qFatal("QBBRootWindow: failed to create window, errno=%d", errno);
    }

    // Move window to proper display
    errno = 0;
    screen_display_t display = m_screen->nativeDisplay();
    result = screen_set_window_property_pv(m_window, SCREEN_PROPERTY_DISPLAY, (void **)&display);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window display, errno=%d", errno);
    }

    // Make sure window is above navigator but below keyboard if running as root
    // since navigator won't automatically set our z-order in this case
    if (getuid() == 0) {
        errno = 0;
        val[0] = MAGIC_ZORDER_FOR_NO_NAV;
        result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_ZORDER, val);
        if (result != 0) {
            qFatal("QBBRootWindow: failed to set window z-order, errno=%d", errno);
        }
    }

    // Window won't be visible unless it has some buffers so make one dummy buffer that is 1x1
    errno = 0;
    val[0] = SCREEN_USAGE_NATIVE;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_USAGE, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window buffer usage, errno=%d", errno);
    }

    errno = 0;
    val[0] = m_screen->nativeFormat();
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_FORMAT, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window pixel format, errno=%d", errno);
    }

    errno = 0;
    val[0] = 1;
    val[1] = 1;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_BUFFER_SIZE, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window buffer size, errno=%d", errno);
    }

    errno = 0;
    result = screen_create_window_buffers(m_window, 1);
    if (result != 0) {
        qFatal("QBB: failed to create window buffer, errno=%d", errno);
    }

    // Window is always the size of the display
    errno = 0;
    QRect geometry = m_screen->geometry();
    val[0] = geometry.width();
    val[1] = geometry.height();
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SIZE, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window size, errno=%d", errno);
    }

    // Fill the window with solid black
    errno = 0;
    val[0] = 0;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_COLOR, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window colour, errno=%d", errno);
    }

    // Make the window opaque
    errno = 0;
    val[0] = SCREEN_TRANSPARENCY_NONE;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_TRANSPARENCY, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window transparency, errno=%d", errno);
    }

    // Set the swap interval to 1
    errno = 0;
    val[0] = 1;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SWAP_INTERVAL, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window swap interval, errno=%d", errno);
    }

    // Set viewport size equal to window size but move outside buffer so the fill colour is used exclusively
    errno = 0;
    val[0] = geometry.width();
    val[1] = geometry.height();
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SOURCE_SIZE, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window source size, errno=%d", errno);
    }

    errno = 0;
    val[0] = 1;
    val[1] = 0;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SOURCE_POSITION, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window source position, errno=%d", errno);
    }

    createWindowGroup();
    post();
}

QBBRootWindow::~QBBRootWindow()
{
    // Cleanup top-level QNX window
    screen_destroy_window(m_window);
}

void QBBRootWindow::post() const
{
#if defined(QBBROOTWINDOW_DEBUG)
    qDebug() << Q_FUNC_INFO;
#endif
    errno = 0;
    screen_buffer_t buffer;
    int result = screen_get_window_property_pv(m_window, SCREEN_PROPERTY_RENDER_BUFFERS, (void **)&buffer);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to query window buffer, errno=%d", errno);
    }

    errno = 0;
    int dirtyRect[] = {0, 0, 1, 1};
    result = screen_post_window(m_window, buffer, 1, dirtyRect, 0);
    if (result != 0) {
        qFatal("QBB: failed to post window buffer, errno=%d", errno);
    }
}

void QBBRootWindow::flush() const
{
#if defined(QBBROOTWINDOW_DEBUG)
    qDebug() << Q_FUNC_INFO;
#endif
    // Force immediate display update
    errno = 0;
    int result = screen_flush_context(m_screen->nativeContext(), 0);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to flush context, errno=%d", errno);
    }
}

void QBBRootWindow::setRotation(int rotation)
{
#if defined(QBBROOTWINDOW_DEBUG)
    qDebug() << Q_FUNC_INFO << "angle =" << rotation;
#endif
    errno = 0;
    int result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_ROTATION, &rotation);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window rotation, errno=%d", errno);
    }
}

void QBBRootWindow::resize(const QSize &size)
{
    errno = 0;
    int val[] = {size.width(), size.height()};
    int result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SIZE, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window size, errno=%d", errno);
    }

    errno = 0;
    result = screen_set_window_property_iv(m_window, SCREEN_PROPERTY_SOURCE_SIZE, val);
    if (result != 0) {
        qFatal("QBBRootWindow: failed to set window source size, errno=%d", errno);
    }

    // NOTE: display will update when child windows relayout and repaint
}

void QBBRootWindow::createWindowGroup()
{
    // Generate a random window group name
    m_windowGroupName = QUuid::createUuid().toString().toAscii();

    // Create window group so child windows can be parented by container window
    errno = 0;
    int result = screen_create_window_group(m_window, m_windowGroupName.constData());
    if (result != 0) {
        qFatal("QBBRootWindow: failed to create app window group, errno=%d", errno);
    }
}
