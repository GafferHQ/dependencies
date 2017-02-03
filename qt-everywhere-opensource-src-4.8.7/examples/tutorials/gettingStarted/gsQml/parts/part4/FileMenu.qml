/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

Rectangle {
    id: fileMenu

    //the menuName is accessible from outside this QML file
    property string menuName: "File"

    //generous amount of screen space that will allow the buttons to fit
    height: 480; width:1000

    color: "#6C646A" 
        gradient: Gradient{
                        GradientStop { position: 0.0; color: "#6C646A" }
                        GradientStop { position: 1.0; color: Qt.darker("#6A6D6A") }
        }

    //a sub-rectangle allows the flexibility of setting the row area 
    Rectangle{
        id:actionContainer

        //make this rectangle invisible
        color:"transparent"
        anchors.centerIn: parent

        //the height is a good proportion that creates more space at the top of the row of buttons
        width: parent.width; height: parent.height / 5

        Row{
            anchors.centerIn: parent
            spacing: parent.width/6
            Button{
                id: loadButton
                buttonColor: "lightgrey"
                label: "Load"
            }
            
            Button{
                buttonColor: "grey"
                id: saveButton
                label: "Save"
            }
            Button{
                id: exitButton
                label: "Exit"
                buttonColor: "darkgrey"

                //exit the application if the exitButton is clicked
                onButtonClick:{
                    Qt.quit()
                }
            }
        }
    }

}
