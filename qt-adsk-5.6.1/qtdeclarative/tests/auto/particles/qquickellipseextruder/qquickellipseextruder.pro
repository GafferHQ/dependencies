CONFIG += testcase
TARGET = tst_qquickellipseextruder
SOURCES += tst_qquickellipseextruder.cpp
macx:CONFIG -= app_bundle

include (../../shared/util.pri)
TESTDATA = data/*

QT += core-private gui-private  qml-private quick-private quickparticles-private testlib

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
