FORMS       = mainwindow.ui
HEADERS     = mainwindow.h \
              treeitem.h \
              treemodel.h
RESOURCES   = editabletreemodel.qrc
SOURCES     = mainwindow.cpp \
              treeitem.cpp \
              treemodel.cpp \
              main.cpp
CONFIG  += qt

# install
target.path = $$[QT_INSTALL_EXAMPLES]/itemviews/editabletreemodel
sources.files = $$FORMS $$HEADERS $$RESOURCES $$SOURCES *.pro *.txt
sources.path = $$[QT_INSTALL_EXAMPLES]/itemviews/editabletreemodel
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

