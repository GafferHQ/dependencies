/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 1.0
import "content"
import "content/tic-tac-toe.js" as Logic

Rectangle {
    id: game

    property bool running: true
    property real difficulty: 1.0   //chance it will actually think

    width: display.width; height: display.height + 10

    Image {
        id: boardImage
        source: "content/pics/board.png"
    }


    Column {
        id: display

        Grid {
            id: board
            width: boardImage.width; height: boardImage.height
            columns: 3

            Repeater {
                model: 9

                TicTac {
                    width: board.width/3
                    height: board.height/3

                    onClicked: {
                        if (game.running && Logic.canPlayAtPos(index)) {
                            if (!Logic.makeMove(index, "X"))
                                Logic.computerTurn();
                        }
                    }
                }
            }
        }

        Row {
            spacing: 4
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: "Hard"
                pressed: game.difficulty == 1.0
                onClicked: { game.difficulty = 1.0 }
            }
            Button {
                text: "Moderate"
                pressed: game.difficulty == 0.8
                onClicked: { game.difficulty = 0.8 }
            }
            Button {
                text: "Easy"
                pressed: game.difficulty == 0.2
                onClicked: { game.difficulty = 0.2 }
            }
        }
    }


    Text {
        id: messageDisplay
        anchors.centerIn: parent
        color: "blue"
        style: Text.Outline; styleColor: "white"
        font.pixelSize: 50; font.bold: true
        visible: false

        Timer {
            running: messageDisplay.visible
            onTriggered: {
                messageDisplay.visible = false;
                Logic.restartGame();
            }
        }
    }
}
