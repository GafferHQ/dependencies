QT += widgets

HEADERS	    =	mainwindow.h \
        tabletcanvas.h \
        tabletapplication.h
SOURCES	    =	mainwindow.cpp \
        main.cpp \
        tabletcanvas.cpp \
        tabletapplication.cpp
RESOURCES += images.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/widgets/tablet
INSTALLS += target
