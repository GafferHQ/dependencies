/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.0

Item {
    id: keyselementtest
    anchors.fill: parent
    property string testtext: ""
    property int sidelength: 1500
    focus: true

    Rectangle {
        id: keyselementbox
        color: "lightgray"; border.color: "gray"; radius: 5; clip: true; opacity: .7
        height: 250; width: parent.width *.8
        anchors.centerIn: parent
        focus: true
        Keys.enabled: true
        Keys.onLeftPressed: { if (statenum == 2) { advance(); } }
        Keys.onRightPressed: { if (statenum == 1) { advance(); } }
        Keys.onUpPressed: { if (statenum == 3) { advance(); } }
        Keys.onDownPressed: { if (statenum == 4) { advance(); } }
        Keys.onPressed: {
            if ((event.key == Qt.Key_Space) && (event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.AltModifier)) {
                if (statenum == 5) { advance(); }
            }
        }
        Rectangle { id: leftone; height: 50; width: 50; color: "green"; anchors{ left: parent.left; verticalCenter: parent.verticalCenter } }
        Rectangle { id: rightone; height: 50; width: 50; color: "green"; anchors{ right: parent.right; verticalCenter: parent.verticalCenter } }
        Rectangle { id: topone; height: 50; width: 50; color: "green"; anchors{ top: parent.top; horizontalCenter: parent.horizontalCenter } }
        Rectangle { id: bottomone; height: 50; width: 50; color: "green"; anchors{ bottom: parent.bottom; horizontalCenter: parent.horizontalCenter } }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: keyselementtest
                testtext: "This is a Keys element. At present there should be four squares\n"+
                "Next, please press the right button on the keypad/board" }
        },
        State { name: "right"; when: statenum == 2
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: rightone; color: "orange" }
            PropertyChanges { target: keyselementtest; testtext: "Good. Now press left." }
        },
        State { name: "left"; when: statenum == 3
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: leftone; color: "orange" }
            PropertyChanges { target: keyselementtest; testtext: "Press up." }
        },
        State { name: "up"; when: statenum == 4
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: topone; color: "orange" }
            PropertyChanges { target: keyselementtest; testtext: "And then press down" }
        },
        State { name: "down"; when: statenum == 5
            PropertyChanges { target: helpbubble; showadvance: false }
            PropertyChanges { target: bottomone; color: "orange" }
            PropertyChanges { target: keyselementtest; testtext: "Excellent. Now hold Ctrl+Alt and press Space" }
        },
        State { name: "modifiers"; when: statenum == 6
            PropertyChanges { target: helpbubble; showadvance: true }
            PropertyChanges { target: keyselementbox; color: "orange" }
            PropertyChanges { target: keyselementtest
                testtext: "Test has completed\n"+
                "Advance to restart the test." }
        }
    ]

}
