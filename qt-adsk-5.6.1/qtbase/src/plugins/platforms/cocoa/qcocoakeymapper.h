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

#ifndef QCOCOAKEYMAPPER_H
#define QCOCOAKEYMAPPER_H

#include <qcocoahelpers.h>

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include <QtCore/QList>
#include <QtGui/QKeyEvent>

QT_BEGIN_NAMESPACE

/*
    \internal
    A Mac KeyboardLayoutItem has 8 possible states:
        1. Unmodified
        2. Shift
        3. Control
        4. Control + Shift
        5. Alt
        6. Alt + Shift
        7. Alt + Control
        8. Alt + Control + Shift
        9. Meta
        10. Meta + Shift
        11. Meta + Control
        12. Meta + Control + Shift
        13. Meta + Alt
        14. Meta + Alt + Shift
        15. Meta + Alt + Control
        16. Meta + Alt + Control + Shift
*/
struct KeyboardLayoutItem {
    bool dirty;
    quint32 qtKey[16]; // Can by any Qt::Key_<foo>, or unicode character
};


class QCocoaKeyMapper
{
public:
    QCocoaKeyMapper();
    ~QCocoaKeyMapper();
    static Qt::KeyboardModifiers queryKeyboardModifiers();
    QList<int> possibleKeys(const QKeyEvent *event) const;
    bool updateKeyboard();
    void deleteLayouts();
    void updateKeyMap(unsigned short macVirtualKey, QChar unicodeKey);
    void clearMappings();

private:
    QCFType<TISInputSourceRef> currentInputSource;

    QLocale keyboardInputLocale;
    Qt::LayoutDirection keyboardInputDirection;
    enum { NullMode, UnicodeMode, OtherMode } keyboard_mode;
    union {
        const UCKeyboardLayout *unicode;
        void *other;
    } keyboard_layout_format;
    KeyboardLayoutKind keyboard_kind;
    UInt32 keyboard_dead;
    KeyboardLayoutItem *keyLayout[256];
};

QT_END_NAMESPACE

#endif

