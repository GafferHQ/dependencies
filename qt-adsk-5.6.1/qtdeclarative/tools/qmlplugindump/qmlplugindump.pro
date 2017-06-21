QT += qml qml-private quick-private core-private

CONFIG += no_import_scan

QTPLUGIN.platforms = qminimal

SOURCES += \
    main.cpp \
    qmlstreamwriter.cpp \
    qmltypereader.cpp

HEADERS += \
    qmlstreamwriter.h \
    qmltypereader.h

macx {
    # Prevent qmlplugindump from popping up in the dock when launched.
    # We embed the Info.plist file, so the application doesn't need to
    # be a bundle.
    QMAKE_LFLAGS += -sectcreate __TEXT __info_plist $$shell_quote($$PWD/Info.plist)
    CONFIG -= app_bundle
}

load(qt_tool)
