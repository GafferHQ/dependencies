TEMPLATE = lib
TARGET = Dummy
QT += qml quick
CONFIG += qt plugin

TARGET = $$qtLibraryTarget($$TARGET)
uri = tests.dumper.Dummy

# Input
SOURCES += \
    dummy_plugin.cpp \
    dummy.cpp

HEADERS += \
    dummy_plugin.h \
    dummy.h

DISTFILES = qmldir

!equals(_PRO_FILE_PWD_, $$OUT_PWD) {
    cpqmldir.files = qmldir
    cpqmldir.path = $$OUT_PWD
    COPIES += cpqmldir
}

qmldir.files = qmldir
unix {
    installPath = $$[QT_INSTALL_QML]/$$replace(uri, \\., /)
    qmldir.path = $$installPath
    target.path = $$installPath
    INSTALLS += target qmldir
}

