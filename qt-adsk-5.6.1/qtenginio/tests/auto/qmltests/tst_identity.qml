/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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
import QtTest 1.0
import Enginio 1.0
import "config.js" as AppConfig

Item {
    id: root

    EnginioClient {
        id: enginio
        backendId: AppConfig.backendData.id
        serviceUrl: AppConfig.backendData.serviceUrl

        onError: {
            console.log("\n\n### ERROR")
            console.log(reply.errorString)
            reply.dumpDebugInfo()
            console.log("\n###\n")
        }
    }

    EnginioOAuth2Authentication {
        id: validIdentity
        user: "logintest"
        password: "logintest"
    }

    EnginioOAuth2Authentication {
        id: invalidIdentity
        user: "INVALID"
        password: "INVALID"
    }

    SignalSpy {
        id: sessionAuthenticatedSpy
        target: enginio
        signalName: "sessionAuthenticated"
    }

    SignalSpy {
        id: sessionAuthenticationErrorSpy
        target: enginio
        signalName: "sessionAuthenticationError"
    }

    TestCase {
        name: "EnginioClient: Assign an identity"

        function init() {
            enginio.identity = null
            sessionAuthenticatedSpy.clear()
            sessionAuthenticationErrorSpy.clear()
        }

        function cleanupTestCase() {
            init()
        }

        function test_assignValidIdentity() {
            verify(enginio.authenticationState !== Enginio.Authenticated)
            enginio.identity = validIdentity
            sessionAuthenticatedSpy.wait()
            compare(enginio.authenticationState, Enginio.Authenticated)

            // reassign the same
            enginio.identity = null
            tryCompare(enginio, "authenticationState", Enginio.NotAuthenticated)
            enginio.identity = validIdentity
            sessionAuthenticatedSpy.wait()
            compare(enginio.authenticationState, Enginio.Authenticated)
        }

        function test_assignInvalidIdentity() {
            verify(enginio.authenticationState !== Enginio.Authenticated)
            enginio.identity = invalidIdentity
            sessionAuthenticationErrorSpy.wait()
            verify(enginio.authenticationState !== Enginio.Authenticated)
        }
    }
}
