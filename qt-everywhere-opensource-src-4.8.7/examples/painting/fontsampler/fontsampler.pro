FORMS     = mainwindowbase.ui
HEADERS   = mainwindow.h
SOURCES   = main.cpp \
            mainwindow.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/painting/fontsampler
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS fontsampler.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/painting/fontsampler
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

