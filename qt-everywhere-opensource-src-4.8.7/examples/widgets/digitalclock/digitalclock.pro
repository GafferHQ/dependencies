HEADERS       = digitalclock.h
SOURCES       = digitalclock.cpp \
                main.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/digitalclock
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS digitalclock.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/widgets/digitalclock
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

