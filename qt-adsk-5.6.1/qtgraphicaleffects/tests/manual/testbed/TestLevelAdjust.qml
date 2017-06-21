/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Graphical Effects module.
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

import QtQuick 2.0
import "../../../src/effects"

TestCaseTemplate {
    ImageSource {
        id: imageSource
        source: "images/butterfly.png"
    }

    LevelAdjust {
        id: effect
        anchors.fill: imageSource
        visible: enabledCheckBox.selected
        cached: cachedCheckBox.selected
        source: imageSource

        minimumInput: Qt.rgba(redInput.blackPointValue + valueInput.blackPointValue * (redInput.whitePointValue - redInput.blackPointValue), greenInput.blackPointValue + valueInput.blackPointValue * (greenInput.whitePointValue - greenInput.blackPointValue), blueInput.blackPointValue + valueInput.blackPointValue * (blueInput.whitePointValue - blueInput.blackPointValue), alphaInput.blackPointValue)
        maximumInput: Qt.rgba(redInput.whitePointValue - (1.0 - valueInput.whitePointValue) * (redInput.whitePointValue - redInput.blackPointValue), greenInput.whitePointValue - (1.0 - valueInput.whitePointValue) * (greenInput.whitePointValue - greenInput.blackPointValue), blueInput.whitePointValue - (1.0 - valueInput.whitePointValue) * (blueInput.whitePointValue - blueInput.blackPointValue), alphaInput.whitePointValue)
        minimumOutput: Qt.rgba(redOutput.blackPointValue + valueOutput.blackPointValue * (redOutput.whitePointValue - redOutput.blackPointValue), greenOutput.blackPointValue + valueOutput.blackPointValue * (greenOutput.whitePointValue - greenOutput.blackPointValue), blueOutput.blackPointValue + valueOutput.blackPointValue * (blueOutput.whitePointValue - blueOutput.blackPointValue), alphaOutput.blackPointValue)
        maximumOutput: Qt.rgba(redOutput.whitePointValue - (1.0 - valueOutput.whitePointValue) * (redOutput.whitePointValue - redOutput.blackPointValue), greenOutput.whitePointValue - (1.0 - valueOutput.whitePointValue) * (greenOutput.whitePointValue - greenOutput.blackPointValue), blueOutput.whitePointValue - (1.0 - valueOutput.whitePointValue) * (blueOutput.whitePointValue - blueOutput.blackPointValue), alphaOutput.whitePointValue)

        gamma: Qt.vector3d((redInput.gamma * valueInput.gamma), (greenInput.gamma * valueInput.gamma), (blueInput.gamma * valueInput.gamma))
    }

    bgColor: bgColorPicker.color
    controls: [
        Control {
            caption: "RGB"
            LevelSlider {
                id: valueInput
                caption: "Input"
            }
            LevelSlider {
                id: valueOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Red"
            __hide: true
            LevelSlider {
                id: redInput
                caption: "Input"
            }
            LevelSlider {
                id: redOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Green"
            __hide: true
            LevelSlider {
                id: greenInput
                caption: "Input"
            }
            LevelSlider {
                id: greenOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Blue"
            __hide: true
            LevelSlider {
                id: blueInput
                caption: "Input"
            }
            LevelSlider {
                id: blueOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "Alpha"
            __hide: true
            LevelSlider {
                id: alphaInput
                showMidPoint: false
                caption: "Input"
            }
            LevelSlider {
                id: alphaOutput
                showMidPoint: false
                caption: "Output"
            }
        },
        Control {
            caption: "advanced"
            last: true
            Label {
                caption: "Effect size"
                text: effect.width + "x" + effect.height
            }
            Label {
                caption: "FPS"
                text: fps
            }
            CheckBox {
                id: cachedCheckBox
                caption: "cached"
            }
            CheckBox {
                id: enabledCheckBox
                caption: "enabled"
            }
            CheckBox {
                id: updateCheckBox
                caption: "animated"
                selected: false
            }
            BGColorPicker {
                id: bgColorPicker
            }
        }
    ]
}
