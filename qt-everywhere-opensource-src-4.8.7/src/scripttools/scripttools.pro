TARGET     = QtScriptTools
QPRO_PWD   = $$PWD
QT         = core gui script
DEFINES   += QT_BUILD_SCRIPTTOOLS_LIB
DEFINES   += QT_NO_USING_NAMESPACE
#win32-msvc*|win32-icc:QMAKE_LFLAGS += /BASE:0x66000000

unix|win32-g++*:QMAKE_PKGCONFIG_REQUIRES = QtCore QtGui QtScript

include(../qbase.pri)


include(debugging/debugging.pri)

symbian:TARGET.UID3=0x2001E625
