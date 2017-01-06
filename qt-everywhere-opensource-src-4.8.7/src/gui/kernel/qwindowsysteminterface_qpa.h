/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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
#ifndef QWINDOWSYSTEMINTERFACE_H
#define QWINDOWSYSTEMINTERFACE_H

#include <QtCore/QTime>
#include <QtGui/qwindowdefs.h>
#include <QtCore/QEvent>
#include <QtGui/QWidget>
#include <QtCore/QWeakPointer>
#include <QtCore/QMutex>
#include <QtGui/QTouchEvent>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class Q_GUI_EXPORT QWindowSystemInterface
{
public:
    static void handleMouseEvent(QWidget *w, const QPoint & local, const QPoint & global, Qt::MouseButtons b);
    static void handleMouseEvent(QWidget *w, ulong timestamp, const QPoint & local, const QPoint & global, Qt::MouseButtons b);

    static void handleKeyEvent(QWidget *w, QEvent::Type t, int k, Qt::KeyboardModifiers mods, const QString & text = QString(), bool autorep = false, ushort count = 1);
    static void handleKeyEvent(QWidget *w, ulong timestamp, QEvent::Type t, int k, Qt::KeyboardModifiers mods, const QString & text = QString(), bool autorep = false, ushort count = 1);

    static void handleExtendedKeyEvent(QWidget *w, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                                       quint32 nativeScanCode, quint32 nativeVirtualKey,
                                       quint32 nativeModifiers,
                                       const QString& text = QString(), bool autorep = false,
                                       ushort count = 1);
    static void handleExtendedKeyEvent(QWidget *w, ulong timestamp, QEvent::Type type, int key, Qt::KeyboardModifiers modifiers,
                                       quint32 nativeScanCode, quint32 nativeVirtualKey,
                                       quint32 nativeModifiers,
                                       const QString& text = QString(), bool autorep = false,
                                       ushort count = 1);

    static void handleWheelEvent(QWidget *w, const QPoint & local, const QPoint & global, int d, Qt::Orientation o);
    static void handleWheelEvent(QWidget *w, ulong timestamp, const QPoint & local, const QPoint & global, int d, Qt::Orientation o);

    struct TouchPoint {
        int id;                 // for application use
        bool isPrimary;         // for application use
        QPointF normalPosition; // touch device coordinates, (0 to 1, 0 to 1)
        QRectF area;            // the touched area, centered at position in screen coordinates
        qreal pressure;         // 0 to 1
        Qt::TouchPointState state; //Qt::TouchPoint{Pressed|Moved|Stationary|Released}
    };

    static void handleTouchEvent(QWidget *w, QEvent::Type type, QTouchEvent::DeviceType devType, const QList<struct TouchPoint> &points);
    static void handleTouchEvent(QWidget *w, ulong timestamp, QEvent::Type type, QTouchEvent::DeviceType devType, const QList<struct TouchPoint> &points);

    static void handleGeometryChange(QWidget *w, const QRect &newRect);
    static void handleCloseEvent(QWidget *w);
    static void handleEnterEvent(QWidget *w);
    static void handleLeaveEvent(QWidget *w);
    static void handleWindowActivated(QWidget *w);
    static void handleWindowStateChanged(QWidget *w, Qt::WindowState newState);

    // Changes to the screen
    static void handleScreenGeometryChange(int screenIndex);
    static void handleScreenAvailableGeometryChange(int screenIndex);
    static void handleScreenCountChange(int count);

    static void handleLocaleChange();

    static void handlePlatformPanelEvent(QWidget *w);
};

QT_END_NAMESPACE
QT_END_HEADER
#endif // QWINDOWSYSTEMINTERFACE_H
