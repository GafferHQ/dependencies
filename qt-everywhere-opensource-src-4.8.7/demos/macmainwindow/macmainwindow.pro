TEMPLATE        = app
TARGET          = macmainwindow

CONFIG          += qt warn_on console

OBJECTIVE_SOURCES += macmainwindow.mm
SOURCES += main.cpp 
HEADERS += macmainwindow.h

build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

LIBS += -framework Cocoa -framework Carbon

# install
mac {
target.path = $$[QT_INSTALL_DEMOS]/macmainwindow
sources.files = $$SOURCES  *.pro *.html
sources.path = $$[QT_INSTALL_DEMOS]/macmainwindow
INSTALLS += target sources
}
