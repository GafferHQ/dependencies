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

Rectangle {

    property string qmlfile: ""
    height: parent.height *.95; width: parent.width *.95; anchors.centerIn: parent; radius: 5

    onQmlfileChanged: { qmlapp.source = qmlfile; if (qmlfile != "") { starttimer.start(); } }

    Loader {
        id: qmlapp
        property int statenum: 0
        property int statecount
        statecount: qmlfile != "" ? children[0].states.length : 0
        anchors.fill: parent; focus: true
        function advance() { statenum = statenum == statecount ? 1 : ++statenum; }
    }

    Timer { id: starttimer; interval: 500; onTriggered: { qmlapp.advance(); } }

    Rectangle {
        anchors { top: parent.top; right: parent.right; topMargin: 3; rightMargin: 3 }
        height: 30; width: 30; color: "red"; radius: 5
        Text { text: "X"; anchors.centerIn: parent; font.pointSize: 12 }
        MouseArea { anchors.fill: parent; onClicked: { elementsapp.qmlfiletoload = "" } }
    }

    Text { anchors.centerIn: parent; visible: qmlapp.status == Loader.Error; text: qmlfile+" failed to load.\n"; }

}
