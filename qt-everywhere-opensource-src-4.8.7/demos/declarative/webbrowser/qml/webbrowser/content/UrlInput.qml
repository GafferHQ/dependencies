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

Item {
    id: container

    property alias image: bg.source
    property alias url: urlText.text

    signal urlEntered(string url)
    signal urlChanged

    width: parent.height; height: parent.height

    BorderImage {
        id: bg; rotation: 180
        x: 8; width: parent.width - 16; height: 30;
        anchors.verticalCenter: parent.verticalCenter
        border { left: 10; top: 10; right: 10; bottom: 10 }
    }

    Rectangle {
        anchors.bottom: bg.bottom
        x: 18; height: 4; color: "#63b1ed"
        width: (bg.width - 20) * webView.progress
        opacity: webView.progress == 1.0 ? 0.0 : 1.0
    }

    TextInput {
        id: urlText
        horizontalAlignment: TextEdit.AlignLeft
        font.pixelSize: 14;

        onTextChanged: container.urlChanged()

        Keys.onEscapePressed: {
            urlText.text = webView.url
            webView.focus = true
        }

        Keys.onEnterPressed: {
            container.urlEntered(urlText.text)
            webView.focus = true
        }

        Keys.onReturnPressed: {
            container.urlEntered(urlText.text)
            webView.focus = true
        }

        anchors {
            left: parent.left; right: parent.right; leftMargin: 18; rightMargin: 18
            verticalCenter: parent.verticalCenter
        }
    }
}
