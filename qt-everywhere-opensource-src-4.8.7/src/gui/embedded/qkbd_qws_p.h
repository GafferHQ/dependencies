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

#ifndef QWSKEYBOARD_P_H
#define QWSKEYBOARD_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QDataStream>

QT_BEGIN_NAMESPACE

namespace QWSKeyboard {
    const quint32 FileMagic = 0x514d4150; // 'QMAP'

    struct Mapping {
        quint16 keycode;
        quint16 unicode;
        quint32 qtcode;
        quint8 modifiers;
        quint8 flags;
        quint16 special;

    };

    enum Flags {
        IsDead     = 0x01,
        IsLetter   = 0x02,
        IsModifier = 0x04,
        IsSystem   = 0x08
    };

    enum System {
        SystemConsoleFirst    = 0x0100,
        SystemConsoleMask     = 0x007f,
        SystemConsoleLast     = 0x017f,
        SystemConsolePrevious = 0x0180,
        SystemConsoleNext     = 0x0181,
        SystemReboot          = 0x0200,
        SystemZap             = 0x0300
    };

    struct Composing {
        quint16 first;
        quint16 second;
        quint16 result;
    };

    enum Modifiers {
        ModPlain   = 0x00,
        ModShift   = 0x01,
        ModAltGr   = 0x02,
        ModControl = 0x04,
        ModAlt     = 0x08,
        ModShiftL  = 0x10,
        ModShiftR  = 0x20,
        ModCtrlL   = 0x40,
        ModCtrlR   = 0x80
        // ModCapsShift = 0x100, // not supported!
    };
};

#ifndef QT_NO_DATASTREAM
inline QDataStream &operator>>(QDataStream &ds, QWSKeyboard::Mapping &m)
{
    return ds >> m.keycode >> m.unicode >> m.qtcode >> m.modifiers >> m.flags >> m.special;
}

inline QDataStream &operator<<(QDataStream &ds, const QWSKeyboard::Mapping &m)
{
    return ds << m.keycode << m.unicode << m.qtcode << m.modifiers << m.flags << m.special;
}

inline QDataStream &operator>>(QDataStream &ds, QWSKeyboard::Composing &c)
{
    return ds >> c.first >> c.second >> c.result;
}

inline QDataStream &operator<<(QDataStream &ds, const QWSKeyboard::Composing &c)
{
    return ds << c.first << c.second << c.result;
}
#endif // QT_NO_DATASTREAM

QT_END_NAMESPACE

#endif // QWSKEYBOARD_H
