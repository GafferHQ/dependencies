QT += script
RESOURCES += helloscript.qrc
SOURCES += main.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/script/helloscript
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS helloscript.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/script/helloscript
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

