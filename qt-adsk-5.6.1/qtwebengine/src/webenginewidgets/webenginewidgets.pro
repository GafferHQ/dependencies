TARGET = QtWebEngineWidgets

# For our export macros
DEFINES += QT_BUILD_WEBENGINEWIDGETS_LIB

QT += webenginecore widgets network quick
QT_PRIVATE += quick-private gui-private core-private

INCLUDEPATH += $$PWD api ../core ../core/api ../webengine/api

SOURCES = \
        api/qtwebenginewidgetsglobal.cpp \
        api/qwebenginecertificateerror.cpp \
        api/qwebenginedownloaditem.cpp \
        api/qwebenginefullscreenrequest.cpp \
        api/qwebenginehistory.cpp \
        api/qwebenginepage.cpp \
        api/qwebengineprofile.cpp \
        api/qwebenginescript.cpp \
        api/qwebenginescriptcollection.cpp \
        api/qwebenginesettings.cpp \
        api/qwebengineview.cpp \
        render_widget_host_view_qt_delegate_widget.cpp

HEADERS = \
        api/qtwebenginewidgetsglobal.h \
        api/qwebenginedownloaditem.h \
        api/qwebenginedownloaditem_p.h \
        api/qwebenginecertificateerror.h \
        api/qwebenginefullscreenrequest.h \
        api/qwebenginehistory.h \
        api/qwebenginepage.h \
        api/qwebenginepage_p.h \
        api/qwebengineprofile.h \
        api/qwebengineprofile_p.h \
        api/qwebenginescriptcollection.h \
        api/qwebenginescriptcollection_p.h \
        api/qwebenginesettings.h \
        api/qwebengineview.h \
        api/qwebengineview_p.h \
        render_widget_host_view_qt_delegate_widget.h

!contains(WEBENGINE_CONFIG, no_ui_delegates) {
    SOURCES += ui/messagebubblewidget.cpp
    HEADERS += ui/messagebubblewidget_p.h
    DEFINES += QT_UI_DELEGATES
}

load(qt_module)
