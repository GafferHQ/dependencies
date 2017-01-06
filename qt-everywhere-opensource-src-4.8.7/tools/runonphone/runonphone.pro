TEMPLATE = app

QT -= gui
CONFIG += console static
CONFIG -= app_bundle

include(symbianutils/symbianutils.pri)

SOURCES += main.cpp \
    trksignalhandler.cpp \
    ossignalconverter.cpp \
    codasignalhandler.cpp \
    texttracehandler.cpp

HEADERS += trksignalhandler.h \
    serenum.h \
    ossignalconverter.h \
    ossignalconverter_p.h \
    codasignalhandler.h \
    texttracehandler.h

DEFINES += SYMBIANUTILS_INCLUDE_PRI

windows { 
    SOURCES += serenum_win.cpp 
    LIBS += -lsetupapi \
            -luuid \
            -ladvapi32
}
else:unix:!symbian {
    SOURCES += serenum_unix.cpp
    LIBS += -lusb
}
else {
    SOURCES += serenum_stub.cpp
}

target.path=$$[QT_INSTALL_BINS]
INSTALLS        += target
