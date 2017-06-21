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
    id: sequentialanimationelementtest
    anchors.fill: parent
    property string testtext: ""
    property int firstduration: 1000
    property int secondduration: 3000
    property int firstY
    firstY: parent.height * .6
    property int secondY
    secondY: parent.height * .8

    Timer { id: startanimationtimer; interval: 1000; onTriggered: sequentialanimationelement.start() }

    SequentialAnimation {
        id: sequentialanimationelement
        running: false
        NumberAnimation { id: movement; target: animatedrect; properties: "y"; to: secondY; duration: firstduration }
        ColorAnimation { id: recolor; target: animatedrect; properties: "color"; to: "green"; duration: secondduration }
    }

    Rectangle {
        id: animatedrect
        width: 50; height: 50; color: "blue"; y: firstY
        anchors.horizontalCenter: parent.horizontalCenter
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: animatedrect; color: "blue"; y: firstY }
            PropertyChanges { target: sequentialanimationelementtest
                testtext: "This square will be animated in a sequence.\n"+
                "The next step will see it move quickly down the display, then slowly change its color to green";
            }
        },
        State { name: "firstchange"; when: statenum == 2
            StateChangeScript { script: { firstduration = 1000; secondduration = 3000; startanimationtimer.start() } }
            PropertyChanges { target: sequentialanimationelementtest
                testtext: "The square should have moved quickly and then recolored slowly\n"+
                "Next, it will move slowly and recolor to blue, in sequence."
            }
        },
        State { name: "secondchange"; when: statenum == 3
            StateChangeScript { script: { firstduration = 3000; secondduration = 1000; startanimationtimer.start() } }
            PropertyChanges { target: movement; to: firstY }
            PropertyChanges { target: recolor; to: "blue" }
            PropertyChanges { target: sequentialanimationelementtest
                testtext: "The square should have moved slowly and then recolored quickly\n"+
                "Advance to restart the test"
            }
        }
    ]
}
