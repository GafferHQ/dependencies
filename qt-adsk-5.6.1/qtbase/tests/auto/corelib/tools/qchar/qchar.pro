CONFIG += testcase parallel_test
TARGET = tst_qchar
QT = core-private testlib
SOURCES = tst_qchar.cpp

TESTDATA += data/NormalizationTest.txt
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

android: !android-no-sdk {
    RESOURCES += \
        testdata.qrc
}
