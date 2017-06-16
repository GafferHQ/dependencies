/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

// Mouse area which flashes to indicate its location on the screen

import QtQuick 2.0

MouseArea {
    property alias hintColor: hintRect.color
    property bool hintEnabled: true

    Rectangle {
        id: hintRect
        anchors.fill: parent
        color: "yellow"
        opacity: 0

        states: [
            State {
                name: "high"
                PropertyChanges {
                    target: hintRect
                    opacity: 0.8
                }
            },
            State {
                name: "low"
                PropertyChanges {
                    target: hintRect
                    opacity: 0.4
                }
            }
        ]

        transitions: [
            Transition {
                from: "low"
                to: "high"
                SequentialAnimation {
                    NumberAnimation {
                        properties: "opacity"
                        easing.type: Easing.InOutSine
                        duration: 500
                    }
                    ScriptAction { script: hintRect.state = "low" }
                }
            },
            Transition {
                from: "*"
                to: "low"
                SequentialAnimation {
                    NumberAnimation {
                        properties: "opacity"
                        easing.type: Easing.InOutSine
                        duration: 500
                    }
                    ScriptAction { script: hintRect.state = "high" }
                }
            },
            Transition {
                from: "*"
                to: "baseState"
                NumberAnimation {
                    properties: "opacity"
                    easing.type: Easing.InOutSine
                    duration: 500
                }
            }
        ]
    }

    onHintEnabledChanged: hintRect.state = hintEnabled ? "low" : "baseState"

    Component.onCompleted: hintRect.state = "low"
}
