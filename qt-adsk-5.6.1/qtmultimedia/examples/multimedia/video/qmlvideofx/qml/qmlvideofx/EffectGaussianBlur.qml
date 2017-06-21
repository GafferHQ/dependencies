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

// Based on http://www.geeks3d.com/20100909/shader-library-gaussian-blur-post-processing-filter-in-glsl/

import QtQuick 2.0

Item {
    id: root
    property bool divider: true
    property real dividerValue: 0.5
    property ListModel parameters: ListModel {
        ListElement {
            name: "Radius"
            value: 0.5
        }
        onDataChanged: updateBlurSize()
    }

    function updateBlurSize()
    {
        if ((targetHeight > 0) && (targetWidth > 0))
        {
            verticalBlurSize = 4.0 * parameters.get(0).value / targetHeight;
            horizontalBlurSize = 4.0 * parameters.get(0).value / targetWidth;
        }
    }

    property alias targetWidth: verticalShader.targetWidth
    property alias targetHeight: verticalShader.targetHeight
    property alias source: verticalShader.source
    property alias horizontalBlurSize: horizontalShader.blurSize
    property alias verticalBlurSize: verticalShader.blurSize


    Effect {
        id: verticalShader
        anchors.fill:  parent
        dividerValue: parent.dividerValue
        property real blurSize: 0.0

        onTargetHeightChanged: {
            updateBlurSize()
        }
        onTargetWidthChanged: {
            updateBlurSize()
        }
        fragmentShaderFilename: "gaussianblur_v.fsh"
    }

    Effect {
        id: horizontalShader
        anchors.fill: parent
        dividerValue: parent.dividerValue
        property real blurSize: 0.0
        fragmentShaderFilename: "gaussianblur_h.fsh"
        source: horizontalShaderSource

        ShaderEffectSource {
            id: horizontalShaderSource
            sourceItem: verticalShader
            smooth: true
            hideSource: true
        }
    }
}

