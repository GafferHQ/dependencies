/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins module of the Qt Toolkit.
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

#include "qlibinputpointer_p.h"
#include <libinput.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <qpa/qwindowsysteminterface.h>
#include <private/qhighdpiscaling_p.h>

QT_BEGIN_NAMESPACE

QLibInputPointer::QLibInputPointer()
    : m_buttons(Qt::NoButton)
{
}

void QLibInputPointer::processButton(libinput_event_pointer *e)
{
    const uint32_t b = libinput_event_pointer_get_button(e);
    const bool pressed = libinput_event_pointer_get_button_state(e) == LIBINPUT_BUTTON_STATE_PRESSED;

    Qt::MouseButton button = Qt::NoButton;
    switch (b) {
    case 0x110: button = Qt::LeftButton; break;    // BTN_LEFT
    case 0x111: button = Qt::RightButton; break;
    case 0x112: button = Qt::MiddleButton; break;
    case 0x113: button = Qt::ExtraButton1; break;  // AKA Qt::BackButton
    case 0x114: button = Qt::ExtraButton2; break;  // AKA Qt::ForwardButton
    case 0x115: button = Qt::ExtraButton3; break;  // AKA Qt::TaskButton
    case 0x116: button = Qt::ExtraButton4; break;
    case 0x117: button = Qt::ExtraButton5; break;
    case 0x118: button = Qt::ExtraButton6; break;
    case 0x119: button = Qt::ExtraButton7; break;
    case 0x11a: button = Qt::ExtraButton8; break;
    case 0x11b: button = Qt::ExtraButton9; break;
    case 0x11c: button = Qt::ExtraButton10; break;
    case 0x11d: button = Qt::ExtraButton11; break;
    case 0x11e: button = Qt::ExtraButton12; break;
    case 0x11f: button = Qt::ExtraButton13; break;
    }

    if (pressed)
        m_buttons |= button;
    else
        m_buttons &= ~button;

    QWindowSystemInterface::handleMouseEvent(Q_NULLPTR, m_pos, m_pos, m_buttons, QGuiApplication::keyboardModifiers());
}

void QLibInputPointer::processMotion(libinput_event_pointer *e)
{
    const double dx = libinput_event_pointer_get_dx(e);
    const double dy = libinput_event_pointer_get_dy(e);
    QScreen * const primaryScreen = QGuiApplication::primaryScreen();
    const QRect g = QHighDpi::toNativePixels(primaryScreen->virtualGeometry(), primaryScreen);

    m_pos.setX(qBound(g.left(), qRound(m_pos.x() + dx), g.right()));
    m_pos.setY(qBound(g.top(), qRound(m_pos.y() + dy), g.bottom()));

    QWindowSystemInterface::handleMouseEvent(Q_NULLPTR, m_pos, m_pos, m_buttons, QGuiApplication::keyboardModifiers());
}

void QLibInputPointer::processAxis(libinput_event_pointer *e)
{
#if QT_LIBINPUT_VERSION_MAJOR == 0 && QT_LIBINPUT_VERSION_MINOR <= 7
    const double v = libinput_event_pointer_get_axis_value(e) * 120;
    const Qt::Orientation ori = libinput_event_pointer_get_axis(e) == LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL
        ? Qt::Vertical : Qt::Horizontal;
    QWindowSystemInterface::handleWheelEvent(Q_NULLPTR, m_pos, m_pos, qRound(-v), ori, QGuiApplication::keyboardModifiers());
#else
    if (libinput_event_pointer_has_axis(e, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        const double v = libinput_event_pointer_get_axis_value(e, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL) * 120;
        QWindowSystemInterface::handleWheelEvent(Q_NULLPTR, m_pos, m_pos, qRound(-v), Qt::Vertical, QGuiApplication::keyboardModifiers());
    }
    if (libinput_event_pointer_has_axis(e, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        const double v = libinput_event_pointer_get_axis_value(e, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL) * 120;
        QWindowSystemInterface::handleWheelEvent(Q_NULLPTR, m_pos, m_pos, qRound(-v), Qt::Horizontal, QGuiApplication::keyboardModifiers());
    }
#endif
}

void QLibInputPointer::setPos(const QPoint &pos)
{
    QScreen * const primaryScreen = QGuiApplication::primaryScreen();
    const QRect g = QHighDpi::toNativePixels(primaryScreen->virtualGeometry(), primaryScreen);

    m_pos.setX(qBound(g.left(), pos.x(), g.right()));
    m_pos.setY(qBound(g.top(), pos.y(), g.bottom()));
}

QT_END_NAMESPACE
