TARGET = QtAndroidExtras
DEFINES += QT_NO_USING_NAMESPACE
QMAKE_DOCS = \
             $$PWD/doc/qtandroidextras.qdocconf
QT -= gui
QT += core-private

include(jni/jni.pri)
include(android/android.pri)

load(qt_module)
