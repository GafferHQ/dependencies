SOURCES = main.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/statemachine/eventtransitions
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS eventtransitions.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/statemachine/eventtransitions
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

simulator: warning(This example might not fully work on Simulator platform)
