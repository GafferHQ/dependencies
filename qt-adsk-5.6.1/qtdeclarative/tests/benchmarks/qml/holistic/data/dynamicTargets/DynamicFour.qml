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

Item {
    id: root
    property int dynamicWidth: 100
    property int dynamicHeight: rect1.height + rect2.height
    property int widthSignaledProperty: 10
    property int heightSignaledProperty: 10

    Rectangle {
        id: rect1
        width: root.dynamicWidth + 20
        height: width + (5*3) - 8 + (width/9)
        color: "red"
        border.color: "black"
        border.width: 5
        radius: 10
    }

    Rectangle {
        id: rect2
        width: rect1.width - 50
        height: width + (5*4) - 6 + (width/3)
        color: "red"
        border.color: "black"
        border.width: 5
        radius: 10
    }

    onDynamicWidthChanged: {
        widthSignaledProperty = widthSignaledProperty + (6*5) - 2;
    }

    onDynamicHeightChanged: {
        heightSignaledProperty = widthSignaledProperty + heightSignaledProperty + (5*3) - 7;
    }
}
