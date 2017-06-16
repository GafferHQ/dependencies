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

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import QtQuick 2.0 as QQ2

Entity {

    property Entity camera: Camera {
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 16/9
        nearPlane: 0.01
        farPlane: 1000.0
        position: Qt.vector3d( 10.0, 10.0, -25.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 10.0 )
    }

    // Shareable Components

    Mesh {
        id: ballMesh
        objectName: "ballMesh"
        source: "assets/ball.obj"
    }

    Mesh {
        id: cubeMesh
        objectName: "cubeMesh"
        source: "assets/cube.obj"
    }

    AnimatedDiffuseMaterial {
        id: animatedMaterial
        texture: Texture2D { TextureImage { source : "assets/gltf/wine/Wood_Cherry_Original_.jpg" } }
    }

    // Scene elements

    Entity {
        id : sceneEntity
        components : SceneLoader {
            id: scene
            source: "qrc:/assets/test_scene.dae"
            objectName: "dae_scene"
        }
    }

    RenderableEntity {
        mesh: ballMesh
        material: animatedMaterial
        transform: Transform {
            property real z: 25
            translation: Qt.vector3d( 0, -10, z )
            scale: 0.3
            QQ2.SequentialAnimation on z {
                running: true
                loops: QQ2.Animation.Infinite
                QQ2.NumberAnimation { to: -1000; duration: 5000 }
                QQ2.NumberAnimation { to: 1000; duration: 3000 }
            }
        }
    }

    RenderableEntity {
        mesh: cubeMesh
        material: animatedMaterial
        transform: Transform {
            property real x: 0
            translation: Qt.vector3d( x, -10, 25 )
            scale: 0.3
            QQ2.SequentialAnimation on x {
                running : true
                loops: QQ2.Animation.Infinite
                QQ2.NumberAnimation { to : -100; duration : 10000 }
                QQ2.NumberAnimation { to : 100; duration : 5000 }
            }
        }
    }
}
