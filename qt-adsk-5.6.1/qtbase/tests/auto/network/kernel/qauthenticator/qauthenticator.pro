CONFIG += testcase
TARGET = tst_qauthenticator
requires(contains(QT_CONFIG,private_tests))
QT = core network-private testlib
SOURCES  += tst_qauthenticator.cpp
DEFINES += SRCDIR=\\\"$$PWD/\\\"
