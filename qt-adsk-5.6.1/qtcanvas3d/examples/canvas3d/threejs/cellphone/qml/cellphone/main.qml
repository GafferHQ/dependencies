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

import QtQuick 2.6
import QtQuick.Window 2.2

Window {
    property int initialWidth: 800
    property int initialHeight: 600
    id: mainView
    width: initialWidth
    height: initialHeight
    visible: true
    title: "Interactive Mobile Phone Demo"
    color: "black"

    //! [0]
    Item {
        id: textureSource
        layer.enabled: true
        layer.smooth: true
        layer.textureMirroring: ShaderEffectSource.NoMirroring
        //! [0]
        anchors.centerIn: parent
        // The dimensions of the texture source item determine the actual OpenGL texture dimensions.
        // We want a fairly large texture, so that even the smallest text we use renders nicely.
        width: 512
        height: 1024
        // Hide texture source behind the canvas normally, so you can't interact with it.
        // It would also be possible to set visible property to false instead, but if the item
        // is hidden, some things don't update correctly. For example, the Flickable doesn't update
        // its content if it is not visible.
        z: -2
        // Specify transform to make the visual representation of the item match the size and
        // orientation of the ui presented in Canvas. If the window initial dimensions,
        // textureSource dimensions, or phone mesh dimensions or position are changed,
        // scaling needs to be adjusted accordingly.
        //! [2]
        transform: [
            Scale {
                origin.y: textureSource.height / 2
                origin.x: textureSource.width / 2
                yScale: 0.5 * mainView.height / mainView.initialHeight
                xScale: 0.5 * mainView.height / mainView.initialHeight
            }
        ]
        opacity: 0.0
        //! [2]

        // CellphoneApp implements the UI of the phone
        CellphoneApp {
            id: cellphoneApp
            anchors.fill: parent
            backgroundColor: "black"
            textColor: "white"
            onLocked: canvas3d.state = "running"
        }
    }

    // CellphoneCanvas displays the rotating phone and the color selector
    CellphoneCanvas {
        id: canvas3d
        anchors.fill:parent
        textureSource: textureSource
        onRotationStopped: {
            cellphoneApp.resetLockTimer()
            // Bring the UI to the foreground so that it can be interacted with
            textureSource.z = 1
        }
        onRotationStarted: {
            // Hide the texture source behind canvas to ensure UI cannot be
            // interacted while the phone is rotating.
            textureSource.z = -1
        }
    }

    ColorSelector {
        anchors.right: parent.right
        anchors.top: parent.top
        width: parent.width / 16

        onSelectedColorChanged: canvas3d.caseColor = selectedColor
    }

    // FPS display, initially hidden, clicking will show it
    FpsDisplay {
        anchors.left: parent.left
        anchors.top: parent.top
        width: 32
        height: 64
        fps: canvas3d.fps
    }
}
