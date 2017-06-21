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

import QtQuick 2.1

Rectangle {
    id: root
    color: "transparent"
    radius: 5
    property alias value: grip.value
    property color fillColor: "#14aaff"
    property real gripSize: 40
    property real gripTolerance: 3.0
    property real increment: 0.1
    property bool enabled: true

    Rectangle {
        id: slider
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        height: 10
        color: "transparent"

        BorderImage {
           id: sliderbarimage
           source: "qrc:/images/Slider_bar.png"
           anchors { fill: parent; margins: 1 }
           border.right: 5
           border.left: 5
        }
        Rectangle {
            height: parent.height -2
            anchors.left: parent.left
            anchors.right: grip.horizontalCenter
            color: root.fillColor
            radius: 3
            border.width: 1
            border.color: Qt.darker(color, 1.3)
            opacity: 0.8
        }
        Rectangle {
            id: grip
            property real value: 0.5
            x: (value * parent.width) - width/2
            anchors.verticalCenter: parent.verticalCenter
            width: root.gripTolerance * root.gripSize
            height: width
            radius: width/2
            color: "transparent"

            Image {
                id: sliderhandleimage
                source: "qrc:/images/Slider_handle.png"
                anchors.centerIn: parent
            }

            MouseArea {
                id: mouseArea
                enabled: root.enabled
                anchors.fill:  parent
                drag {
                    target: grip
                    axis: Drag.XAxis
                    minimumX: -parent.width/2
                    maximumX: root.width - parent.width/2
                }
                onPositionChanged:  {
                    if (drag.active)
                        updatePosition()
                }
                onReleased: {
                    updatePosition()
                }
                function updatePosition() {
                    value = (grip.x + grip.width/2) / slider.width
                }
            }
        }

    }
}
