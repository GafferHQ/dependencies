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

Rectangle {
    id: gradientelement
    property real gradstop: 0.25
    gradient: Gradient {
         GradientStop { position: 0.0; color: "red" }
         GradientStop { position: gradstop; color: "yellow" }
         GradientStop { position: 1.0; color: "green" }
    }
    MouseArea { anchors.fill: parent; enabled: qmlfiletoload == ""; hoverEnabled: true
        onEntered: { helptext = "The gradient should show a trio of colors - red, yellow and green"+
        " - with a slow movement of the yellow up and down the view" }
        onExited: { helptext = "" }
    }
    // Animate the background gradient
    SequentialAnimation { id: gradanim; running: true; loops: Animation.Infinite
        NumberAnimation { target: gradientelement; property: "gradstop"; to: 0.88; duration: 10000; easing.type: Easing.InOutQuad }
        NumberAnimation { target: gradientelement; property: "gradstop"; to: 0.22; duration: 10000; easing.type: Easing.InOutQuad }
    }
}
