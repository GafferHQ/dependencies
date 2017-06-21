/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

import QtQuick 2.0

ListView {
    id: controlviewitem
    property string controlname: ""
    property variant controlvalue
    signal reset
    height: 40; width: 200; highlightRangeMode: ListView.StrictlyEnforceRange
    orientation: ListView.Horizontal; snapMode: ListView.SnapOneItem
    preferredHighlightBegin: 50; preferredHighlightEnd: 150;
    delegate: Rectangle { height: 40; width: 100; border.color: "black"
        Text { anchors.centerIn: parent; width: parent.width; text: model.name; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter } }
    header: headfoot
    footer: headfoot
    Component {
        id: headfoot
        Rectangle {
            MouseArea { id: resetbutton; anchors.fill: parent; onClicked: { reset(); } }
            color: "lightgray"; height: 40; width: 50; border.color: "gray"
            Text { id: headertext; anchors.centerIn: parent; wrapMode: Text.WrapAnywhere
                rotation: -40; text: controlviewitem.controlname; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
        }
    }
}