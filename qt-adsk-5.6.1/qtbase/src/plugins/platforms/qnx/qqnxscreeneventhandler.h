/***************************************************************************
**
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#ifndef QQNXSCREENEVENTHANDLER_H
#define QQNXSCREENEVENTHANDLER_H

#include <qpa/qwindowsysteminterface.h>

#include <screen/screen.h>

QT_BEGIN_NAMESPACE

class QQnxIntegration;
class QQnxScreenEventFilter;
#if defined(QQNX_SCREENEVENTTHREAD)
class QQnxScreenEventThread;
#endif

class QQnxScreenEventHandler : public QObject
{
    Q_OBJECT
public:
    explicit QQnxScreenEventHandler(QQnxIntegration *integration);

    void addScreenEventFilter(QQnxScreenEventFilter *filter);
    void removeScreenEventFilter(QQnxScreenEventFilter *filter);

    bool handleEvent(screen_event_t event);
    bool handleEvent(screen_event_t event, int qnxType);

    static void injectKeyboardEvent(int flags, int sym, int mod, int scan, int cap);

#if defined(QQNX_SCREENEVENTTHREAD)
    void setScreenEventThread(QQnxScreenEventThread *eventThread);
#endif

Q_SIGNALS:
    void newWindowCreated(void *window);
    void windowClosed(void *window);

protected:
    void timerEvent(QTimerEvent *event);

#if defined(QQNX_SCREENEVENTTHREAD)
private Q_SLOTS:
    void processEventsFromScreenThread();
#endif

private:
    void handleKeyboardEvent(screen_event_t event);
    void handlePointerEvent(screen_event_t event);
    void handleTouchEvent(screen_event_t event, int qnxType);
    void handleCloseEvent(screen_event_t event);
    void handleCreateEvent(screen_event_t event);
    void handleDisplayEvent(screen_event_t event);
    void handlePropertyEvent(screen_event_t event);
    void handleKeyboardFocusPropertyEvent(screen_window_t window);

private:
    enum {
        MaximumTouchPoints = 10
    };

    QQnxIntegration *m_qnxIntegration;
    QPoint m_lastGlobalMousePoint;
    QPoint m_lastLocalMousePoint;
    Qt::MouseButtons m_lastButtonState;
    screen_window_t m_lastMouseWindow;
    QTouchDevice *m_touchDevice;
    QWindowSystemInterface::TouchPoint m_touchPoints[MaximumTouchPoints];
    QList<QQnxScreenEventFilter*> m_eventFilters;
#if defined(QQNX_SCREENEVENTTHREAD)
    QQnxScreenEventThread *m_eventThread;
#endif
    int m_focusLostTimer;
};

QT_END_NAMESPACE

#endif // QQNXSCREENEVENTHANDLER_H
