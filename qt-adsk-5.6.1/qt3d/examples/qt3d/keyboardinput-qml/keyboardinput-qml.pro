!include( ../examples.pri ) {
    error( "Couldn't find the examples.pri file!" )
}

QT += 3dcore 3drender 3dquick 3dinput quick qml

SOURCES += \
    main.cpp

RESOURCES += \
    keyboardinput-qml.qrc

OTHER_FILES += \
    main.qml \
    SphereEntity.qml
