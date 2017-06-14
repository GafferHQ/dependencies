TEMPLATE = app

DEFINES += ENGINIO_SAMPLE_NAME=\\\"socialtodos\\\"

include(../../common/backendhelper/qmlbackendhelper.pri)

QT += quick qml enginio
SOURCES += ../main.cpp

mac: CONFIG -= app_bundle

OTHER_FILES += \
    Header.qml \
    List.qml \
    Login.qml \
    ShareDialog.qml \
    TextField.qml \
    TodoLists.qml \
    socialtodos.qml

RESOURCES += socialtodos.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/enginio/quick/socialtodos
INSTALLS += target

EXAMPLE_FILES += \
    backendconfig
