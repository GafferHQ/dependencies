TEMPLATE  = subdirs
SUBDIRS   = toplevel childwidget windowlayout nestedlayouts

# install
target.path = $$[QT_INSTALL_EXAMPLES]/tutorials/widgets
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS widgets.pro README
sources.path = $$[QT_INSTALL_EXAMPLES]/tutorials/widgets
INSTALLS += target sources
