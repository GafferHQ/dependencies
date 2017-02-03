HEADERS += glwidget.h
SOURCES += glwidget.cpp main.cpp
RESOURCES += framebufferobject2.qrc

QT += opengl

# install
target.path = $$[QT_INSTALL_EXAMPLES]/opengl/framebufferobject2
sources.files = $$SOURCES $$HEADERS $$RESOURCES framebufferobject2.pro *.png
sources.path = $$[QT_INSTALL_EXAMPLES]/opengl/framebufferobject2
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

symbian: warning(This example might not fully work on Symbian platform)
maemo5: warning(This example does not work on Maemo platform)
simulator: warning(This example might not fully work on Simulator platform)
