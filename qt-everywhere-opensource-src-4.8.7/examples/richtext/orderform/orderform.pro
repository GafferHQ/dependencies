HEADERS     = detailsdialog.h \
              mainwindow.h
SOURCES     = detailsdialog.cpp \
              main.cpp \
              mainwindow.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/richtext/orderform
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS orderform.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/richtext/orderform
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

