requires(qtHaveModule(qml))

TARGETPATH = Enginio

IMPORT_VERSION = 1.0

QT = qml enginio enginio-private core-private

QMAKE_DOCS = $$PWD/doc/qtenginioqml.qdocconf
OTHER_FILES += \
    doc/qtenginioqml.qdocconf \
    doc/enginio_plugin.qdoc \
    qmldir

include(../src.pri)


TARGET = enginioplugin
TARGET.module_name = Enginio

SOURCES += \
    enginioqmlclient.cpp \
    enginioqmlmodel.cpp \
    enginioplugin.cpp \
    enginioqmlreply.cpp \

HEADERS += \
    enginioqmlobjectadaptor_p.h \
    enginioqmlclient_p_p.h \
    enginioplugin_p.h \
    enginioqmlclient_p.h \
    enginioqmlmodel_p.h \
    enginioqmlreply_p.h

CONFIG += no_cxx_module
load(qml_plugin)

DEFINES +=  "ENGINIO_VERSION=\\\"$$MODULE_VERSION\\\""
