# Qt kernel library base module

HEADERS +=  \
	global/qglobal.h \
        global/qsystemdetection.h \
        global/qcompilerdetection.h \
        global/qprocessordetection.h \
	global/qnamespace.h \
        global/qendian.h \
        global/qnumeric_p.h \
        global/qnumeric.h \
        global/qglobalstatic.h \
        global/qlibraryinfo.h \
        global/qlogging.h \
        global/qtypeinfo.h \
        global/qsysinfo.h \
        global/qisenum.h \
        global/qtypetraits.h \
        global/qflags.h \
        global/qhooks_p.h \
        global/qversiontagging.h

SOURCES += \
        global/archdetect.cpp \
	global/qglobal.cpp \
        global/qglobalstatic.cpp \
        global/qlibraryinfo.cpp \
	global/qmalloc.cpp \
        global/qnumeric.cpp \
        global/qlogging.cpp \
        global/qhooks.cpp \
        global/qversiontagging.cpp

# qlibraryinfo.cpp includes qconfig.cpp
INCLUDEPATH += $$QT_BUILD_TREE/src/corelib/global

# Only used on platforms with CONFIG += precompile_header
PRECOMPILED_HEADER = global/qt_pch.h

# qlogging.cpp uses backtrace(3), which is in a separate library on the BSDs.
LIBS_PRIVATE += $$QMAKE_LIBS_EXECINFO

if(linux*|hurd*):!cross_compile:!static:!*-armcc* {
   QMAKE_LFLAGS += -Wl,-e,qt_core_boilerplate
   prog=$$quote(if (/program interpreter: (.*)]/) { print $1; })
   DEFINES += ELF_INTERPRETER=\\\"$$system(LC_ALL=C readelf -l /bin/ls | perl -n -e \'$$prog\')\\\"
}

slog2 {
    LIBS_PRIVATE += -lslog2
    DEFINES += QT_USE_SLOG2
}

journald {
    CONFIG += link_pkgconfig
    packagesExist(libsystemd): \
        PKGCONFIG_PRIVATE += libsystemd
    else: \
        PKGCONFIG_PRIVATE += libsystemd-journal
    DEFINES += QT_USE_JOURNALD
}

syslog {
    DEFINES += QT_USE_SYSLOG
}
