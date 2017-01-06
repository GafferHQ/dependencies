/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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
import Qt.labs.folderlistmodel 1.0

Rectangle {
    id: root
    property bool showFocusHighlight: false
    property variant folders: folders1
    property variant view: view1
    width: 320
    height: 480
    color: palette.window

    FolderListModel {
        id: folders1
        nameFilters: [ "*.qml" ]
        folder: qmlViewerFolder
    }
    FolderListModel {
        id: folders2
        nameFilters: [ "*.qml" ]
        folder: qmlViewerFolder
    }

    SystemPalette { id: palette }

    function down(path) {
        if (folders == folders1) {
            view = view2
            folders = folders2;
            view1.state = "exitLeft";
        } else {
            view = view1
            folders = folders1;
            view2.state = "exitLeft";
        }
        view.x = root.width;
        view.state = "current";
        view.focus = true;
        folders.folder = path;
    }
    function up() {
        var path = folders.parentFolder;
        if (folders == folders1) {
            view = view2
            folders = folders2;
            view1.state = "exitRight";
        } else {
            view = view1
            folders = folders1;
            view2.state = "exitRight";
        }
        view.x = -root.width;
        view.state = "current";
        view.focus = true;
        folders.folder = path;
    }
    function keyPressed(key) {
        switch (key) {
            case Qt.Key_Up:
            case Qt.Key_Down:
            case Qt.Key_Left:
            case Qt.Key_Right:
                root.showFocusHighlight = true;
            break;
            default:
                // do nothing
            break;
        }
    }

    Component {
        id: folderDelegate
        Rectangle {
            id: wrapper
            function launch() {
                if (folders.isFolder(index)) {
                    down(filePath);
                } else {
                    qmlViewer.launch(filePath);
                }
            }
            width: root.width
            height: 52
            color: "transparent"
            Rectangle {
                id: highlight; visible: false
                anchors.fill: parent
                color: palette.highlight
                gradient: Gradient {
                    GradientStop { id: t1; position: 0.0; color: palette.highlight }
                    GradientStop { id: t2; position: 1.0; color: Qt.lighter(palette.highlight) }
                }
            }
            Item {
                width: 48; height: 48
                Image { source: "images/folder.png"; anchors.centerIn: parent; visible: folders.isFolder(index)}
            }
            Text {
                id: nameText
                anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                text: fileName
                anchors.leftMargin: 54
                font.pixelSize: 32
                color: (wrapper.ListView.isCurrentItem && root.showFocusHighlight) ? palette.highlightedText : palette.windowText
                elide: Text.ElideRight
            }
            MouseArea {
                id: mouseRegion
                anchors.fill: parent
                onPressed: {
                    root.showFocusHighlight = false;
                    wrapper.ListView.view.currentIndex = index;
                }
                onClicked: { if (folders == wrapper.ListView.view.model) launch() }
            }
            states: [
                State {
                    name: "pressed"
                    when: mouseRegion.pressed
                    PropertyChanges { target: highlight; visible: true }
                    PropertyChanges { target: nameText; color: palette.highlightedText }
                }
            ]
        }
    }

    ListView {
        id: view1
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        x: 0
        width: parent.width
        model: folders1
        delegate: folderDelegate
        highlight: Rectangle {
            color: palette.highlight
            visible: root.showFocusHighlight && view1.count != 0
            gradient: Gradient {
                GradientStop { id: t1; position: 0.0; color: palette.highlight }
                GradientStop { id: t2; position: 1.0; color: Qt.lighter(palette.highlight) }
            }
            width: view1.currentItem == null ? 0 : view1.currentItem.width
        }
        highlightMoveSpeed: 1000
        pressDelay: 100
        focus: true
        state: "current"
        states: [
            State {
                name: "current"
                PropertyChanges { target: view1; x: 0 }
            },
            State {
                name: "exitLeft"
                PropertyChanges { target: view1; x: -root.width }
            },
            State {
                name: "exitRight"
                PropertyChanges { target: view1; x: root.width }
            }
        ]
        transitions: [
            Transition {
                to: "current"
                SequentialAnimation {
                    NumberAnimation { properties: "x"; duration: 250 }
                }
            },
            Transition {
                NumberAnimation { properties: "x"; duration: 250 }
                NumberAnimation { properties: "x"; duration: 250 }
            }
        ]
        Keys.onPressed: root.keyPressed(event.key)
    }

    ListView {
        id: view2
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        x: parent.width
        width: parent.width
        model: folders2
        delegate: folderDelegate
        highlight: Rectangle {
            color: palette.highlight
            visible: root.showFocusHighlight && view2.count != 0
            gradient: Gradient {
                GradientStop { id: t1; position: 0.0; color: palette.highlight }
                GradientStop { id: t2; position: 1.0; color: Qt.lighter(palette.highlight) }
            }
            width: view1.currentItem == null ? 0 : view1.currentItem.width
        }
        highlightMoveSpeed: 1000
        pressDelay: 100
        states: [
            State {
                name: "current"
                PropertyChanges { target: view2; x: 0 }
            },
            State {
                name: "exitLeft"
                PropertyChanges { target: view2; x: -root.width }
            },
            State {
                name: "exitRight"
                PropertyChanges { target: view2; x: root.width }
            }
        ]
        transitions: [
            Transition {
                to: "current"
                SequentialAnimation {
                    NumberAnimation { properties: "x"; duration: 250 }
                }
            },
            Transition {
                NumberAnimation { properties: "x"; duration: 250 }
            }
        ]
        Keys.onPressed: root.keyPressed(event.key)
    }

    Keys.onPressed: {
        root.keyPressed(event.key);
        if (event.key == Qt.Key_Return || event.key == Qt.Key_Select || event.key == Qt.Key_Right) {
            view.currentItem.launch();
            event.accepted = true;
        } else if (event.key == Qt.Key_Left) {
            up();
        }
    }

    BorderImage {
        source: "images/titlebar.sci";
        width: parent.width;
        height: 52
        y: -7
        id: titleBar

        Rectangle {
            id: upButton
            width: 48
            height: titleBar.height - 7
            color: "transparent"

            Image { anchors.centerIn: parent; source: "images/up.png" }
            MouseArea { id: upRegion; anchors.centerIn: parent
                width: 56
                height: 56
                onClicked: if (folders.parentFolder != "") up()
            }
            states: [
                State {
                    name: "pressed"
                    when: upRegion.pressed
                    PropertyChanges { target: upButton; color: palette.highlight }
                }
            ]
        }
        Rectangle {
            color: "gray"
            x: 48
            width: 1
            height: 44
        }

        Text {
            anchors.left: upButton.right; anchors.right: parent.right; height: parent.height
            anchors.leftMargin: 4; anchors.rightMargin: 4
            text: folders.folder
            color: "white"
            elide: Text.ElideLeft; horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
            font.pixelSize: 32
        }
    }
}
