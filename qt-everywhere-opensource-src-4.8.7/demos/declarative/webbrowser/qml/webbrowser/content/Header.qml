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

Image {
    id: header

    property alias editUrl: urlInput.url
    property bool urlChanged: false

    source: "pics/titlebar-bg.png"; fillMode: Image.TileHorizontally

    x: webView.contentX < 0 ? -webView.contentX : webView.contentX > webView.contentWidth-webView.width
       ? -webView.contentX+webView.contentWidth-webView.width : 0
    y: {
        if (webView.progress < 1.0)
            return 0;
        else {
            webView.contentY < 0 ? -webView.contentY : webView.contentY > height ? -height : -webView.contentY
        }
    }
    Column {
        width: parent.width

        Item {
            width: parent.width; height: 20
            Text {
                anchors.centerIn: parent
                text: webView.title; font.pixelSize: 14; font.bold: true
                color: "white"; styleColor: "black"; style: Text.Sunken
            }
        }

        Item {
            width: parent.width; height: 40

            Button {
                id: backButton
                action: webView.back; image: "pics/go-previous-view.png"
                anchors { left: parent.left; bottom: parent.bottom }
            }

            Button {
                id: nextButton
                anchors.left: backButton.right
                action: webView.forward; image: "pics/go-next-view.png"
            }

            UrlInput {
                id: urlInput
                anchors { left: nextButton.right; right: reloadButton.left }
                image: "pics/display.png"
                onUrlEntered: {
                    webBrowser.urlString = url
                    webBrowser.focus = true
                    header.urlChanged = false
                }
                onUrlChanged: header.urlChanged = true
            }

            Button {
                id: reloadButton
                anchors { right: quitButton.left; rightMargin: 10 }
                action: webView.reload; image: "pics/view-refresh.png"
                visible: webView.progress == 1.0 && !header.urlChanged
            }
            Text {
                id: quitButton
                color: "white"
                style: Text.Sunken
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 18
                width: 60
                text: "Quit"
                MouseArea {
                    anchors.fill: parent
                    onClicked: Qt.quit()
                }
                Rectangle {
                    width: 1
                    y: 5
                    height: parent.height-10
                    anchors.right: parent.left
                    color: "darkgray"
                }
            }

            Button {
                id: stopButton
                anchors { right: quitButton.left; rightMargin: 10 }
                action: webView.stop; image: "pics/edit-delete.png"
                visible: webView.progress < 1.0 && !header.urlChanged
            }

            Button {
                id: goButton
                anchors { right: parent.right; rightMargin: 4 }
                onClicked: {
                    webBrowser.urlString = urlInput.url
                    webBrowser.focus = true
                    header.urlChanged = false
                }
                image: "pics/go-jump-locationbar.png"; visible: header.urlChanged
            }
        }
    }
}
