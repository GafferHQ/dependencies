/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtTest 1.0
import Qt.labs.controls 1.0

TestCase {
    id: testCase
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "ScrollBar"

    SignalSpy{
        id: pressedSpy
        signalName: "pressedChanged"
    }

    Component {
        id: scrollBar
        ScrollBar { padding: 0 }
    }

    Component {
        id: flickable
        Flickable {
            width: 100
            height: 100
            contentWidth: 200
            contentHeight: 200
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalAndVerticalFlick
        }
    }

    function init() {
        verify(!pressedSpy.target)
        compare(pressedSpy.count, 0)
    }

    function cleanup() {
        pressedSpy.target = null
        pressedSpy.clear()
    }

    function test_attach() {
        var container = flickable.createObject(testCase)
        verify(container)
        waitForRendering(container)

        var vertical = scrollBar.createObject()
        verify(!vertical.parent)
        compare(vertical.size, 0.0)
        compare(vertical.position, 0.0)
        compare(vertical.active, false)
        compare(vertical.orientation, Qt.Vertical)
        compare(vertical.x, 0)
        compare(vertical.y, 0)
        verify(vertical.width > 0)
        verify(vertical.height > 0)

        container.ScrollBar.vertical = vertical
        compare(vertical.parent, container)
        compare(vertical.orientation, Qt.Vertical)
        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(vertical.x, container.width - vertical.width)
        compare(vertical.y, 0)
        verify(vertical.width > 0)
        compare(vertical.height, container.height)

        var horizontal = scrollBar.createObject()
        verify(!horizontal.parent)
        compare(horizontal.size, 0.0)
        compare(horizontal.position, 0.0)
        compare(horizontal.active, false)
        compare(horizontal.orientation, Qt.Vertical)
        compare(horizontal.x, 0)
        compare(horizontal.y, 0)
        verify(horizontal.width > 0)
        verify(horizontal.height > 0)

        container.ScrollBar.horizontal = horizontal
        compare(horizontal.parent, container)
        compare(horizontal.orientation, Qt.Horizontal)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)
        compare(horizontal.x, 0)
        compare(horizontal.y, container.height - horizontal.height)
        compare(horizontal.width, container.width)
        verify(horizontal.height > 0)

        var velocity = container.maximumFlickVelocity

        compare(container.flicking, false)
        container.flick(-velocity, -velocity)
        compare(container.flicking, true)
        tryCompare(container, "flicking", false)

        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)

        compare(container.flicking, false)
        container.flick(velocity, velocity)
        compare(container.flicking, true)
        tryCompare(container, "flicking", false)

        compare(vertical.size, container.visibleArea.heightRatio)
        compare(vertical.position, container.visibleArea.yPosition)
        compare(horizontal.size, container.visibleArea.widthRatio)
        compare(horizontal.position, container.visibleArea.xPosition)

        container.destroy()
    }

    function test_mouse_data() {
        return [
            { tag: "horizontal", properties: { visible: true, orientation: Qt.Horizontal, width: testCase.width } },
            { tag: "vertical", properties: { visible: true, orientation: Qt.Vertical, height: testCase.height } }
        ]
    }

    function test_mouse(data) {
        var control = scrollBar.createObject(testCase, data.properties)
        verify(control)

        pressedSpy.target = control
        verify(pressedSpy.valid)

        mousePress(control, 0, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.position, 0.0)

        mouseMove(control, -control.width, -control.height, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        compare(control.position, 0.0)

        mouseMove(control, control.width * 0.5, control.height * 0.5, 0, Qt.LeftButton)
        compare(pressedSpy.count, 1)
        compare(control.pressed, true)
        verify(control.position, 0.5)

        mouseRelease(control, control.width * 0.5, control.height * 0.5, Qt.LeftButton)
        compare(pressedSpy.count, 2)
        compare(control.pressed, false)
        compare(control.position, 0.5)

        mousePress(control, control.width, control.height, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 0.5)

        mouseMove(control, control.width * 2, control.height * 2, 0, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 1.0)

        mouseMove(control, control.width * 0.75, control.height * 0.75, 0, Qt.LeftButton)
        compare(pressedSpy.count, 3)
        compare(control.pressed, true)
        compare(control.position, 0.75)

        mouseRelease(control, control.width * 0.25, control.height * 0.25, Qt.LeftButton)
        compare(pressedSpy.count, 4)
        compare(control.pressed, false)
        compare(control.position, 0.25)

        control.destroy()
    }

    function test_padding_data() {
        return [
            { tag: "horizontal", properties: { visible: true, orientation: Qt.Horizontal, width: testCase.width, leftPadding: testCase.width * 0.1 } },
            { tag: "vertical", properties: { visible: true, orientation: Qt.Vertical, height: testCase.height, topPadding: testCase.height * 0.1 } }
        ]
    }

    function test_padding(data) {
        var control = scrollBar.createObject(testCase, data.properties)

        mousePress(control, control.leftPadding + control.availableWidth * 0.5, control.topPadding + control.availableHeight * 0.5, Qt.LeftButton)
        mouseRelease(control, control.leftPadding + control.availableWidth * 0.5, control.topPadding + control.availableHeight * 0.5, Qt.LeftButton)

        compare(control.position, 0.5)
    }
}
