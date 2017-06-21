/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
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

import QtQuick 2.2

/*
    The test verifies that batching does not interfere with overlapping
    regions.

    #samples: 8
                 PixelPos     R    G    B    Error-tolerance
    #base:         0   0     1.0  0.0  0.0        0.05
    #base:         1   1     0.0  0.0  1.0        0.05
    #base:        10  10     1.0  0.0  0.0        0.05
    #base:         1  11     0.0  0.0  1.0        0.05

    #final:        0   0     1.0  0.0  0.0        0.05
    #final:        1   1     0.0  1.0  0.0        0.05
    #final:       10  10     1.0  0.0  0.0        0.05
    #final:        1  11     0.0  1.0  0.0        0.05

*/

RenderTestBase
{
    id: root

    opacity: 0.99

    Rectangle {
        width: 100
        height: 9
        color: Qt.rgba(1, 0, 0);

        Rectangle {
            id: box
            width: 5
            height: 5
            x: 1
            y: 1
            color: Qt.rgba(0, 0, 1);
        }
    }

    ShaderEffect { // Something which blends and is different than rectangle. Will get its own batch
        width: 100
        height: 9
        y: 10
        fragmentShader: "varying highp vec2 qt_TexCoord0; void main() { gl_FragColor = vec4(1, 0, 0, 1); }"

        Rectangle {
            width: 5
            height: 5
            x: 1
            y: 1
            color: box.color
        }
    }

    onEnterFinalStage: {
        box.color = Qt.rgba(0, 1, 0);
        root.finalStageComplete = true;
    }

}
