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

#include "qxcbscreen.h"

#include <stdio.h>

QXcbScreen::QXcbScreen(QXcbConnection *connection, xcb_screen_t *screen, int number)
    : QXcbObject(connection)
    , m_screen(screen)
    , m_number(number)
{
    printf ("\n");
    printf ("Information of screen %d:\n", screen->root);
    printf ("  width.........: %d\n", screen->width_in_pixels);
    printf ("  height........: %d\n", screen->height_in_pixels);
    printf ("  depth.........: %d\n", screen->root_depth);
    printf ("  white pixel...: %x\n", screen->white_pixel);
    printf ("  black pixel...: %x\n", screen->black_pixel);
    printf ("\n");

    const quint32 mask = XCB_CW_EVENT_MASK;
    const quint32 values[] = {
        // XCB_CW_EVENT_MASK
        XCB_EVENT_MASK_ENTER_WINDOW
        | XCB_EVENT_MASK_LEAVE_WINDOW
        | XCB_EVENT_MASK_PROPERTY_CHANGE
    };

    xcb_change_window_attributes(xcb_connection(), screen->root, mask, values);

    xcb_generic_error_t *error;

    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(xcb_connection(),
            xcb_get_property(xcb_connection(), false, screen->root,
                             atom(QXcbAtom::_NET_SUPPORTING_WM_CHECK),
                             XCB_ATOM_WINDOW, 0, 1024), &error);

    if (reply && reply->format == 32 && reply->type == XCB_ATOM_WINDOW) {
        xcb_window_t windowManager = *((xcb_window_t *)xcb_get_property_value(reply));

        if (windowManager != XCB_WINDOW_NONE) {
            xcb_get_property_reply_t *windowManagerReply =
                xcb_get_property_reply(xcb_connection(),
                    xcb_get_property(xcb_connection(), false, windowManager,
                                     atom(QXcbAtom::_NET_WM_NAME),
                                     atom(QXcbAtom::UTF8_STRING), 0, 1024), &error);
            if (windowManagerReply && windowManagerReply->format == 8 && windowManagerReply->type == atom(QXcbAtom::UTF8_STRING)) {
                m_windowManagerName = QString::fromUtf8((const char *)xcb_get_property_value(windowManagerReply), xcb_get_property_value_length(windowManagerReply));
                printf("Running window manager: %s\n", qPrintable(m_windowManagerName));
            } else if (error) {
                connection->handleXcbError(error);
                free(error);
            }

            free(windowManagerReply);
        }
    } else if (error) {
        connection->handleXcbError(error);
        free(error);
    }

    free(reply);

    m_syncRequestSupported = m_windowManagerName != QLatin1String("KWin");
}

QXcbScreen::~QXcbScreen()
{
}

QRect QXcbScreen::geometry() const
{
    return QRect(0, 0, m_screen->width_in_pixels, m_screen->height_in_pixels);
}

int QXcbScreen::depth() const
{
    return m_screen->root_depth;
}

QImage::Format QXcbScreen::format() const
{
    return QImage::Format_RGB32;
}

QSize QXcbScreen::physicalSize() const
{
    return QSize(m_screen->width_in_millimeters, m_screen->height_in_millimeters);
}

int QXcbScreen::screenNumber() const
{
    return m_number;
}
