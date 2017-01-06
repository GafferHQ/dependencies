/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

Item {
    id: main
    anchors.fill: parent

    Image {
        id: background
        anchors.fill: parent
        source: "images/bg.jpg"
    }

    DropShadowEffect {
        id: layer

        property real distance: 0.0

        width: photo.width
        height: photo.height
        sourceItem: photo
        color: "black"
        blur: distance / 10.0
        opacity: 1 - distance / 50.0

        Binding {
            target: layer
            property: "x"
            value: -0.4 * layer.distance
            when: !dragArea.pressed
        }
        Binding {
            target: layer
            property: "y"
            value: 0.9 * layer.distance
            when: !dragArea.pressed
        }

        SequentialAnimation on distance {
            id: animation
            running: true
            loops:  Animation.Infinite

            NumberAnimation { to: 30; duration: 2000 }
            PauseAnimation { duration: 500 }
            NumberAnimation { to: 0; duration: 2000 }
            PauseAnimation { duration: 500 }
        }
    }

    Image {
        id: photo
        anchors.fill: parent
        source: "images/drop_shadow.png"
        smooth: true
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent

        property int startX
        property int startY

        onPressed: {
            startX = mouseX
            startY = mouseY
        }

        onPositionChanged: {
            layer.x += mouseX - startX
            layer.y += mouseY - startY
            startX = mouseX
            startY = mouseY
        }
    }
}
