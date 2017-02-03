HEADERS = ../connection.h \
          imageitem.h \
          informationwindow.h \
          view.h
RESOURCES = drilldown.qrc
SOURCES = imageitem.cpp \
          informationwindow.cpp \
          main.cpp \
          view.cpp
QT += sql

# install
target.path = $$[QT_INSTALL_EXAMPLES]/sql/drilldown
sources.files = $$SOURCES *.h $$RESOURCES $$FORMS drilldown.pro *.png *.jpg images
sources.path = $$[QT_INSTALL_EXAMPLES]/sql/drilldown
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000C612
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

simulator: warning(This example might not fully work on Simulator platform)
