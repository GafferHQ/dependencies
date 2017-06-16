/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qeventdispatcher_glib_p.h"

#include "qguiapplication.h"

#include "qplatformdefs.h"

#include <glib.h>
#include "private/qguiapplication_p.h"

#include <qdebug.h>

QT_BEGIN_NAMESPACE

struct GUserEventSource
{
    GSource source;
    QPAEventDispatcherGlib *q;
};

static gboolean userEventSourcePrepare(GSource *s, gint *timeout)
{
    Q_UNUSED(s)
    Q_UNUSED(timeout)

    return QWindowSystemInterface::windowSystemEventsQueued() > 0;
}

static gboolean userEventSourceCheck(GSource *source)
{
    return userEventSourcePrepare(source, 0);
}

static gboolean userEventSourceDispatch(GSource *source, GSourceFunc, gpointer)
{
    GUserEventSource *userEventSource = reinterpret_cast<GUserEventSource *>(source);
    QPAEventDispatcherGlib *dispatcher = userEventSource->q;
    QWindowSystemInterface::sendWindowSystemEvents(dispatcher->m_flags);
    return true;
}

static GSourceFuncs userEventSourceFuncs = {
    userEventSourcePrepare,
    userEventSourceCheck,
    userEventSourceDispatch,
    NULL,
    NULL,
    NULL
};

QPAEventDispatcherGlibPrivate::QPAEventDispatcherGlibPrivate(GMainContext *context)
    : QEventDispatcherGlibPrivate(context)
{
    Q_Q(QPAEventDispatcherGlib);
    userEventSource = reinterpret_cast<GUserEventSource *>(g_source_new(&userEventSourceFuncs,
                                                                       sizeof(GUserEventSource)));
    userEventSource->q = q;
    g_source_set_can_recurse(&userEventSource->source, true);
    g_source_attach(&userEventSource->source, mainContext);
}


QPAEventDispatcherGlib::QPAEventDispatcherGlib(QObject *parent)
    : QEventDispatcherGlib(*new QPAEventDispatcherGlibPrivate, parent)
    , m_flags(QEventLoop::AllEvents)
{
    Q_D(QPAEventDispatcherGlib);
    d->userEventSource->q = this;
}

QPAEventDispatcherGlib::~QPAEventDispatcherGlib()
{
    Q_D(QPAEventDispatcherGlib);

    g_source_destroy(&d->userEventSource->source);
    g_source_unref(&d->userEventSource->source);
    d->userEventSource = 0;
}

bool QPAEventDispatcherGlib::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    m_flags = flags;
    return QEventDispatcherGlib::processEvents(m_flags);
}

QT_END_NAMESPACE
