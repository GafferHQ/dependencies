contains(QT_CONFIG, evdev) {
    include($$PWD/evdevmouse/evdevmouse.pri)
    include($$PWD/evdevkeyboard/evdevkeyboard.pri)
    include($$PWD/evdevtouch/evdevtouch.pri)
    include($$PWD/evdevtablet/evdevtablet.pri)
}

contains(QT_CONFIG, tslib) {
    include($$PWD/tslib/tslib.pri)
}

contains(QT_CONFIG, libinput) {
    include($$PWD/libinput/libinput.pri)
}
