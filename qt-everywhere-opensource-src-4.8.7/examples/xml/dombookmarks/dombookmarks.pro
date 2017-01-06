HEADERS       = mainwindow.h \
                xbeltree.h
SOURCES       = main.cpp \
                mainwindow.cpp \
                xbeltree.cpp
QT           += xml

# install
target.path = $$[QT_INSTALL_EXAMPLES]/xml/dombookmarks
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS dombookmarks.pro *.xbel
sources.path = $$[QT_INSTALL_EXAMPLES]/xml/dombookmarks
INSTALLS += target sources

symbian: {
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
    addFiles.files = frank.xbel jennifer.xbel
    addFiles.path = files
    DEPLOYMENT += addFiles
}

wince*: {
    addFiles.files = frank.xbel jennifer.xbel
    addFiles.path = "\\My Documents"
    DEPLOYMENT += addFiles
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

