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

import QtQuick 1.0

Flipable {
    id: flipable
    property int angle: 0

    width: 40;  height: 40
    transform: Rotation { origin.x: 20; origin.y: 20; axis.x: 1; axis.z: 0; angle: flipable.angle }

    front: Image {
        source: "pics/front.png"; width: 40; height: 40

        Image {
            anchors.centerIn: parent
            source: "pics/flag.png"; opacity: modelData.hasFlag

            Behavior on opacity { NumberAnimation {} }
        }
    }

    back: Image {
        source: "pics/back.png"
        width: 40; height: 40

        Text {
            anchors.centerIn: parent
            text: modelData.hint; color: "white"; font.bold: true
            opacity: !modelData.hasMine && modelData.hint > 0
        }

        Image {
            anchors.centerIn: parent
            source: "pics/bomb.png"; opacity: modelData.hasMine
        }

        Explosion { id: expl }
    }

    states: State {
        name: "back"; when: modelData.flipped
        PropertyChanges { target: flipable; angle: 180 }
    }

    property real pauseDur: 250
    transitions: Transition {
        SequentialAnimation {
            ScriptAction {
                script: {
                    var ret = Math.abs(flipable.x - field.clickx)
                        + Math.abs(flipable.y - field.clicky);
                    if (modelData.hasMine && modelData.flipped)
                        pauseDur = ret * 3
                    else
                        pauseDur = ret
                }
            }
            PauseAnimation {
                duration: pauseDur
            }
            RotationAnimation { easing.type: Easing.InOutQuad }
            ScriptAction { script: if (modelData.hasMine && modelData.flipped) { expl.explode = true } }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: {
            field.clickx = flipable.x
            field.clicky = flipable.y
            var row = Math.floor(index / 9)
            var col = index - (Math.floor(index / 9) * 9)
            if (mouse.button == undefined || mouse.button == Qt.RightButton) {
                flag(row, col)
            } else {
                flip(row, col)
            }
        }
        onPressAndHold: {
            field.clickx = flipable.x
            field.clicky = flipable.y
            var row = Math.floor(index / 9)
            var col = index - (Math.floor(index / 9) * 9)
            flag(row, col)
        }
    }
}
