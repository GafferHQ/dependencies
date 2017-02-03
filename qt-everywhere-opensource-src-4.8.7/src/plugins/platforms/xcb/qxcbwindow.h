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

#ifndef QXCBWINDOW_H
#define QXCBWINDOW_H

#include <QtGui/QPlatformWindow>
#include <QtGui/QPlatformWindowFormat>
#include <QtGui/QImage>

#include <xcb/xcb.h>
#include <xcb/sync.h>

#include "qxcbobject.h"

class QXcbScreen;

class QXcbWindow : public QXcbObject, public QPlatformWindow
{
public:
    QXcbWindow(QWidget *tlw);
    ~QXcbWindow();

    void setGeometry(const QRect &rect);

    void setVisible(bool visible);
    Qt::WindowFlags setWindowFlags(Qt::WindowFlags flags);
    WId winId() const;
    void setParent(const QPlatformWindow *window);

    void setWindowTitle(const QString &title);
    void raise();
    void lower();

    void requestActivateWindow();

    QPlatformGLContext *glContext() const;

    xcb_window_t window() const { return m_window; }
    uint depth() const { return m_depth; }
    QImage::Format format() const { return m_format; }

    void handleExposeEvent(const xcb_expose_event_t *event);
    void handleClientMessageEvent(const xcb_client_message_event_t *event);
    void handleConfigureNotifyEvent(const xcb_configure_notify_event_t *event);
    void handleButtonPressEvent(const xcb_button_press_event_t *event);
    void handleButtonReleaseEvent(const xcb_button_release_event_t *event);
    void handleMotionNotifyEvent(const xcb_motion_notify_event_t *event);

    void handleEnterNotifyEvent(const xcb_enter_notify_event_t *event);
    void handleLeaveNotifyEvent(const xcb_leave_notify_event_t *event);
    void handleFocusInEvent(const xcb_focus_in_event_t *event);
    void handleFocusOutEvent(const xcb_focus_out_event_t *event);

    void handleMouseEvent(xcb_button_t detail, uint16_t state, xcb_timestamp_t time, const QPoint &local, const QPoint &global);

    void updateSyncRequestCounter();

private:
    void setNetWmWindowTypes(Qt::WindowFlags flags);

    QXcbScreen *m_screen;

    xcb_window_t m_window;
    QPlatformGLContext *m_context;

    uint m_depth;
    QImage::Format m_format;

    xcb_sync_int64_t m_syncValue;
    xcb_sync_counter_t m_syncCounter;

    bool m_hasReceivedSyncRequest;
};

#endif
