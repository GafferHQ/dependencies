CONFIG += testcase
TARGET = tst_qquicksmoothedanimation
macx:CONFIG -= app_bundle

SOURCES += tst_qquicksmoothedanimation.cpp

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test

QT += core-private gui-private  qml-private quick-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
