CONFIG += testcase
TARGET = tst_qquickanimations
SOURCES += tst_qquickanimations.cpp

include (../../shared/util.pri)

macx:CONFIG -= app_bundle

TESTDATA = data/*

CONFIG += parallel_test

QT += core-private gui-private  qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
