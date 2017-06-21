# Qt core io module

HEADERS +=  \
        io/qabstractfileengine_p.h \
        io/qbuffer.h \
        io/qdatastream.h \
        io/qdatastream_p.h \
        io/qdataurl_p.h \
        io/qdebug.h \
        io/qdebug_p.h \
        io/qdir.h \
        io/qdir_p.h \
        io/qdiriterator.h \
        io/qfile.h \
        io/qfiledevice.h \
        io/qfiledevice_p.h \
        io/qfileinfo.h \
        io/qfileinfo_p.h \
        io/qipaddress_p.h \
        io/qiodevice.h \
        io/qiodevice_p.h \
        io/qlockfile.h \
        io/qlockfile_p.h \
        io/qnoncontiguousbytedevice_p.h \
        io/qprocess.h \
        io/qprocess_p.h \
        io/qtextstream.h \
        io/qtextstream_p.h \
        io/qtemporarydir.h \
        io/qtemporaryfile.h \
        io/qtemporaryfile_p.h \
        io/qresource_p.h \
        io/qresource_iterator_p.h \
        io/qsavefile.h \
        io/qstandardpaths.h \
        io/qstorageinfo.h \
        io/qstorageinfo_p.h \
        io/qurl.h \
        io/qurl_p.h \
        io/qurlquery.h \
        io/qurltlds_p.h \
        io/qtldurl_p.h \
        io/qsettings.h \
        io/qsettings_p.h \
        io/qfsfileengine_p.h \
        io/qfsfileengine_iterator_p.h \
        io/qfilesystemwatcher.h \
        io/qfilesystemwatcher_p.h \
        io/qfilesystemwatcher_polling_p.h \
        io/qfilesystementry_p.h \
        io/qfilesystemengine_p.h \
        io/qfilesystemmetadata_p.h \
        io/qfilesystemiterator_p.h \
        io/qfileselector.h \
        io/qfileselector_p.h \
        io/qloggingcategory.h \
        io/qloggingregistry_p.h

SOURCES += \
        io/qabstractfileengine.cpp \
        io/qbuffer.cpp \
        io/qdatastream.cpp \
        io/qdataurl.cpp \
        io/qtldurl.cpp \
        io/qdebug.cpp \
        io/qdir.cpp \
        io/qdiriterator.cpp \
        io/qfile.cpp \
        io/qfiledevice.cpp \
        io/qfileinfo.cpp \
        io/qipaddress.cpp \
        io/qiodevice.cpp \
        io/qlockfile.cpp \
        io/qnoncontiguousbytedevice.cpp \
        io/qprocess.cpp \
        io/qstorageinfo.cpp \
        io/qtextstream.cpp \
        io/qtemporarydir.cpp \
        io/qtemporaryfile.cpp \
        io/qresource.cpp \
        io/qresource_iterator.cpp \
        io/qsavefile.cpp \
        io/qstandardpaths.cpp \
        io/qurl.cpp \
        io/qurlidna.cpp \
        io/qurlquery.cpp \
        io/qurlrecode.cpp \
        io/qsettings.cpp \
        io/qfsfileengine.cpp \
        io/qfsfileengine_iterator.cpp \
        io/qfilesystemwatcher.cpp \
        io/qfilesystemwatcher_polling.cpp \
        io/qfilesystementry.cpp \
        io/qfilesystemengine.cpp \
        io/qfileselector.cpp \
        io/qloggingcategory.cpp \
        io/qloggingregistry.cpp

win32 {
        SOURCES += io/qfsfileengine_win.cpp
        SOURCES += io/qlockfile_win.cpp

        SOURCES += io/qfilesystemwatcher_win.cpp
        HEADERS += io/qfilesystemwatcher_win_p.h
        SOURCES += io/qfilesystemengine_win.cpp
        SOURCES += io/qfilesystemiterator_win.cpp

    !winrt {
        SOURCES += io/qsettings_win.cpp
        SOURCES += io/qstandardpaths_win.cpp

        wince* {
            SOURCES += io/qprocess_wince.cpp \
                io/qstorageinfo_stub.cpp
        } else {
            HEADERS += \
                io/qwinoverlappedionotifier_p.h \
                io/qwindowspipereader_p.h \
                io/qwindowspipewriter_p.h
            SOURCES += \
                io/qprocess_win.cpp \
                io/qwinoverlappedionotifier.cpp \
                io/qwindowspipereader.cpp \
                io/qwindowspipewriter.cpp \
                io/qstorageinfo_win.cpp
            LIBS += -lmpr
        }
    } else {
        SOURCES += \
                io/qstandardpaths_winrt.cpp \
                io/qsettings_winrt.cpp \
                io/qstorageinfo_stub.cpp
    }
} else:unix|integrity {
        SOURCES += \
                io/qfsfileengine_unix.cpp \
                io/qfilesystemengine_unix.cpp \
                io/qlockfile_unix.cpp \
                io/qprocess_unix.cpp \
                io/qfilesystemiterator_unix.cpp \
                io/forkfd_qt.cpp
        HEADERS += \
                ../3rdparty/forkfd/forkfd.h
        INCLUDEPATH += ../3rdparty/forkfd

        !nacl:mac: {
            SOURCES += io/qsettings_mac.cpp
            OBJECTIVE_SOURCES += io/qurl_mac.mm
        }
        freebsd: LIBS_PRIVATE += -lutil         # qlockfile_unix.cpp requires this
        mac {
            osx {
                OBJECTIVE_SOURCES += io/qfilesystemwatcher_fsevents.mm
                HEADERS += io/qfilesystemwatcher_fsevents_p.h
            }
            macx {
                SOURCES += io/qstorageinfo_mac.cpp
                OBJECTIVE_SOURCES += io/qstandardpaths_mac.mm
                LIBS += -framework DiskArbitration -framework IOKit
            } else:ios {
                OBJECTIVE_SOURCES += io/qstandardpaths_ios.mm
                SOURCES += io/qstorageinfo_mac.cpp
                LIBS += -framework MobileCoreServices
            } else {
                SOURCES += io/qstandardpaths_unix.cpp
            }
        } else:blackberry {
            SOURCES += \
                io/qstandardpaths_blackberry.cpp \
                io/qstorageinfo_unix.cpp
        } else:android:!android-no-sdk {
            SOURCES += \
                io/qstandardpaths_android.cpp \
                io/qstorageinfo_unix.cpp
        } else:haiku {
            SOURCES += \
                io/qstandardpaths_haiku.cpp \
                io/qstorageinfo_unix.cpp
            LIBS += -lbe
        } else {
            SOURCES += \
                io/qstandardpaths_unix.cpp \
                io/qstorageinfo_unix.cpp
        }

        linux|if(qnx:contains(QT_CONFIG, inotify)) {
            SOURCES += io/qfilesystemwatcher_inotify.cpp
            HEADERS += io/qfilesystemwatcher_inotify_p.h
        }

        !nacl {
            freebsd-*|mac|darwin-*|openbsd-*|netbsd-*:{
                SOURCES += io/qfilesystemwatcher_kqueue.cpp
                HEADERS += io/qfilesystemwatcher_kqueue_p.h
            }
        }
}

