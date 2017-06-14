/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.Controls 1.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.2

Window {
    id: canvasWindow
    width: 400
    height: 600
    visible: true

    property alias canvasArea: canvasArea
    property var managerObject: null
    property var canvas: null
    property var previousCanvas: null

    function setManager(manager) {
        managerObject = manager;
    }

    onClosing: {
       managerObject.removeWindow(canvasWindow)
    }

    ColumnLayout {
        spacing: 1
        anchors.fill: parent
        visible: true

        Item {
            id: canvasArea
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredHeight: 400

            onChildrenChanged: {
                if (children.length === 0) {
                    previousCanvas = canvas
                    canvas = null
                }
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "New QuickTexture canvas"
            onClicked: {
                previousCanvas = canvas
                if (canvas)
                    canvas.parent = null
                canvas = managerObject.createItemTextureCanvas(canvasArea)
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "New framebuffer canvas"
            onClicked: {
                previousCanvas = canvas
                if (canvas)
                    canvas.parent = null
                canvas = managerObject.createFboCanvas(canvasArea)
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Grab random canvas"
            onClicked: {
                previousCanvas = canvas
                if (canvas)
                    canvas.parent = null
                canvas = chooseRandomCanvas()
                if (canvas)
                    canvas.parent = canvasArea
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Grab previous canvas"
            onClicked: {
                var previous = previousCanvas
                previousCanvas = canvas
                if (canvas)
                    canvas.parent = null
                canvas = previous
                if (canvas)
                    canvas.parent = canvasArea
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Delete canvas"
            onClicked: {
                managerObject.deleteCanvas(canvas)
            }
        }

        Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            text: "Release canvas"
            onClicked: {
                if (canvas)
                    canvas.parent = null
            }
        }
    }
}
