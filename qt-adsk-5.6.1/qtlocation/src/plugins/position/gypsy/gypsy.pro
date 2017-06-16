TARGET = qtposition_gypsy

QT = core positioning

HEADERS += \
    qgeosatelliteinfosource_gypsy_p.h \
    qgeopositioninfosourcefactory_gypsy.h

SOURCES += \
    qgeosatelliteinfosource_gypsy.cpp \
    qgeopositioninfosourcefactory_gypsy.cpp

CONFIG += link_pkgconfig
PKGCONFIG += gypsy gconf-2.0

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryGypsy
load(qt_plugin)
