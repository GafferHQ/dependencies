CONFIG += testcase
TARGET = tst_accessibility
SOURCES += tst_accessibility.cpp

osx:CONFIG -= app_bundle

QT += core-private gui-private qml-private quick-private labstemplates-private testlib

include (../shared/util.pri)

TESTDATA = data/*

OTHER_FILES += \
    data/*

