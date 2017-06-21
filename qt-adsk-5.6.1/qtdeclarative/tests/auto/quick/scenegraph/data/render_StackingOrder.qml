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

    #samples: 3
                 PixelPos     R    G    B    Error-tolerance
    #final:       10  10     1.0  0.0  0.0       0.05
    #final:       10 110     0.0  1.0  0.0       0.05
    #final:       10 120     0.0  0.0  1.0       0.05
*/

RenderTestBase {
    Item {
        opacity: 0.99; // Just to trigger alpha batches
        Rectangle { color: "#ff0000";  x: 10;  y: 10;  width: 20;  height: 20; }
        Image { source: "logo-small.jpg";  x: 10;  y: 50;   width: 50; height: 51; }
        Rectangle { color: "#00ff00"; x: 10;  y: 100; width: 50; height: 50; }
        Rectangle { color: "#0000ff"; x: 10;  y: 120; width: 10; height: 10; }
    }
    onEnterFinalStage: finalStageComplete = true;
}
