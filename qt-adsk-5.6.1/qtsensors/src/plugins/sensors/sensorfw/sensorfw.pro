TARGET = qtsensors_sensorfw
QT = core sensors network dbus

include(sensorfw.pri)


CONFIG += link_pkgconfig
PKGCONFIG += sensord-qt5

CONFIGFILES.files = Sensors.conf
CONFIGFILES.path = /etc/xdg/QtProject/
INSTALLS += CONFIGFILES

OTHER_FILES = plugin.json

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = sensorfwSensorPlugin
load(qt_plugin)
