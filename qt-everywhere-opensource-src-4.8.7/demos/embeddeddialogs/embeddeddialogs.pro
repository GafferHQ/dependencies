SOURCES += main.cpp
SOURCES += customproxy.cpp embeddeddialog.cpp
HEADERS += customproxy.h embeddeddialog.h

FORMS += embeddeddialog.ui
RESOURCES += embeddeddialogs.qrc

build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

# install
target.path = $$[QT_INSTALL_DEMOS]/embeddeddialogs
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.png *.jpg *.plist *.icns *.ico *.rc *.pro *.html *.doc images
sources.path = $$[QT_INSTALL_DEMOS]/embeddeddialogs
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/demos/symbianpkgrules.pri)
