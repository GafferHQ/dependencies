TEMPLATE = subdirs
SUBDIRS += globalVariables

# install
target.path = $$[QT_INSTALL_EXAMPLES]/xmlpatterns/xquery
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS xquery.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/xmlpatterns/xquery
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

