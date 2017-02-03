HEADERS += httpwindow.h
SOURCES += httpwindow.cpp \
           main.cpp
FORMS += authenticationdialog.ui
QT += network

# install
target.path = $$[QT_INSTALL_EXAMPLES]/network/http
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS http.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/network/http
INSTALLS += target sources

symbian: {
    TARGET.CAPABILITY = NetworkServices
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

