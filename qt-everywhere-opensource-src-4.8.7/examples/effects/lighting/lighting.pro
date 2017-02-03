SOURCES += main.cpp lighting.cpp
HEADERS += lighting.h

# install
target.path = $$[QT_INSTALL_EXAMPLES]/effects/lighting
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS lighting.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/effects/lighting
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

