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

import QtQuick 2.2

/*
    This test verifies that items that go from being batched because
    of no overlap will be split into multiple batches because of an
    overlap and that no rendering errors occur as a result of this.

    #samples: 8
                 PixelPos     R    G    B    Error-tolerance
    #base:         0   0     0.0  0.0  0.0       0.0
    #base:        10  10     0.5  0.0  0.0       0.05
    #base:       100 100     0.0  0.0  0.0       0.0
    #base:       110 110     0.5  0.0  0.0       0.05
    #final:       40  40     0.0  0.0  0.0       0.0
    #final:       50  50     0.5  0.0  0.0       0.05
    #final:       60  60     0.0  0.0  0.0       0.0
    #final:       70  70     0.5  0.0  0.0       0.05
*/

RenderTestBase {
    id: root

    Rectangle {
        id: one
        x: 0
        y: x
        width: 80
        height: 80
        color: "black"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            color: "red"
            opacity: 0.5
        }
    }

    Rectangle {
        id: two
        x: 100
        y: x
        width: 80
        height: 80
        color: "black"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            color: "red"
            opacity: 0.5
        }
    }

    SequentialAnimation {
        id: animation
        ParallelAnimation {
            NumberAnimation { target: one; property: "x"; from: 0; to: 40; duration: 100 }
            NumberAnimation { target: two; property: "x"; from: 100; to: 60; duration: 100 }
        }
        PropertyAction { target: root; property: "finalStageComplete"; value: true; }
    }

    onEnterFinalStage: {
        animation.running = true;
    }

}
