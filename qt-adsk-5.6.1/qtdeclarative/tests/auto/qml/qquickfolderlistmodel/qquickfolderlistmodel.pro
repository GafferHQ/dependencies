CONFIG += testcase
TARGET = tst_qquickfolderlistmodel
macx:CONFIG -= app_bundle

SOURCES += tst_qquickfolderlistmodel.cpp

include (../../shared/util.pri)

TESTDATA = data/*

CONFIG += parallel_test
QT += core-private gui-private qml-private testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

RESOURCES += data/introspect.qrc
