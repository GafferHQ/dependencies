TEMPLATE = app
QT += svg

# Input
HEADERS += embeddedsvgviewer.h
SOURCES += embeddedsvgviewer.cpp main.cpp
RESOURCES += embeddedsvgviewer.qrc

target.path = $$[QT_INSTALL_DEMOS]/embedded/embeddedsvgviewer
sources.files = $$SOURCES $$HEADERS $$RESOURCES *.pro *.html *.svg files
sources.path = $$[QT_INSTALL_DEMOS]/embedded/embeddedsvgviewer
INSTALLS += target sources

wince* {
   DEPLOYMENT_PLUGIN += qsvg
}

symbian {
    TARGET.UID3 = 0xA000A640
    include($$QT_SOURCE_TREE/demos/symbianpkgrules.pri)
}
