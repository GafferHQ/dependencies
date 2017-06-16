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

#ifndef QXCBSYSTEMTRAYTRACKER_H
#define QXCBSYSTEMTRAYTRACKER_H

#include "qxcbconnection.h"

#include <xcb/xcb.h>

QT_BEGIN_NAMESPACE

class QXcbConnection;
class QScreen;

class QXcbSystemTrayTracker : public QObject, public QXcbWindowEventListener
{
    Q_OBJECT
public:
    static QXcbSystemTrayTracker *create(QXcbConnection *connection);

    xcb_window_t trayWindow();
    void requestSystemTrayWindowDock(xcb_window_t window) const;
    QRect systemTrayWindowGlobalGeometry(xcb_window_t window) const;

    void notifyManagerClientMessageEvent(const xcb_client_message_event_t *);

    void handleDestroyNotifyEvent(const xcb_destroy_notify_event_t *) Q_DECL_OVERRIDE;

    bool visualHasAlphaChannel();
signals:
    void systemTrayWindowChanged(QScreen *screen);

private:
    explicit QXcbSystemTrayTracker(QXcbConnection *connection,
                                   xcb_atom_t trayAtom,
                                   xcb_atom_t selection);
    static xcb_window_t locateTrayWindow(const QXcbConnection *connection, xcb_atom_t selection);
    void emitSystemTrayWindowChanged();

    const xcb_atom_t m_selection;
    const xcb_atom_t m_trayAtom;
    QXcbConnection *m_connection;
    xcb_window_t m_trayWindow;
};

QT_END_NAMESPACE

#endif // QXCBSYSTEMTRAYTRACKER_H
