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
        id: stateMachine
        initialState: historyState1

        State {
            id: state1
            SignalTransition {
                id: st1
                targetState: state2
            }
        }

        State {
            id: state2
            initialState: historyState2
            HistoryState {
                id: historyState2
                defaultState: state21
            }
            State {
                id: state21
            }
        }

        HistoryState {
            id: historyState1
            defaultState: state1
        }
    }

    SignalSpy {
        id: state1SpyActive
        target: state1
        signalName: "activeChanged"
    }

    SignalSpy {
        id: state2SpyActive
        target: state2
        signalName: "activeChanged"
    }


    function test_historyStateAsInitialState()
    {
        stateMachine.start();
        tryCompare(stateMachine, "running", true);
        tryCompare(state1SpyActive, "count" , 1);
        tryCompare(state2SpyActive, "count" , 0);
        st1.invoke();
        tryCompare(state1SpyActive, "count" , 2);
        tryCompare(state2SpyActive, "count" , 1);
        tryCompare(state21, "active", true);
        tryCompare(state1, "active", false);
    }
}
