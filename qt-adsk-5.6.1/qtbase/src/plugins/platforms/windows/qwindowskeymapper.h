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

#ifndef QWINDOWSKEYMAPPER_H
#define QWINDOWSKEYMAPPER_H

#include "qtwindows_additional.h"

#include <QtCore/QLocale>

QT_BEGIN_NAMESPACE

class QKeyEvent;
class QWindow;

/*
    \internal
    A Windows KeyboardLayoutItem has 8 possible states:
        1. Unmodified
        2. Shift
        3. Control
        4. Control + Shift
        5. Alt
        6. Alt + Shift
        7. Alt + Control
        8. Alt + Control + Shift
*/
struct KeyboardLayoutItem {
    uint dirty : 1;
    uint exists : 1; // whether this item has been initialized (by updatePossibleKeyCodes)
    quint8 deadkeys;
    static const size_t NumQtKeys = 9;
    quint32 qtKey[NumQtKeys]; // Can by any Qt::Key_<foo>, or unicode character
};

class QWindowsKeyMapper
{
    Q_DISABLE_COPY(QWindowsKeyMapper)
public:
    explicit QWindowsKeyMapper();
    ~QWindowsKeyMapper();

    void changeKeyboard();

    void setUseRTLExtensions(bool e) { m_useRTLExtensions = e; }
    bool useRTLExtensions() const    { return m_useRTLExtensions; }

    bool translateKeyEvent(QWindow *widget, HWND hwnd, const MSG &msg, LRESULT *result);

    QWindow *keyGrabber() const      { return m_keyGrabber; }
    void setKeyGrabber(QWindow *w)   { m_keyGrabber = w; }

    static Qt::KeyboardModifiers queryKeyboardModifiers();
    QList<int> possibleKeys(const QKeyEvent *e) const;

private:
    bool translateKeyEventInternal(QWindow *receiver, const MSG &msg, bool grab);
    bool translateMultimediaKeyEventInternal(QWindow *receiver, const MSG &msg);
    void updateKeyMap(const MSG &msg);

    bool m_useRTLExtensions;

    QLocale keyboardInputLocale;
    Qt::LayoutDirection keyboardInputDirection;

    void updatePossibleKeyCodes(unsigned char *kbdBuffer, quint32 scancode, quint32 vk_key);
    void deleteLayouts();

    QWindow *m_keyGrabber;
    static const size_t NumKeyboardLayoutItems = 256;
    KeyboardLayoutItem keyLayout[NumKeyboardLayoutItems];
};

enum WindowsNativeModifiers {
    ShiftLeft            = 0x00000001,
    ControlLeft          = 0x00000002,
    AltLeft              = 0x00000004,
    MetaLeft             = 0x00000008,
    ShiftRight           = 0x00000010,
    ControlRight         = 0x00000020,
    AltRight             = 0x00000040,
    MetaRight            = 0x00000080,
    CapsLock             = 0x00000100,
    NumLock              = 0x00000200,
    ScrollLock           = 0x00000400,
    ExtendedKey          = 0x01000000,

    // Convenience mappings
    ShiftAny             = 0x00000011,
    ControlAny           = 0x00000022,
    AltAny               = 0x00000044,
    MetaAny              = 0x00000088,
    LockAny              = 0x00000700
};

QT_END_NAMESPACE

#endif // QWINDOWSKEYMAPPER_H
