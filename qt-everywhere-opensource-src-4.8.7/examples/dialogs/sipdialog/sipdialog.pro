HEADERS       = dialog.h
SOURCES       = main.cpp \
                dialog.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/dialogs/sipdialog
sources.files = $$SOURCES $$HEADERS *.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/dialogs/sipdialog
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
wince50standard-x86-msvc2005: LIBS += libcmt.lib corelibc.lib ole32.lib oleaut32.lib uuid.lib commctrl.lib coredll.lib winsock.lib ws2.lib

maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)
symbian: warning(This example might not fully work on Symbian platform)
maemo5: warning(This example might not fully work on Maemo platform)
simulator: warning(This example might not fully work on Simulator platform)
