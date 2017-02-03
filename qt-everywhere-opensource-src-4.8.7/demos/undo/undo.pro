SOURCES += main.cpp mainwindow.cpp commands.cpp document.cpp
HEADERS += mainwindow.h commands.h document.h
FORMS += mainwindow.ui

build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

RESOURCES += undo.qrc

# install
target.path = $$[QT_INSTALL_DEMOS]/undo
sources.files = $$SOURCES $$HEADERS *.pro icons $$RESOURCES $$FORMS
sources.path = $$[QT_INSTALL_DEMOS]/undo
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/demos/symbianpkgrules.pri)
