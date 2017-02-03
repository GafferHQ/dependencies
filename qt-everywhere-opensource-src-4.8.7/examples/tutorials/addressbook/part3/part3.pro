SOURCES = addressbook.cpp \
          main.cpp
HEADERS = addressbook.h

# install
target.path = $$[QT_INSTALL_EXAMPLES]/tutorials/addressbook/part3
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS part3.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/tutorials/addressbook/part3
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

symbian: warning(This example might not fully work on Symbian platform)
maemo5: warning(This example might not fully work on Maemo platform)
simulator: warning(This example might not fully work on Simulator platform)
