HEADERS += \
        mouse.h \
        graphicsview.h
SOURCES += \
	main.cpp \
        mouse.cpp \
        graphicsview.cpp

RESOURCES += \
	mice.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/touch/pinchzoom
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS pinchzoom.pro images
sources.path = $$[QT_INSTALL_EXAMPLES]/touch/pinchzoom
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

symbian: warning(This example might not fully work on Symbian platform)
maemo5: warning(This example might not fully work on Maemo platform)
simulator: warning(This example might not fully work on Simulator platform)
