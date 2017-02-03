HEADERS       = wigglywidget.h \
                dialog.h
SOURCES       = wigglywidget.cpp \
                dialog.cpp \
                main.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/wiggly
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS wiggly.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/widgets/wiggly
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000C607
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

