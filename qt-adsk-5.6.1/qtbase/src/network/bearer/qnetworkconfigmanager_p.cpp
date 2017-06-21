/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qnetworkconfigmanager_p.h"
#include "qbearerplugin_p.h"

#include <QtCore/private/qfactoryloader_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qtimer.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qthread.h>
#include <QtCore/private/qcoreapplication_p.h>
#include <QtCore/private/qthread_p.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>


#ifndef QT_NO_BEARERMANAGEMENT

QT_BEGIN_NAMESPACE

#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader,
                          (QBearerEngineFactoryInterface_iid, QLatin1String("/bearer")))
#endif

QNetworkConfigurationManagerPrivate::QNetworkConfigurationManagerPrivate()
    : QObject(), pollTimer(0), mutex(QMutex::Recursive), forcedPolling(0), firstUpdate(true)
{
    qRegisterMetaType<QNetworkConfiguration>();
    qRegisterMetaType<QNetworkConfigurationPrivatePointer>();
}

void QNetworkConfigurationManagerPrivate::initialize()
{
    //Two stage construction, because we only want to do this heavyweight work for the winner of the Q_GLOBAL_STATIC race.
    bearerThread = new QDaemonThread();
    bearerThread->setObjectName(QStringLiteral("Qt bearer thread"));

    bearerThread->moveToThread(QCoreApplicationPrivate::mainThread()); // because cleanup() is called in main thread context.
    moveToThread(bearerThread);
    bearerThread->start();
    updateConfigurations();
}

QNetworkConfigurationManagerPrivate::~QNetworkConfigurationManagerPrivate()
{
    QMutexLocker locker(&mutex);

    qDeleteAll(sessionEngines);
    sessionEngines.clear();
    if (bearerThread)
        bearerThread->quit();
}

void QNetworkConfigurationManagerPrivate::cleanup()
{
    QThread* thread = bearerThread;
    deleteLater();
    if (thread->wait(5000))
        delete thread;
}

QNetworkConfiguration QNetworkConfigurationManagerPrivate::defaultConfiguration() const
{
    QMutexLocker locker(&mutex);

    foreach (QBearerEngine *engine, sessionEngines) {
        QNetworkConfigurationPrivatePointer ptr = engine->defaultConfiguration();
        if (ptr) {
            QNetworkConfiguration config;
            config.d = ptr;
            return config;
        }
    }

    // Engines don't have a default configuration.

    // Return first active snap
    QNetworkConfigurationPrivatePointer defaultConfiguration;

    foreach (QBearerEngine *engine, sessionEngines) {
        QHash<QString, QNetworkConfigurationPrivatePointer>::Iterator it;
        QHash<QString, QNetworkConfigurationPrivatePointer>::Iterator end;

        QMutexLocker locker(&engine->mutex);

        for (it = engine->snapConfigurations.begin(),
             end = engine->snapConfigurations.end(); it != end; ++it) {
            QNetworkConfigurationPrivatePointer ptr = it.value();

            QMutexLocker configLocker(&ptr->mutex);

            if ((ptr->state & QNetworkConfiguration::Active) == QNetworkConfiguration::Active) {
                QNetworkConfiguration config;
                config.d = ptr;
                return config;
            } else if (!defaultConfiguration) {
                if ((ptr->state & QNetworkConfiguration::Discovered) == QNetworkConfiguration::Discovered)
                    defaultConfiguration = ptr;
            }
        }
    }

    // No Active SNAPs return first Discovered SNAP.
    if (defaultConfiguration) {
        QNetworkConfiguration config;
        config.d = defaultConfiguration;
        return config;
    }

    /*
        No Active or Discovered SNAPs, find the perferred access point.
        The following priority order is used:

            1. Active Ethernet
            2. Active WLAN
            3. Active Other
            4. Discovered Ethernet
            5. Discovered WLAN
            6. Discovered Other
    */

    foreach (QBearerEngine *engine, sessionEngines) {
        QHash<QString, QNetworkConfigurationPrivatePointer>::Iterator it;
        QHash<QString, QNetworkConfigurationPrivatePointer>::Iterator end;

        QMutexLocker locker(&engine->mutex);

        for (it = engine->accessPointConfigurations.begin(),
             end = engine->accessPointConfigurations.end(); it != end; ++it) {
            QNetworkConfigurationPrivatePointer ptr = it.value();

            QMutexLocker configLocker(&ptr->mutex);
            QNetworkConfiguration::BearerType bearerType = ptr->bearerType;

            if ((ptr->state & QNetworkConfiguration::Discovered) == QNetworkConfiguration::Discovered) {
                if (!defaultConfiguration) {
                    defaultConfiguration = ptr;
                } else {
                    QMutexLocker defaultConfigLocker(&defaultConfiguration->mutex);

                    if (defaultConfiguration->state == ptr->state) {
                        switch (defaultConfiguration->bearerType) {
                        case QNetworkConfiguration::BearerEthernet:
                            // do nothing
                            break;
                        case QNetworkConfiguration::BearerWLAN:
                            // Ethernet beats WLAN
                            defaultConfiguration = ptr;
                            break;
                        default:
                            // Ethernet and WLAN beats other
                            if (bearerType == QNetworkConfiguration::BearerEthernet ||
                                bearerType == QNetworkConfiguration::BearerWLAN) {
                                defaultConfiguration = ptr;
                            }
                        }
                    } else {
                        // active beats discovered
                        if ((defaultConfiguration->state & QNetworkConfiguration::Active) !=
                            QNetworkConfiguration::Active) {
                            defaultConfiguration = ptr;
                        }
                    }
                }
            }
        }
    }

    // No Active InternetAccessPoint return first Discovered InternetAccessPoint.
    if (defaultConfiguration) {
        QNetworkConfiguration config;
        config.d = defaultConfiguration;
        return config;
    }

    return QNetworkConfiguration();
}

QList<QNetworkConfiguration> QNetworkConfigurationManagerPrivate::allConfigurations(QNetworkConfiguration::StateFlags filter) const
{
    QList<QNetworkConfiguration> result;

    QMutexLocker locker(&mutex);

    foreach (QBearerEngine *engine, sessionEngines) {
        QHash<QString, QNetworkConfigurationPrivatePointer>::Iterator it;
        QHash<QString, QNetworkConfigurationPrivatePointer>::Iterator end;

        QMutexLocker locker(&engine->mutex);

        //find all InternetAccessPoints
        for (it = engine->accessPointConfigurations.begin(),
             end = engine->accessPointConfigurations.end(); it != end; ++it) {
            QNetworkConfigurationPrivatePointer ptr = it.value();

            QMutexLocker configLocker(&ptr->mutex);

            if ((ptr->state & filter) == filter) {
                QNetworkConfiguration pt;
                pt.d = ptr;
                result << pt;
            }
        }

        //find all service networks
        for (it = engine->snapConfigurations.begin(),
             end = engine->snapConfigurations.end(); it != end; ++it) {
            QNetworkConfigurationPrivatePointer ptr = it.value();

            QMutexLocker configLocker(&ptr->mutex);

            if ((ptr->state & filter) == filter) {
                QNetworkConfiguration pt;
                pt.d = ptr;
                result << pt;
            }
        }
    }

    return result;
}

QNetworkConfiguration QNetworkConfigurationManagerPrivate::configurationFromIdentifier(const QString &identifier) const
{
    QNetworkConfiguration item;

    QMutexLocker locker(&mutex);

    foreach (QBearerEngine *engine, sessionEngines) {
        QMutexLocker locker(&engine->mutex);

        if (engine->accessPointConfigurations.contains(identifier))
            item.d = engine->accessPointConfigurations[identifier];
        else if (engine->snapConfigurations.contains(identifier))
            item.d = engine->snapConfigurations[identifier];
        else if (engine->userChoiceConfigurations.contains(identifier))
            item.d = engine->userChoiceConfigurations[identifier];
        else
            continue;

        return item;
    }

    return item;
}

bool QNetworkConfigurationManagerPrivate::isOnline() const
{
    QMutexLocker locker(&mutex);

    // We need allConfigurations since onlineConfigurations is filled with queued connections
    // and thus is not always (more importantly just after creation) up to date
    return !allConfigurations(QNetworkConfiguration::Active).isEmpty();
}

QNetworkConfigurationManager::Capabilities QNetworkConfigurationManagerPrivate::capabilities() const
{
    QMutexLocker locker(&mutex);

    QNetworkConfigurationManager::Capabilities capFlags;

    foreach (QBearerEngine *engine, sessionEngines)
        capFlags |= engine->capabilities();

    return capFlags;
}

void QNetworkConfigurationManagerPrivate::configurationAdded(QNetworkConfigurationPrivatePointer ptr)
{
    QMutexLocker locker(&mutex);

    if (!firstUpdate) {
        QNetworkConfiguration item;
        item.d = ptr;
        emit configurationAdded(item);
    }

    ptr->mutex.lock();
    if (ptr->state == QNetworkConfiguration::Active) {
        ptr->mutex.unlock();
        onlineConfigurations.insert(ptr->id);
        if (!firstUpdate && onlineConfigurations.count() == 1)
            emit onlineStateChanged(true);
    } else {
        ptr->mutex.unlock();
    }
}

void QNetworkConfigurationManagerPrivate::configurationRemoved(QNetworkConfigurationPrivatePointer ptr)
{
    QMutexLocker locker(&mutex);

    ptr->mutex.lock();
    ptr->isValid = false;
    ptr->mutex.unlock();

    if (!firstUpdate) {
        QNetworkConfiguration item;
        item.d = ptr;
        emit configurationRemoved(item);
    }

    onlineConfigurations.remove(ptr->id);
    if (!firstUpdate && onlineConfigurations.isEmpty())
        emit onlineStateChanged(false);
}

void QNetworkConfigurationManagerPrivate::configurationChanged(QNetworkConfigurationPrivatePointer ptr)
{
    QMutexLocker locker(&mutex);

    if (!firstUpdate) {
        QNetworkConfiguration item;
        item.d = ptr;
        emit configurationChanged(item);
    }

    bool previous = !onlineConfigurations.isEmpty();

    ptr->mutex.lock();
    if (ptr->state == QNetworkConfiguration::Active)
        onlineConfigurations.insert(ptr->id);
    else
        onlineConfigurations.remove(ptr->id);
    ptr->mutex.unlock();

    bool online = !onlineConfigurations.isEmpty();

    if (!firstUpdate && online != previous)
        emit onlineStateChanged(online);
}

void QNetworkConfigurationManagerPrivate::updateConfigurations()
{
#ifndef QT_NO_LIBRARY
    typedef QMultiMap<int, QString> PluginKeyMap;
    typedef PluginKeyMap::const_iterator PluginKeyMapConstIterator;
#endif
    QMutexLocker locker(&mutex);

    if (firstUpdate) {
        if (qobject_cast<QBearerEngine *>(sender()))
            return;

        updating = false;

#ifndef QT_NO_LIBRARY
        bool envOK  = false;
        const int skipGeneric = qgetenv("QT_EXCLUDE_GENERIC_BEARER").toInt(&envOK);
        QBearerEngine *generic = 0;
        QFactoryLoader *l = loader();
        const PluginKeyMap keyMap = l->keyMap();
        const PluginKeyMapConstIterator cend = keyMap.constEnd();
        QStringList addedEngines;
        for (PluginKeyMapConstIterator it = keyMap.constBegin(); it != cend; ++it) {
            const QString &key = it.value();
            if (addedEngines.contains(key))
                continue;

            addedEngines.append(key);
            if (QBearerEngine *engine = qLoadPlugin<QBearerEngine, QBearerEnginePlugin>(l, key)) {
                if (key == QLatin1String("generic"))
                    generic = engine;
                else
                    sessionEngines.append(engine);

                engine->moveToThread(bearerThread);

                connect(engine, SIGNAL(updateCompleted()),
                        this, SLOT(updateConfigurations()),
                        Qt::QueuedConnection);
                connect(engine, SIGNAL(configurationAdded(QNetworkConfigurationPrivatePointer)),
                        this, SLOT(configurationAdded(QNetworkConfigurationPrivatePointer)),
                        Qt::QueuedConnection);
                connect(engine, SIGNAL(configurationRemoved(QNetworkConfigurationPrivatePointer)),
                        this, SLOT(configurationRemoved(QNetworkConfigurationPrivatePointer)),
                        Qt::QueuedConnection);
                connect(engine, SIGNAL(configurationChanged(QNetworkConfigurationPrivatePointer)),
                        this, SLOT(configurationChanged(QNetworkConfigurationPrivatePointer)),
                        Qt::QueuedConnection);
            }
        }

        if (generic) {
            if (!envOK || skipGeneric <= 0)
                sessionEngines.append(generic);
            else
                delete generic;
        }
#endif // QT_NO_LIBRARY
    }

    QBearerEngine *engine = qobject_cast<QBearerEngine *>(sender());
    if (engine && !updatingEngines.isEmpty())
        updatingEngines.remove(engine);

    if (updating && updatingEngines.isEmpty()) {
        updating = false;
        emit configurationUpdateComplete();
    }

    if (engine && !pollingEngines.isEmpty()) {
        pollingEngines.remove(engine);
        if (pollingEngines.isEmpty())
            startPolling();
    }

    if (firstUpdate) {
        firstUpdate = false;
        QList<QBearerEngine*> enginesToInitialize = sessionEngines; //shallow copy the list in case it is modified when we unlock mutex
        locker.unlock();
        foreach (QBearerEngine* engine, enginesToInitialize) {
            QMetaObject::invokeMethod(engine, "initialize", Qt::BlockingQueuedConnection);
        }
    }
}

void QNetworkConfigurationManagerPrivate::performAsyncConfigurationUpdate()
{
    QMutexLocker locker(&mutex);

    if (sessionEngines.isEmpty()) {
        emit configurationUpdateComplete();
        return;
    }

    updating = true;

    foreach (QBearerEngine *engine, sessionEngines) {
        updatingEngines.insert(engine);
        QMetaObject::invokeMethod(engine, "requestUpdate");
    }
}

QList<QBearerEngine *> QNetworkConfigurationManagerPrivate::engines() const
{
    QMutexLocker locker(&mutex);

    return sessionEngines;
}

void QNetworkConfigurationManagerPrivate::startPolling()
{
    QMutexLocker locker(&mutex);
    if (!pollTimer) {
        pollTimer = new QTimer(this);
        bool ok;
        int interval = qgetenv("QT_BEARER_POLL_TIMEOUT").toInt(&ok);
        if (!ok)
            interval = 10000;//default 10 seconds
        pollTimer->setInterval(interval);
        pollTimer->setSingleShot(true);
        connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollEngines()));
    }

    if (pollTimer->isActive())
        return;

    foreach (QBearerEngine *engine, sessionEngines) {
        if (engine->requiresPolling() && (forcedPolling || engine->configurationsInUse())) {
            pollTimer->start();
            break;
        }
    }
    performAsyncConfigurationUpdate();
}

void QNetworkConfigurationManagerPrivate::pollEngines()
{
    QMutexLocker locker(&mutex);

    foreach (QBearerEngine *engine, sessionEngines) {
        if (engine->requiresPolling() && (forcedPolling || engine->configurationsInUse())) {
            pollingEngines.insert(engine);
            QMetaObject::invokeMethod(engine, "requestUpdate");
        }
    }
}

void QNetworkConfigurationManagerPrivate::enablePolling()
{
    QMutexLocker locker(&mutex);

    ++forcedPolling;

    if (forcedPolling == 1)
        QMetaObject::invokeMethod(this, "startPolling");
}

void QNetworkConfigurationManagerPrivate::disablePolling()
{
    QMutexLocker locker(&mutex);

    --forcedPolling;
}

QT_END_NAMESPACE

#endif // QT_NO_BEARERMANAGEMENT
