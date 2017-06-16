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

import QtQuick 2.1

Rectangle {
    id: root
    property alias effect: effectLoader.item
    property alias gripSize: divider.gripSize
    property string effectSource
    property real volume: 0.5

    signal videoFramePainted

    Divider {
        id: divider
        visible: false
        z: 1.0
        onValueChanged: updateDivider()
    }

    ShaderEffectSource {
        id: theSource
        smooth: true
        hideSource: true
    }

    Loader {
        id: contentLoader
    }

    Loader {
        id: effectLoader
        source: effectSource
    }

    Connections {
        id: videoFramePaintedConnection
        onFramePainted: {
            if (performanceLoader.item)
               root.videoFramePainted()
        }
        ignoreUnknownSignals: true
    }

    onWidthChanged: {
        if (effectLoader.item)
            effectLoader.item.targetWidth = root.width
    }

    onHeightChanged: {
        if (effectLoader.item)
            effectLoader.item.targetHeight = root.height
    }

    onEffectSourceChanged: {
        effectLoader.source = effectSource
        effectLoader.item.parent = root
        effectLoader.item.targetWidth = root.width
        effectLoader.item.targetHeight = root.height
        updateSource()
        effectLoader.item.source = theSource
        divider.visible = effectLoader.item.divider
        updateDivider()
    }

    function init() {
        openImage("qrc:/images/qt-logo.png")
        root.effectSource = "EffectPassThrough.qml"
    }

    function updateDivider() {
        if (effectLoader.item && effectLoader.item.divider)
            effectLoader.item.dividerValue = divider.value
    }

    function updateSource() {
        if (contentLoader.item) {
            contentLoader.item.parent = root
            contentLoader.item.anchors.fill = root
            theSource.sourceItem = contentLoader.item
            if (effectLoader.item)
                effectLoader.item.anchors.fill = contentLoader.item
        }
    }

    function openImage(path) {
        stop()
        contentLoader.source = "ContentImage.qml"
        videoFramePaintedConnection.target = null
        contentLoader.item.source = path
        updateSource()
    }

    function openVideo(path) {
        stop()
        contentLoader.source = "ContentVideo.qml"
        videoFramePaintedConnection.target = contentLoader.item
        contentLoader.item.mediaSource = path
        contentLoader.item.volume = volume
        contentLoader.item.play()
        updateSource()
    }

    function openCamera() {
        stop()
        contentLoader.source = "ContentCamera.qml"
        videoFramePaintedConnection.target = contentLoader.item
        updateSource()
    }

    function stop() {
        if (contentLoader.source == "ContentVideo.qml")
            contentLoader.item.stop()
        theSource.sourceItem = null
    }
}
