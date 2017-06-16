/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qlogicaspect.h"
#include "qlogicaspect_p.h"
#include "executor_p.h"
#include "handler_p.h"
#include "manager_p.h"
#include "qlogiccomponent.h"

#include <Qt3DCore/qnode.h>
#include <Qt3DCore/private/qchangearbiter_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qservicelocator_p.h>

#include <QThread>
#include <QWindow>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DLogic {

QLogicAspectPrivate::QLogicAspectPrivate()
    : QAbstractAspectPrivate()
    , m_time(0)
    , m_initialized(false)
    , m_manager(new Logic::Manager)
    , m_executor(new Logic::Executor)
    , m_callbackJob(new Logic::CallbackJob)
{
    m_callbackJob->setManager(m_manager.data());
    m_manager->setExecutor(m_executor.data());
}

QLogicAspect::QLogicAspect(QObject *parent)
    : QAbstractAspect(*new QLogicAspectPrivate(), parent)
{
    registerBackendTypes();
    d_func()->m_manager->setLogicAspect(this);
}

/*! \internal */
QLogicAspect::QLogicAspect(QLogicAspectPrivate &dd, QObject *parent)
    : QAbstractAspect(dd, parent)
{
    registerBackendTypes();
    d_func()->m_manager->setLogicAspect(this);
}

void QLogicAspect::registerBackendTypes()
{
    registerBackendType<QLogicComponent>(QBackendNodeFunctorPtr(new Logic::HandlerFunctor(d_func()->m_manager.data())));
}

QVector<QAspectJobPtr> QLogicAspect::jobsToExecute(qint64 time)
{
    Q_D(QLogicAspect);
    const qint64 deltaTime = time - d->m_time;
    const float dt = static_cast<const float>(deltaTime) / 1.0e9;
    d->m_manager->setDeltaTime(dt);
    d->m_time = time;

    // Create jobs that will get exectued by the threadpool
    QVector<QAspectJobPtr> jobs;
    jobs.append(d->m_callbackJob);
    return jobs;
}

void QLogicAspect::onInitialize(const QVariantMap &data)
{
    Q_UNUSED(data);
}

void QLogicAspect::onCleanup()
{
}

void QLogicAspect::onStartup()
{
    Q_D(QLogicAspect);
    d->m_executor->setScene(d->m_arbiter->scene());
}

void QLogicAspect::onShutdown()
{
    Q_D(QLogicAspect);
    // Throw away any pending work that may deadlock during the shutdown procedure
    // when the main thread waits for any queued jobs to finish.
    d->m_executor->clearQueueAndProceed();
}

} // namespace Qt3DLogic

QT_END_NAMESPACE

QT3D_REGISTER_NAMESPACED_ASPECT("logic", QT_PREPEND_NAMESPACE(Qt3DLogic), QLogicAspect)

