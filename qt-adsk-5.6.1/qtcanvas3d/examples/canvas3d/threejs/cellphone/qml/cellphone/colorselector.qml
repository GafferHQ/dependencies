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

Item {
    id: colorSelector
    property color selectedColor

    height: ((width - 4) * colorSelectorModel.count) + 4

    Rectangle {
        anchors.fill: parent
        color: "black"
        radius: width / 10
        opacity: 0.3
    }

    ListModel {
        id: colorSelectorModel
        ListElement { caseColor: "#eeeeee" }
        ListElement { caseColor: "#111111" }
        ListElement { caseColor: "#ffe400" }
        ListElement { caseColor: "#469835" }
        ListElement { caseColor: "#fa0000" }
    }

    GridView {
        id: colorSelectorGrid
        anchors.fill: colorSelector
        anchors.margins: 4
        model: colorSelectorModel
        interactive: false
        cellWidth: width
        cellHeight: cellWidth + 4
        delegate: Component {
            Rectangle {
                id: colorDelegate
                width: colorSelectorGrid.cellWidth
                height: colorSelectorGrid.cellWidth
                color: caseColor
                border.color: "lightgray"
                border.width: 2
                radius: width / 10
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        selectedColor = parent.color
                        colorDelegateAnimation.start()
                    }
                }
                NumberAnimation {
                    id: colorDelegateAnimation
                    running: false
                    loops: 1
                    target: colorDelegate
                    property: "opacity"
                    from: 0.1
                    to: 1.0
                    duration: 500
                }
            }
        }
    }
}

