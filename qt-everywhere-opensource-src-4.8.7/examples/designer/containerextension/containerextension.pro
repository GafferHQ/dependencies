#! [0]
TEMPLATE = lib
#! [0]
TARGET   = $$qtLibraryTarget($$TARGET)
#! [1]
CONFIG  += designer plugin
#! [1]
QTDIR_build:DESTDIR  = $$QT_BUILD_TREE/plugins/designer

#! [2]
HEADERS += multipagewidget.h \
           multipagewidgetplugin.h \ 
           multipagewidgetcontainerextension.h \
           multipagewidgetextensionfactory.h 

SOURCES += multipagewidget.cpp \
           multipagewidgetplugin.cpp \
           multipagewidgetcontainerextension.cpp \
           multipagewidgetextensionfactory.cpp 
#! [2]

build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

# install
target.path = $$[QT_INSTALL_PLUGINS]/designer
sources.files = $$SOURCES $$HEADERS *.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/designer/containerextension
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)
symbian: warning(This example does not work on Symbian platform)
maemo5: warning(This example does not work on Maemo platform)
simulator: warning(This example does not work on Simulator platform)
