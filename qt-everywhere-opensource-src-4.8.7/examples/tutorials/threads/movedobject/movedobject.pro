CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
SOURCES += main.cpp \
    workerobject.cpp \
    thread.cpp

HEADERS += \
    workerobject.h \
    thread.h

# install
target.path = $$[QT_INSTALL_EXAMPLES]/tutorials/threads/movedobject
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS movedobject.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/tutorials/threads/movedobject
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
