TEMPLATE = subdirs

!android:linux*:qtHaveModule(dbus) {
    SUBDIRS += generic
    SUBDIRS += connman networkmanager
}

#win32:SUBDIRS += nla
win32:SUBDIRS += generic
blackberry:SUBDIRS += blackberry
win32:!wince:!winrt: SUBDIRS += nativewifi
mac:contains(QT_CONFIG, corewlan):SUBDIRS += corewlan
mac:SUBDIRS += generic
android:!android-no-sdk:SUBDIRS += android

isEmpty(SUBDIRS):SUBDIRS = generic
