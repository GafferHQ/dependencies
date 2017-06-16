# Qt tools module

intel_icc: QMAKE_CXXFLAGS += -fp-model strict

HEADERS +=  \
        tools/qalgorithms.h \
        tools/qarraydata.h \
        tools/qarraydataops.h \
        tools/qarraydatapointer.h \
        tools/qbitarray.h \
        tools/qbytearray.h \
        tools/qbytearray_p.h \
        tools/qbytearraylist.h \
        tools/qbytearraymatcher.h \
        tools/qbytedata_p.h \
        tools/qcache.h \
        tools/qchar.h \
        tools/qcommandlineoption.h \
        tools/qcommandlineparser.h \
        tools/qcollator.h \
        tools/qcollator_p.h \
        tools/qcontainerfwd.h \
        tools/qcryptographichash.h \
        tools/qdatetime.h \
        tools/qdatetime_p.h \
        tools/qdatetimeparser_p.h \
        tools/qeasingcurve.h \
        tools/qfreelist_p.h \
        tools/qhash.h \
        tools/qhashfunctions.h \
        tools/qiterator.h \
        tools/qline.h \
        tools/qlinkedlist.h \
        tools/qlist.h \
        tools/qlocale.h \
        tools/qlocale_p.h \
        tools/qlocale_tools_p.h \
        tools/qlocale_data_p.h \
        tools/qmap.h \
        tools/qmargins.h \
        tools/qmessageauthenticationcode.h \
        tools/qcontiguouscache.h \
        tools/qpodlist_p.h \
        tools/qpair.h \
        tools/qpoint.h \
        tools/qqueue.h \
        tools/qrect.h \
        tools/qregexp.h \
        tools/qringbuffer_p.h \
        tools/qrefcount.h \
        tools/qscopedpointer.h \
        tools/qscopedpointer_p.h \
        tools/qscopedvaluerollback.h \
        tools/qshareddata.h \
        tools/qsharedpointer.h \
        tools/qsharedpointer_impl.h \
        tools/qset.h \
        tools/qsimd_p.h \
        tools/qsize.h \
        tools/qstack.h \
        tools/qstring.h \
        tools/qstringalgorithms_p.h \
        tools/qstringbuilder.h \
        tools/qstringiterator_p.h \
        tools/qstringlist.h \
        tools/qstringmatcher.h \
        tools/qtextboundaryfinder.h \
        tools/qtimeline.h \
        tools/qtimezone.h \
        tools/qtimezoneprivate_p.h \
        tools/qtimezoneprivate_data_p.h \
        tools/qtools_p.h \
        tools/qelapsedtimer.h \
        tools/qunicodetables_p.h \
        tools/qunicodetools_p.h \
        tools/qvarlengtharray.h \
        tools/qvector.h \
        tools/qversionnumber.h


SOURCES += \
        tools/qarraydata.cpp \
        tools/qbitarray.cpp \
        tools/qbytearray.cpp \
        tools/qbytearraylist.cpp \
        tools/qbytearraymatcher.cpp \
        tools/qcollator.cpp \
        tools/qcommandlineoption.cpp \
        tools/qcommandlineparser.cpp \
        tools/qcryptographichash.cpp \
        tools/qdatetime.cpp \
        tools/qdatetimeparser.cpp \
        tools/qeasingcurve.cpp \
        tools/qelapsedtimer.cpp \
        tools/qfreelist.cpp \
        tools/qhash.cpp \
        tools/qline.cpp \
        tools/qlinkedlist.cpp \
        tools/qlist.cpp \
        tools/qlocale.cpp \
        tools/qlocale_tools.cpp \
        tools/qpoint.cpp \
        tools/qmap.cpp \
        tools/qmargins.cpp \
        tools/qmessageauthenticationcode.cpp \
        tools/qcontiguouscache.cpp \
        tools/qrect.cpp \
        tools/qregexp.cpp \
        tools/qrefcount.cpp \
        tools/qringbuffer.cpp \
        tools/qshareddata.cpp \
        tools/qsharedpointer.cpp \
        tools/qsimd.cpp \
        tools/qsize.cpp \
        tools/qstring.cpp \
        tools/qstringbuilder.cpp \
        tools/qstringlist.cpp \
        tools/qtextboundaryfinder.cpp \
        tools/qtimeline.cpp \
        tools/qtimezone.cpp \
        tools/qtimezoneprivate.cpp \
        tools/qunicodetools.cpp \
        tools/qvector.cpp \
        tools/qvsnprintf.cpp \
        tools/qversionnumber.cpp

NO_PCH_SOURCES = tools/qstring_compat.cpp
msvc: NO_PCH_SOURCES += tools/qvector_msvc.cpp
false: SOURCES += $$NO_PCH_SOURCES # Hack for QtCreator

!nacl:mac: {
    SOURCES += tools/qelapsedtimer_mac.cpp
    OBJECTIVE_SOURCES += tools/qlocale_mac.mm \
                         tools/qtimezoneprivate_mac.mm \
                         tools/qstring_mac.mm \
                         tools/qbytearray_mac.mm \
                         tools/qdatetime_mac.mm
}
else:blackberry {
    SOURCES += tools/qelapsedtimer_unix.cpp tools/qlocale_blackberry.cpp tools/qtimezoneprivate_tz.cpp
    HEADERS += tools/qlocale_blackberry.h
}
else:android {
    SOURCES += tools/qelapsedtimer_unix.cpp tools/qlocale_unix.cpp tools/qtimezoneprivate_android.cpp
}
else:unix {
    SOURCES += tools/qelapsedtimer_unix.cpp tools/qlocale_unix.cpp tools/qtimezoneprivate_tz.cpp
}
else:win32 {
    SOURCES += tools/qelapsedtimer_win.cpp \
               tools/qlocale_win.cpp \
               tools/qtimezoneprivate_win.cpp
    winphone: LIBS_PRIVATE += -lWindowsPhoneGlobalizationUtil
    winrt-*-msvc2013: LIBS += advapi32.lib
} else:integrity:SOURCES += tools/qelapsedtimer_unix.cpp tools/qlocale_unix.cpp
else:SOURCES += tools/qelapsedtimer_generic.cpp

contains(QT_CONFIG, zlib) {
    include($$PWD/../../3rdparty/zlib.pri)
} else {
    CONFIG += no_core_dep
    include($$PWD/../../3rdparty/zlib_dependency.pri)
}

contains(QT_CONFIG,icu) {
    include($$PWD/../../3rdparty/icu_dependency.pri)

    SOURCES += tools/qlocale_icu.cpp \
               tools/qcollator_icu.cpp \
               tools/qtimezoneprivate_icu.cpp
    DEFINES += QT_USE_ICU
} else: win32 {
    SOURCES += tools/qcollator_win.cpp
} else: macx {
    SOURCES += tools/qcollator_macx.cpp
} else {
    SOURCES += tools/qcollator_posix.cpp
}

!contains(QT_DISABLED_FEATURES, regularexpression) {
    include($$PWD/../../3rdparty/pcre_dependency.pri)

    HEADERS += tools/qregularexpression.h
    SOURCES += tools/qregularexpression.cpp
}

INCLUDEPATH += ../3rdparty/harfbuzz/src
HEADERS += ../3rdparty/harfbuzz/src/harfbuzz.h
SOURCES += ../3rdparty/harfbuzz/src/harfbuzz-buffer.c \
           ../3rdparty/harfbuzz/src/harfbuzz-gdef.c \
           ../3rdparty/harfbuzz/src/harfbuzz-gsub.c \
           ../3rdparty/harfbuzz/src/harfbuzz-gpos.c \
           ../3rdparty/harfbuzz/src/harfbuzz-impl.c \
           ../3rdparty/harfbuzz/src/harfbuzz-open.c \
           ../3rdparty/harfbuzz/src/harfbuzz-stream.c \
           ../3rdparty/harfbuzz/src/harfbuzz-shaper-all.cpp \
           tools/qharfbuzz.cpp
HEADERS += tools/qharfbuzz_p.h

INCLUDEPATH += ../3rdparty/md5 \
               ../3rdparty/md4 \
               ../3rdparty/sha3

# Note: libm should be present by default becaue this is C++
!macx-icc:!vxworks:!haiku:unix:LIBS_PRIVATE += -lm

TR_EXCLUDE += ../3rdparty/*

# MIPS DSP
MIPS_DSP_ASM += tools/qstring_mips_dsp_asm.S
MIPS_DSP_HEADERS += ../gui/painting/qt_mips_asm_dsp_p.h
CONFIG += simd
