QT = compositor compositor-private core-private gui-private platformsupport-private

OTHER_FILES += wayland-egl.json

SOURCES += \
    main.cpp

include(../../../../hardwareintegration/compositor/wayland-egl/wayland-egl.pri)

PLUGIN_TYPE = wayland-graphics-integration-server
load(qt_plugin)
