HEADERS += \
        edge.h \
        node.h \
        graphwidget.h

SOURCES += \
        edge.cpp \
        main.cpp \
        node.cpp \
        graphwidget.cpp

TARGET.EPOCHEAPSIZE = 0x200000 0xA00000

# install
target.path = $$[QT_INSTALL_EXAMPLES]/graphicsview/elasticnodes
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS elasticnodes.pro 
sources.path = $$[QT_INSTALL_EXAMPLES]/graphicsview/elasticnodes
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000A642
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

simulator: warning(This example might not fully work on Simulator platform)
