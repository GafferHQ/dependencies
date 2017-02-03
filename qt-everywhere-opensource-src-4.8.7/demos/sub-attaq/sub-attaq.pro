contains(QT_CONFIG, opengl):QT += opengl
HEADERS += boat.h \
    bomb.h \
    mainwindow.h \
    submarine.h \
    torpedo.h \
    pixmapitem.h \
    graphicsscene.h \
    animationmanager.h \
    states.h \
    boat_p.h \
    submarine_p.h \
    qanimationstate.h \
    progressitem.h \
    textinformationitem.h
SOURCES += boat.cpp \
    bomb.cpp \
    main.cpp \
    mainwindow.cpp \
    submarine.cpp \
    torpedo.cpp \
    pixmapitem.cpp \
    graphicsscene.cpp \
    animationmanager.cpp \
    states.cpp \
    qanimationstate.cpp \
    progressitem.cpp \
    textinformationitem.cpp
RESOURCES += subattaq.qrc

# install
target.path = $$[QT_INSTALL_DEMOS]/sub-attaq
sources.files = $$SOURCES \
    $$HEADERS \
    $$RESOURCES \
    $$FORMS \
    sub-attaq.pro \
    pics
sources.path = $$[QT_INSTALL_DEMOS]/sub-attaq
INSTALLS += target \
    sources
