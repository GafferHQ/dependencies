import QtQuick 2.0

Rectangle {
    width: 400
    height: 400
    color: "gray"

    Flickable {
        id: flick
        objectName: "flick"
        anchors.fill: parent
        contentWidth: 800
        contentHeight: 800

        Rectangle {
            width: flick.contentWidth
            height: flick.contentHeight
            color: "red"
            Rectangle {
                width: 50; height: 50; color: "blue"
                anchors.centerIn: parent
            }
        }
    }
}
