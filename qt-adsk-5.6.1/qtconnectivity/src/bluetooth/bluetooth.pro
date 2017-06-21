TARGET = QtBluetooth
QT = core
QT_PRIVATE = concurrent


QMAKE_DOCS = $$PWD/doc/qtbluetooth.qdocconf
OTHER_FILES += doc/src/*.qdoc   # show .qdoc files in Qt Creator

PUBLIC_HEADERS += \
    qbluetoothglobal.h \
    qbluetoothaddress.h\
    qbluetoothhostinfo.h \
    qbluetoothuuid.h\
    qbluetoothdeviceinfo.h\
    qbluetoothserviceinfo.h\
    qbluetoothdevicediscoveryagent.h\
    qbluetoothservicediscoveryagent.h\
    qbluetoothsocket.h\
    qbluetoothserver.h \
    qbluetooth.h \
    qbluetoothlocaldevice.h \
    qbluetoothtransfermanager.h \
    qbluetoothtransferrequest.h \
    qlowenergyservice.h \
    qlowenergycharacteristic.h \
    qlowenergydescriptor.h \
    qbluetoothtransferreply.h \
    qlowenergycontroller.h

PRIVATE_HEADERS += \
    qbluetoothaddress_p.h\
    qbluetoothhostinfo_p.h \
    qbluetoothdeviceinfo_p.h\
    qbluetoothserviceinfo_p.h\
    qbluetoothdevicediscoveryagent_p.h\
    qbluetoothservicediscoveryagent_p.h\
    qbluetoothsocket_p.h\
    qbluetoothserver_p.h\
    qbluetoothtransferreply_p.h \
    qbluetoothtransferrequest_p.h \
    qprivatelinearbuffer_p.h \
    qbluetoothlocaldevice_p.h \
    qlowenergycontroller_p.h \
    qlowenergyserviceprivate_p.h

SOURCES += \
    qbluetoothaddress.cpp\
    qbluetoothhostinfo.cpp \
    qbluetoothuuid.cpp\
    qbluetoothdeviceinfo.cpp\
    qbluetoothserviceinfo.cpp\
    qbluetoothdevicediscoveryagent.cpp\
    qbluetoothservicediscoveryagent.cpp\
    qbluetoothsocket.cpp\
    qbluetoothserver.cpp \
    qbluetoothlocaldevice.cpp \
    qbluetooth.cpp \
    qbluetoothtransfermanager.cpp \
    qbluetoothtransferrequest.cpp \
    qbluetoothtransferreply.cpp \
    qlowenergyservice.cpp \
    qlowenergycharacteristic.cpp \
    qlowenergydescriptor.cpp \
    qlowenergycontroller.cpp \
    qlowenergyserviceprivate.cpp

config_bluez:qtHaveModule(dbus) {
    QT_FOR_PRIVATE += dbus
    DEFINES += QT_BLUEZ_BLUETOOTH

    include(bluez/bluez.pri)

    PRIVATE_HEADERS += \
        qbluetoothtransferreply_bluez_p.h

    SOURCES += \
        qbluetoothserviceinfo_bluez.cpp \
        qbluetoothdevicediscoveryagent_bluez.cpp\
        qbluetoothservicediscoveryagent_bluez.cpp \
        qbluetoothsocket_bluez.cpp \
        qbluetoothserver_bluez.cpp \
        qbluetoothlocaldevice_bluez.cpp \
        qbluetoothtransferreply_bluez.cpp \


    # old versions of Bluez do not have the required BTLE symbols
    config_bluez_le {
        SOURCES +=  \
            qlowenergycontroller_bluez.cpp
    } else {
        message("Bluez version is too old to support Bluetooth Low Energy.")
        message("Only classic Bluetooth will be available.")
        DEFINES += QT_BLUEZ_NO_BTLE
        SOURCES += \
            qlowenergycontroller_p.cpp
    }

} else:android:!android-no-sdk {
    include(android/android.pri)
    DEFINES += QT_ANDROID_BLUETOOTH
    QT_FOR_PRIVATE += core-private androidextras

    ANDROID_PERMISSIONS = \
        android.permission.BLUETOOTH \
        android.permission.BLUETOOTH_ADMIN \
        android.permission.ACCESS_COARSE_LOCATION # since Android 6.0 (API lvl 23)
    ANDROID_BUNDLED_JAR_DEPENDENCIES = \
        jar/QtAndroidBluetooth-bundled.jar:org.qtproject.qt5.android.bluetooth.QtBluetoothBroadcastReceiver
    ANDROID_JAR_DEPENDENCIES = \
        jar/QtAndroidBluetooth.jar:org.qtproject.qt5.android.bluetooth.QtBluetoothBroadcastReceiver

    SOURCES += \
        qbluetoothdevicediscoveryagent_android.cpp \
        qbluetoothlocaldevice_android.cpp \
        qbluetoothserviceinfo_android.cpp \
        qbluetoothservicediscoveryagent_android.cpp \
        qbluetoothsocket_android.cpp \
        qbluetoothserver_android.cpp \
        qlowenergycontroller_android.cpp

} else:osx {
    DEFINES += QT_OSX_BLUETOOTH
    LIBS_PRIVATE += -framework Foundation -framework IOBluetooth

    include(osx/osxbt.pri)
    OBJECTIVE_SOURCES += \
        qbluetoothlocaldevice_osx.mm \
        qbluetoothdevicediscoveryagent_osx.mm \
        qbluetoothserviceinfo_osx.mm \
        qbluetoothservicediscoveryagent_osx.mm \
        qbluetoothsocket_osx.mm \
        qbluetoothserver_osx.mm \
        qbluetoothtransferreply_osx.mm \
        qlowenergycontroller_osx.mm \
        qlowenergyservice_osx.mm

    SOURCES += \
        qlowenergycontroller_p.cpp

    PRIVATE_HEADERS += qbluetoothsocket_osx_p.h \
                       qbluetoothserver_osx_p.h \
                       qbluetoothtransferreply_osx_p.h \
                       qbluetoothtransferreply_osx_p.h \
                       qbluetoothdevicediscoverytimer_osx_p.h \
                       qlowenergycontroller_osx_p.h

    SOURCES -= qbluetoothdevicediscoveryagent.cpp
    SOURCES -= qbluetoothserviceinfo.cpp
    SOURCES -= qbluetoothservicediscoveryagent.cpp
    SOURCES -= qbluetoothsocket.cpp
    SOURCES -= qbluetoothserver.cpp
    SOURCES -= qlowenergyservice_p.cpp
    SOURCES -= qlowenergyservice.cpp
    SOURCES -= qlowenergycontroller.cpp
    SOURCES -= qlowenergycontroller_p.cpp
} else:ios {
    DEFINES += QT_IOS_BLUETOOTH
    LIBS_PRIVATE += -framework Foundation -framework CoreBluetooth

    OBJECTIVE_SOURCES += \
        qbluetoothdevicediscoveryagent_ios.mm \
        qlowenergycontroller_osx.mm \
        qlowenergyservice_osx.mm

    PRIVATE_HEADERS += \
        qlowenergycontroller_osx_p.h \
        qbluetoothdevicediscoverytimer_osx_p.h

    include(osx/osxbt.pri)
    SOURCES += \
        qbluetoothdevicediscoveryagent_p.cpp \
        qbluetoothlocaldevice_p.cpp \
        qbluetoothserviceinfo_p.cpp \
        qbluetoothservicediscoveryagent_p.cpp \
        qbluetoothsocket_p.cpp \
        qbluetoothserver_p.cpp \
        qlowenergycontroller_p.cpp

    SOURCES -= qbluetoothdevicediscoveryagent.cpp
    SOURCES -= qlowenergycontroller_p.cpp
    SOURCES -= qlowenergyservice.cpp
    SOURCES -= qlowenergycontroller.cpp
} else {
    message("Unsupported Bluetooth platform, will not build a working QtBluetooth library.")
    message("Either no Qt D-Bus found or no BlueZ headers available.")
    include(dummy/dummy.pri)
    SOURCES += \
        qbluetoothdevicediscoveryagent_p.cpp \
        qbluetoothlocaldevice_p.cpp \
        qbluetoothserviceinfo_p.cpp \
        qbluetoothservicediscoveryagent_p.cpp \
        qbluetoothsocket_p.cpp \
        qbluetoothserver_p.cpp \
        qlowenergycontroller_p.cpp
}

OTHER_FILES +=

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

load(qt_module)
