CONFIG += testcase
TARGET = tst_qqmlinfo
macx:CONFIG -= app_bundle

SOURCES += tst_qqmlinfo.cpp

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test
QT += core-private gui-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
