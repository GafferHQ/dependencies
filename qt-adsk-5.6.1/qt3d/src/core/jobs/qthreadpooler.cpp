/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

#include "qthreadpooler_p.h"
#include "dependencyhandler_p.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QThreadPooler::QThreadPooler(QObject *parent)
    : QObject(parent),
      m_futureInterface(Q_NULLPTR),
      m_mutex(new QMutex(QMutex::NonRecursive)),
      m_taskCount(0)
{
    // Ensures that threads will never be recycled
    m_threadPool.setExpiryTimeout(-1);
}

QThreadPooler::~QThreadPooler()
{
    // Wait till all tasks are finished before deleting mutex
    QMutexLocker locker(m_mutex);
    locker.unlock();

    delete m_mutex;
}

void QThreadPooler::setDependencyHandler(DependencyHandler *handler)
{
    m_dependencyHandler = handler;
    m_dependencyHandler->setMutex(m_mutex);
}

void QThreadPooler::enqueueTasks(const QVector<RunnableInterface *> &tasks)
{
    // The caller have to set the mutex
    const QVector<RunnableInterface *>::const_iterator end = tasks.cend();

    for (QVector<RunnableInterface *>::const_iterator it = tasks.cbegin();
         it != end; ++it) {
        if (!m_dependencyHandler->hasDependency((*it)) && !(*it)->reserved()) {
            (*it)->setReserved(true);
            (*it)->setPooler(this);
            m_threadPool.start((*it));
        }
    }
}

void QThreadPooler::taskFinished(RunnableInterface *task)
{
    const QMutexLocker locker(m_mutex);

    release();

    if (task->dependencyHandler()) {
        const QVector<RunnableInterface *> freedTasks = m_dependencyHandler->freeDependencies(task);
        if (!freedTasks.empty())
            enqueueTasks(freedTasks);
    }

    if (currentCount() == 0) {
        if (m_futureInterface) {
            m_futureInterface->reportFinished();
            delete m_futureInterface;
        }
        m_futureInterface = Q_NULLPTR;
    }
}

QFuture<void> QThreadPooler::mapDependables(QVector<RunnableInterface *> &taskQueue)
{
    const QMutexLocker locker(m_mutex);

    if (!m_futureInterface)
        m_futureInterface = new QFutureInterface<void>();
    if (!taskQueue.empty())
        m_futureInterface->reportStarted();

    acquire(taskQueue.size());
    enqueueTasks(taskQueue);

    return QFuture<void>(m_futureInterface);
}

QFuture<void> QThreadPooler::future()
{
    const QMutexLocker locker(m_mutex);

    if (!m_futureInterface)
        return QFuture<void>();
    else
        return QFuture<void>(m_futureInterface);
}

void QThreadPooler::acquire(int add)
{
    // The caller have to set the mutex

    m_taskCount.fetchAndAddOrdered(add);
}

void QThreadPooler::release()
{
    // The caller have to set the mutex

    m_taskCount.fetchAndAddOrdered(-1);
}

int QThreadPooler::currentCount() const
{
    // The caller have to set the mutex

    return m_taskCount.load();
}

int QThreadPooler::maxThreadCount() const
{
    return m_threadPool.maxThreadCount();
}

} // namespace Qt3DCore

QT_END_NAMESPACE
