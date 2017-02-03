HEADERS =   window.h
SOURCES =   main.cpp \ 
	    window.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/calendarwidget
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS calendarwidget.pro resources
sources.path = $$[QT_INSTALL_EXAMPLES]/widgets/calendarwidget
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000C603
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

symbian: warning(This example might not fully work on Symbian platform)
maemo5: warning(This example might not fully work on Maemo platform)
simulator: warning(This example might not fully work on Simulator platform)
