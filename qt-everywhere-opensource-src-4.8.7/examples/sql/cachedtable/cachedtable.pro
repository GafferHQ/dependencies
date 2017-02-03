HEADERS       = ../connection.h \
                tableeditor.h
SOURCES       = main.cpp \
                tableeditor.cpp
QT           += sql

# install
target.path = $$[QT_INSTALL_EXAMPLES]/sql/cachedtable
sources.files = $$SOURCES *.h $$RESOURCES $$FORMS cachedtable.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/sql/cachedtable
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

