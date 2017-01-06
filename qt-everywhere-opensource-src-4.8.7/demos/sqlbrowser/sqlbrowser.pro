TEMPLATE        = app
TARGET          = sqlbrowser

QT              += sql

HEADERS         = browser.h connectionwidget.h qsqlconnectiondialog.h
SOURCES         = main.cpp browser.cpp connectionwidget.cpp qsqlconnectiondialog.cpp

FORMS           = browserwidget.ui qsqlconnectiondialog.ui
build_all:!build_pass {
    CONFIG -= build_all
    CONFIG += release
}

# install
target.path = $$[QT_INSTALL_DEMOS]/sqlbrowser
sources.files = $$SOURCES $$HEADERS $$FORMS *.pro
sources.path = $$[QT_INSTALL_DEMOS]/sqlbrowser
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/demos/symbianpkgrules.pri)

wince*: {
    DEPLOYMENT_PLUGIN += qsqlite
}
