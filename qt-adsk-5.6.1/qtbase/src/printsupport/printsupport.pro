TARGET     = QtPrintSupport
QT = core-private gui-private widgets-private

DEFINES   += QT_NO_USING_NAMESPACE

QMAKE_DOCS = $$PWD/doc/qtprintsupport.qdocconf

QMAKE_LIBS += $$QMAKE_LIBS_PRINTSUPPORT

include(kernel/kernel.pri)
include(widgets/widgets.pri)
include(dialogs/dialogs.pri)

MODULE_PLUGIN_TYPES = \
    printsupport
load(qt_module)
