QT += network gui widgets enginio

TARGET = cloudaddressbook
TEMPLATE = app

include(../../common/backendhelper/backendhelper.pri)

SOURCES += \
    main.cpp\
    mainwindow.cpp \
    addressbookmodel.cpp \

HEADERS += \
    mainwindow.h \
    addressbookmodel.h \

FORMS += \
    mainwindow.ui

target.path = $$[QT_INSTALL_EXAMPLES]/enginio/widgets/cloudaddressbook
INSTALLS += target
