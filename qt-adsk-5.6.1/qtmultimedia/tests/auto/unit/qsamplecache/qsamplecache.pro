CONFIG += testcase
TARGET = tst_qsamplecache

QT += multimedia-private testlib

SOURCES += tst_qsamplecache.cpp

TESTDATA += testdata/*
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
