CONFIG += testcase
CONFIG += parallel_test

SOURCES += tst_qsslcipher.cpp
win32:!wince: LIBS += -lws2_32
QT = core network testlib

TARGET = tst_qsslcipher

win32 {
  CONFIG(debug, debug|release) {
    DESTDIR = debug
} else {
    DESTDIR = release
  }
}
