HEADERS   += certificateinfo.h \
             sslclient.h
SOURCES   += certificateinfo.cpp \
             main.cpp \
             sslclient.cpp
RESOURCES += securesocketclient.qrc
FORMS     += certificateinfo.ui \
             sslclient.ui \
             sslerrors.ui
QT        += network

# install
target.path = $$[QT_INSTALL_EXAMPLES]/network/securesocketclient
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.pro *.png *.jpg images
sources.path = $$[QT_INSTALL_EXAMPLES]/network/securesocketclient
INSTALLS += target sources

symbian {
    TARGET.UID3 = 0xA000CF67
    include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
    TARGET.CAPABILITY = NetworkServices
}
maemo5: include($$QT_SOURCE_TREE/examples/maemo5pkgrules.pri)

