TEMPLATE = app
TARGET = tst_bench_qnetworkreply

QT -= gui
QT += core-private network network-private testlib

CONFIG += release

SOURCES += tst_qnetworkreply.cpp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
