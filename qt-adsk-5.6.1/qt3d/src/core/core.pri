INCLUDEPATH += $$PWD

# Aspects
include (./aspects/aspects.pri)
# Jobs
include (./jobs/jobs.pri)
# Nodes
include (./nodes/nodes.pri)
# Qml Components
include (./core-components/core-components.pri)
#Transformations
include (./transforms/transforms.pri)
# Resources Management
include (./resources/resources.pri)
# Services
include (./services/services.pri)

HEADERS += \
    $$PWD/qt3dcore_global.h \
    $$PWD/qtickclock_p.h \
    $$PWD/qscheduler_p.h \
    $$PWD/corelogging_p.h \
    $$PWD/qscenechange.h \
    $$PWD/qscenepropertychange.h \
    $$PWD/qscenechange_p.h \
    $$PWD/qscenepropertychange_p.h \
    $$PWD/qsceneobserverinterface_p.h \
    $$PWD/qpostman_p.h \
    $$PWD/qbackendscenepropertychange.h \
    $$PWD/qbackendscenepropertychange_p.h \
    $$PWD/qobservableinterface_p.h \
    $$PWD/qobserverinterface_p.h \
    $$PWD/qlockableobserverinterface_p.h \
    $$PWD/qchangearbiter_p.h \
    $$PWD/qbackendnodefactory_p.h \
    $$PWD/qray3d.h \
    $$PWD/qt3dcore_global_p.h \
    $$PWD/qscene_p.h

SOURCES += \
    $$PWD/qtickclock.cpp \
    $$PWD/qscheduler.cpp \
    $$PWD/qchangearbiter.cpp \
    $$PWD/corelogging.cpp \
    $$PWD/qobservableinterface.cpp \
    $$PWD/qobserverinterface.cpp \
    $$PWD/qlockableobserverinterface.cpp \
    $$PWD/qscenechange.cpp \
    $$PWD/qscenepropertychange.cpp \
    $$PWD/qsceneobserverinterface.cpp \
    $$PWD/qpostman.cpp \
    $$PWD/qscene.cpp \
    $$PWD/qbackendscenepropertychange.cpp \
    $$PWD/qbackendnodefactory.cpp \
    $$PWD/qray3d.cpp
