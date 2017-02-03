SOURCES = main.cpp
RESOURCES = animatedtiles.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/animation/animatedtiles
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS animatedtiles.pro images
sources.path = $$[QT_INSTALL_EXAMPLES]/animation/animatedtiles
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000D7D1
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)
