/****************************************************************************
**
** Copyright (C) 2015 Paul Lemire
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

Entity {
    id: root
    property Material visualMaterial;
    property real rotateAngle: 0.0
    property vector3d rotateAxis: Qt.vector3d(1.0, 0.0, 0.0)
    property vector3d center;
    property vector3d normal;
    readonly property vector4d equation: Qt.vector4d(normal.x,
                                                     normal.y,
                                                     normal.z,
                                                     -(normal.x * center.x +
                                                       normal.y * center.y +
                                                       normal.z * center.z))

    PlaneMesh {
        id: mesh
        width: 20.0
        height: 20.0
        meshResolution: Qt.size(2, 2)
    }

    Transform {
        id: transform
        translation: root.center
        rotation: fromAxisAndAngle(root.rotateAxis, root.rotateAngle)
    }

    property Layer layer: Layer {
        names: "visualization"
    }

    components: [visualMaterial, mesh, transform, layer]
}

