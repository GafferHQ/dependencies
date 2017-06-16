/****************************************************************************
**
** Copyright (C) 2013 Samuel Gaist <samuel.gaist@edeltech.ch>
** Copyright (C) 2013 Teo Mrnjavac <teo@kde.org>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qplatformsessionmanager.h"

#include "qguiapplication_p.h"

#ifndef QT_NO_SESSIONMANAGER

QT_BEGIN_NAMESPACE

QPlatformSessionManager::QPlatformSessionManager(const QString &id, const QString &key)
    : m_sessionId(id),
      m_sessionKey(key),
      m_restartHint(QSessionManager::RestartIfRunning)
{
}

QPlatformSessionManager::~QPlatformSessionManager()
{
}

QString QPlatformSessionManager::sessionId() const
{
    return m_sessionId;
}

QString QPlatformSessionManager::sessionKey() const
{
    return m_sessionKey;
}

bool QPlatformSessionManager::allowsInteraction()
{
    return false;
}

bool QPlatformSessionManager::allowsErrorInteraction()
{
    return false;
}

void QPlatformSessionManager::release()
{
}

void QPlatformSessionManager::cancel()
{
}

void QPlatformSessionManager::setRestartHint(QSessionManager::RestartHint restartHint)
{
    m_restartHint = restartHint;
}

QSessionManager::RestartHint QPlatformSessionManager::restartHint() const
{
    return m_restartHint;
}

void QPlatformSessionManager::setRestartCommand(const QStringList &command)
{
    m_restartCommand = command;
}

QStringList QPlatformSessionManager::restartCommand() const
{
    return m_restartCommand;
}

void QPlatformSessionManager::setDiscardCommand(const QStringList &command)
{
    m_discardCommand = command;
}

QStringList QPlatformSessionManager::discardCommand() const
{
    return m_discardCommand;
}

void QPlatformSessionManager::setManagerProperty(const QString &name, const QString &value)
{
    Q_UNUSED(name)
    Q_UNUSED(value)
}

void QPlatformSessionManager::setManagerProperty(const QString &name, const QStringList &value)
{
    Q_UNUSED(name)
    Q_UNUSED(value)
}

bool QPlatformSessionManager::isPhase2() const
{
    return false;
}

void QPlatformSessionManager::requestPhase2()
{
}

void QPlatformSessionManager::appCommitData()
{
    qGuiApp->d_func()->commitData();
}

void QPlatformSessionManager::appSaveState()
{
    qGuiApp->d_func()->saveState();
}

QT_END_NAMESPACE

#endif // QT_NO_SESSIONMANAGER
