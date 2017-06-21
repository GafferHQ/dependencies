/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite module of the Qt Toolkit.
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

import QtTest 1.1
import QtQml.StateMachine 1.0

TestCase {
    StateMachine {
        id: myStateMachine
        childMode: State.ParallelStates
        State {
            id: childState1
            childMode: State.ParallelStates
            State {
                id: childState11
            }
            State {
                id: childState12
            }
        }
        State {
            id: childState2
            initialState: childState21
            State {
                id: childState21
            }
            State {
                id: childState22
            }
        }
    }
    name: "nestedParallelMachineStates"

    function test_nestedInitalStates() {
        // uncomment me after vm problems are fixed.
        //            compare(myStateMachine.running, false);
        compare(childState1.active, false);
        compare(childState11.active, false);
        compare(childState12.active, false);
        compare(childState2.active, false);
        compare(childState21.active, false);
        compare(childState22.active, false);
        myStateMachine.start();
        tryCompare(myStateMachine, "running", true);
        tryCompare(childState1, "active", true);
        tryCompare(childState11, "active", true);
        tryCompare(childState12, "active", true);
        tryCompare(childState2, "active", true);
        tryCompare(childState21, "active", true);
        tryCompare(childState22, "active", false);
    }
}
