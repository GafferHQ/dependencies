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

import QtQuick 2.0

Rectangle {
    id: root
    color: "transparent"
    height: view.model.count * sliderHeight
    property color lineColor: "black"
    property real gripSize: 25
    property real spacing: 20
    property real sliderHeight: 60

    property ListModel model: ListModel { }

    anchors.topMargin: 10

    Component {
        id: editDelegate

        Rectangle {
            id: delegate
            width: parent.width
            height: root.sliderHeight
            color: "transparent"

            Text {
                id: text
                text: name
                color: "white"
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                }
                font.pixelSize: fontSize
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                width: 8 * fontSize
            }

            Slider {
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: text.right
                    leftMargin: 20
                    right: parent.right
                    rightMargin: 20
                }
                gripSize: root.gripSize
                value: model.value
                onValueChanged: view.model.setProperty(index, "value", value)
            }
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        model: root.model
        delegate: editDelegate
        interactive: false
    }
}
