TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .
QT += script
win32: CONFIG += console
mac: CONFIG -= app_bundle

SOURCES += main.cpp

include(qsdbg.pri)

# install
target.path = $$[QT_INSTALL_EXAMPLES]/script/qsdbg
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS qsdbg.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/script/qsdbg
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

symbian: warning(This example does not work on Symbian platform)
