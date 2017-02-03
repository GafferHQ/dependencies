TEMPLATE        = app
TARGET          = lupdate
DESTDIR          = ../../../bin

QT              -= gui

CONFIG          += qt warn_on console
CONFIG          -= app_bundle

build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

include(../shared/formats.pri)
include(../shared/proparser.pri)

include($$QT_SOURCE_TREE/src/declarative/qml/parser/parser.pri)
INCLUDEPATH += $$QT_SOURCE_TREE/src/declarative/qml $$QT_BUILD_TREE/include/QtDeclarative

SOURCES += \
    main.cpp \
    merge.cpp \
    ../shared/simtexth.cpp \
    \
    cpp.cpp \
    java.cpp \
    qscript.cpp \
    qdeclarative.cpp \
    ui.cpp

HEADERS += \
    lupdate.h \
    ../shared/simtexth.h

DEFINES += QT_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII


win32:RC_FILE = winmanifest.rc

embed_manifest_exe:win32-msvc2005 {
    # The default configuration embed_manifest_exe overrides the manifest file
    # already embedded via RC_FILE. Vs2008 already have the necessary manifest entry
    QMAKE_POST_LINK += $$quote(mt.exe -updateresource:$$DESTDIR/lupdate.exe -manifest \"$${PWD}\\lupdate.exe.manifest\")
}

target.path=$$[QT_INSTALL_BINS]
INSTALLS        += target

