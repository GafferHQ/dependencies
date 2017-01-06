HEADERS       = tetrixboard.h \
                tetrixpiece.h \
                tetrixwindow.h
SOURCES       = main.cpp \
                tetrixboard.cpp \
                tetrixpiece.cpp \
                tetrixwindow.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/tetrix
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS tetrix.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/widgets/tetrix
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000C606
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

