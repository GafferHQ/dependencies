CXX_MODULE = qml
TARGET = quick3dlogicplugin
TARGETPATH = Qt3D/Logic

QT += core-private qml 3dcore 3dlogic

OTHER_FILES += qmldir

HEADERS += \
    qt3dquick3dlogicplugin.h

SOURCES += \
    qt3dquick3dlogicplugin.cpp

load(qml_plugin)
