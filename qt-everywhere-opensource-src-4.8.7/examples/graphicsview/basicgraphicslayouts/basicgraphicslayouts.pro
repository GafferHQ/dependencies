HEADERS   = layoutitem.h \
            window.h
SOURCES   = layoutitem.cpp \
            main.cpp \
            window.cpp
RESOURCES = basicgraphicslayouts.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/graphicsview/basicgraphicslayouts
sources.files = $$SOURCES $$HEADERS $$RESOURCES basicgraphicslayouts.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/graphicsview/basicgraphicslayouts
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000A645
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)
simulator: warning(This example might not fully work on Simulator platform)
