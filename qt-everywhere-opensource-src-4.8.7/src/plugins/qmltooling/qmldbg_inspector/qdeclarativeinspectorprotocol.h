/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#ifndef QDECLARATIVEINSPECTORPROTOCOL_H
#define QDECLARATIVEINSPECTORPROTOCOL_H

#include <QtCore/QDebug>
#include <QtCore/QMetaType>
#include <QtCore/QMetaEnum>
#include <QtCore/QObject>

namespace QmlJSDebugger {

class InspectorProtocol : public QObject
{
    Q_OBJECT
    Q_ENUMS(Message Tool)

public:
    enum Message {
        AnimationSpeedChanged  = 0,
        AnimationPausedChanged = 19, // highest value
        ChangeTool             = 1,
        ClearComponentCache    = 2,
        ColorChanged           = 3,
        CreateObject           = 5,
        CurrentObjectsChanged  = 6,
        DestroyObject          = 7,
        MoveObject             = 8,
        ObjectIdList           = 9,
        Reload                 = 10,
        Reloaded               = 11,
        SetAnimationSpeed      = 12,
        SetAnimationPaused     = 18,
        SetCurrentObjects      = 14,
        SetDesignMode          = 15,
        ShowAppOnTop           = 16,
        ToolChanged            = 17
    };

    enum Tool {
        ColorPickerTool,
        SelectMarqueeTool,
        SelectTool,
        ZoomTool
    };

    static inline QString toString(Message message)
    {
        return QLatin1String(staticMetaObject.enumerator(0).valueToKey(message));
    }

    static inline QString toString(Tool tool)
    {
        return QLatin1String(staticMetaObject.enumerator(1).valueToKey(tool));
    }
};

inline QDataStream & operator<< (QDataStream &stream, InspectorProtocol::Message message)
{
    return stream << static_cast<quint32>(message);
}

inline QDataStream & operator>> (QDataStream &stream, InspectorProtocol::Message &message)
{
    quint32 i;
    stream >> i;
    message = static_cast<InspectorProtocol::Message>(i);
    return stream;
}

inline QDebug operator<< (QDebug dbg, InspectorProtocol::Message message)
{
    dbg << InspectorProtocol::toString(message);
    return dbg;
}

inline QDataStream & operator<< (QDataStream &stream, InspectorProtocol::Tool tool)
{
    return stream << static_cast<quint32>(tool);
}

inline QDataStream & operator>> (QDataStream &stream, InspectorProtocol::Tool &tool)
{
    quint32 i;
    stream >> i;
    tool = static_cast<InspectorProtocol::Tool>(i);
    return stream;
}

inline QDebug operator<< (QDebug dbg, InspectorProtocol::Tool tool)
{
    dbg << InspectorProtocol::toString(tool);
    return dbg;
}

} // namespace QmlJSDebugger

#endif // QDECLARATIVEINSPECTORPROTOCOL_H
