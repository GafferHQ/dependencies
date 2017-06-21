import QtQuick 2.5
import QtQuick.Window 2.2
import Qt.labs.controls 1.0

Window {
    visible: true

    Slider {
        id: slider
        objectName: "slider"
        from: 0
        to: 100
        value: 50
        stepSize: 1
        orientation: "Horizontal"
    }
}
