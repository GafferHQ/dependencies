CONFIG += testcase
TARGET = tst_applicationwindow
SOURCES += tst_applicationwindow.cpp

osx:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private labstemplates-private labscontrols-private testlib

include (../shared/util.pri)

TESTDATA = data/*

OTHER_FILES += \
    data/*

