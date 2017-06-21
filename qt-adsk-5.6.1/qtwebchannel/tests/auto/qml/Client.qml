/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

import QtWebChannel 1.0
import QtWebChannel.Tests 1.0
import "qrc:///qtwebchannel/qwebchannel.js" as JSClient

Item {
    id: root

    TestTransport {
        id: serverTransport
    }
    readonly property var serverTransport: serverTransport

    property var clientMessages: []
    property var serverMessages: []

    property bool debug: false

    QtObject {
        id: clientTransport

        property var onmessage;

        function send(message)
        {
            if (debug)
                console.log("client", (root.objectName ? "(" + root.objectName + ")" : ""), "posts message: ", message, "is idle:", webChannel.clientIsIdle());
            clientMessages.push(JSON.parse(message));
            serverTransport.receiveMessage(message);
            if (message && message.type && message.type === JSClient.QWebChannelMessageTypes.idle)
                verify(webChannel.clientIsIdle());
        }

        Component.onCompleted: {
            serverTransport.sendMessageRequested.connect(function receive(message) {
                if (debug)
                    console.log("client", (root.objectName ? "(" + root.objectName + ")" : ""), "received message:", JSON.stringify(message));
                serverMessages.push(message);
                if (onmessage)
                    onmessage({data:message});
            });
        }
    }
    readonly property var clientTransport: clientTransport

    function createChannel(callback, raw)
    {
        return new JSClient.QWebChannel(clientTransport, callback, raw);
    }

    function cleanup()
    {
        clientMessages = [];
        serverMessages = [];
    }

    function awaitRawMessage(from)
    {
        var messages;
        if (!from || typeof from !== "string" || from == "client") {
            from = "client";
            messages = clientMessages;
        } else {
            from = "server";
            messages = serverMessages;
        }

        for (var i = 0; i < 10 && !messages.length; ++i)
            wait(10);

        var msg = messages.shift();
        if (debug) {
            console.log((root.objectName ? "(" + root.objectName + ")" : ""), "shifting message " + from + "[" + messages.length + "]" + ":" + JSON.stringify(msg));
        }
        return msg;
    }

    function awaitMessage(from)
    {
        var msg = awaitRawMessage(from)
        if (debug)
          console.log((root.objectName ? "(" + root.objectName + ")" : ""), "handling message: ", JSON.stringify(msg));
        if (!msg)
            return false;
        return msg;
    }

    function await(type, from, skip) {
        var msg;
        do {
            msg = awaitMessage(from);
            if (!msg) {
                console.trace();
                verify(msg);
            }
        } while (skip && (msg.type === JSClient.QWebChannelMessageTypes.idle));
        if (type !== null) {
            if (!msg || msg.type != type)
                console.trace();
            verify(msg);
            compare(msg.type, type);
        }
        return msg;
    }

    function awaitInit() {
        return await(JSClient.QWebChannelMessageTypes.init);
    }

    function awaitIdle() {
        return await(JSClient.QWebChannelMessageTypes.idle);
    }

    function awaitMessageSkipIdle() {
        return awaitFunc(null, null, true);
    }

    function awaitServerInit() {
        return await(JSClient.QWebChannelMessageTypes.init, "server");
    }

    function awaitSignal()
    {
        return await(JSClient.QWebChannelMessageTypes.signal, "server");
    }

    function awaitPropertyUpdate()
    {
        return await(JSClient.QWebChannelMessageTypes.propertyUpdate, "server");
    }

    function awaitResponse()
    {
        return await(JSClient.QWebChannelMessageTypes.response, "server");
    }

    function skipToMessage(type, from, max) {
        do {
            var msg = awaitMessage(from);
            if (msg && msg.type === type)
                return msg
        } while (--max > 0);
        return false;
    }
}
