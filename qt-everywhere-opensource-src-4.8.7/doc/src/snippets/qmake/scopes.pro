#! [syntax]
<condition> {
    <command or definition>
    ...
}
#! [syntax]

#! [0]
win32 {
    SOURCES += paintwidget_win.cpp
}
#! [0]

#! [1]
!win32 {
    SOURCES -= paintwidget_win.cpp
}
#! [1]

unix {
    SOURCES += paintwidget_unix.cpp
}

#! [2]
macx {
    debug {
        HEADERS += debugging.h
    }
}
#! [2]

#! [3]
macx:debug {
    HEADERS += debugging.h
}
#! [3]

#! [4]
win32|macx {
    HEADERS += debugging.h
}
#! [4]
