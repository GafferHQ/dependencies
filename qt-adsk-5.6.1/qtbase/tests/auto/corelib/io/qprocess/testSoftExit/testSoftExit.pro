win32 {
   SOURCES = main_win.cpp
   !wince: LIBS += -luser32
}
unix {
   SOURCES = main_unix.cpp
}

CONFIG -= qt app_bundle
CONFIG += console
DESTDIR = ./
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
QT = core
