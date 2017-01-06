HEADERS             = screenshot.h
SOURCES             = main.cpp \
                      screenshot.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/desktop/screenshot
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS screenshot.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/desktop/screenshot
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)
symbian: warning(This example might not fully work on Symbian platform)
maemo5: warning(This example might not fully work on Maemo platform)
simulator: warning(This example might not fully work on Simulator platform)
