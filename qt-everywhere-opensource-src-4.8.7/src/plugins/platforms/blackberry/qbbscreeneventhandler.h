/****************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion <blackberry-qt@qnx.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QBBSCREENEVENTHANDLER_H
#define QBBSCREENEVENTHANDLER_H

#include <QWindowSystemInterface>

#include <screen/screen.h>

QT_BEGIN_NAMESPACE

class QBBIntegration;

class QBBScreenEventHandler : public QObject
{
    Q_OBJECT
public:
    explicit QBBScreenEventHandler(QBBIntegration *integration);

    static void injectKeyboardEvent(int flags, int sym, int mod, int scan, int cap);
    void injectPointerMoveEvent(int x, int y);

    bool handleEvent(screen_event_t event);
    bool handleEvent(screen_event_t event, int qnxType);

Q_SIGNALS:
    void newWindowCreated(screen_window_t window);
    void windowClosed(screen_window_t window);

private:
    void handleKeyboardEvent(screen_event_t event);
    void handlePointerEvent(screen_event_t event);
    void handleTouchEvent(screen_event_t event, int type);
    void handleCloseEvent(screen_event_t event);
    void handleCreateEvent(screen_event_t event);
    void handleDisplayEvent(screen_event_t event);

private:
    enum {
        MAX_TOUCH_POINTS = 10
    };

    QBBIntegration *mBBIntegration;
    QPoint mLastGlobalMousePoint;
    QPoint mLastLocalMousePoint;
    Qt::MouseButtons mLastButtonState;
    void* mLastMouseWindow;

    QWindowSystemInterface::TouchPoint mTouchPoints[MAX_TOUCH_POINTS];
};

QT_END_NAMESPACE

#endif // QBBSCREENEVENTHANDLER_H
