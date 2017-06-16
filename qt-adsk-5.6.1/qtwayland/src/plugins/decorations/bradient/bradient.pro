QT += waylandclient-private

OTHER_FILES += \
    bradient.json

SOURCES += main.cpp

contains(QT_CONFIG, no-pkg-config) {
    LIBS += -lwayland-client
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-client
}

PLUGIN_TYPE = wayland-decoration-client
load(qt_plugin)
