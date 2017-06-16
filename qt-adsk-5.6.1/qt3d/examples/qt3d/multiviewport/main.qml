/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
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
import Qt3D.Core 2.0
import Qt3D.Render 2.0

Entity {
    id: rootNode
    components: [quadViewportFrameGraph]

    QuadViewportFrameGraph {
        id: quadViewportFrameGraph
        topLeftCamera: cameraSet.cameras[0]
        topRightCamera: cameraSet.cameras[1]
        bottomLeftCamera: cameraSet.cameras[2]
        bottomRightCamera: cameraSet.cameras[3]
    }

    Entity {
        id: cameraSet
        property var cameras: [camera1, camera2, camera3, camera4]

        Timer {
            running: true
            interval: 10000
            repeat: true
            property int count: 0
            onTriggered: {
                quadViewportFrameGraph.topLeftCamera = cameraSet.cameras[count++ % 4];
                quadViewportFrameGraph.topRightCamera = cameraSet.cameras[count % 4];
                quadViewportFrameGraph.bottomLeftCamera = cameraSet.cameras[(count + 1) % 4];
                quadViewportFrameGraph.bottomRightCamera = cameraSet.cameras[(count + 2) % 4];
            }
        }

        CameraLens {
            id : cameraLens
            projectionType: CameraLens.PerspectiveProjection
            fieldOfView: 45
            aspectRatio: 16/9
            nearPlane : 0.01
            farPlane : 1000.0
        }

        SimpleCamera {
            id: camera1
            lens: cameraLens
            position: Qt.vector3d( 0.0, 0.0, -20.0 )
        }

        SimpleCamera {
            id: camera2
            lens: cameraLens
            position: Qt.vector3d( 0.0, 0.0, 20.0 )
            viewCenter: Qt.vector3d( -3.0, 0.0, 10.0 )
        }

        SimpleCamera {
            id: camera3
            lens: cameraLens
            position: Qt.vector3d( 0.0, 30.0, 30.0 )
            viewCenter: Qt.vector3d( -5.0, -20.0, -10.0 )
        }

        SimpleCamera {
            id: camera4
            lens: cameraLens
            position: Qt.vector3d( 0.0, 15.0, 20.0 )
            viewCenter: Qt.vector3d( 0.0, -15.0, -20.0 )
        }
    }

    Entity {
        id: sceneRoot
        property real rotationAngle: 0

        SequentialAnimation {
            running: true
            loops: Animation.Infinite
            NumberAnimation { target: sceneRoot; property: "rotationAngle"; to: 360; duration: 2000; }
        }

        Entity {
            components: [
                Transform {
                    rotation: fromAxisAndAngle(Qt.vector3d(0, 0, 1), -sceneRoot.rotationAngle)
                },
                SceneLoader {
                    source: "qrc:/assets/test_scene.dae"
                }
            ]
        }
    } // sceneRoot
} // rootNode
