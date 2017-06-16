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

#include "qwaylandnativeinterface_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandextendedsurface_p.h"
#include "qwaylandintegration_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandwindowmanagerintegration_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandwlshellsurface_p.h"
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/QScreen>
#include <QtWaylandClient/private/qwaylandclientbufferintegration_p.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandNativeInterface::QWaylandNativeInterface(QWaylandIntegration *integration)
    : m_integration(integration)
{
}

void *QWaylandNativeInterface::nativeResourceForIntegration(const QByteArray &resourceString)
{
    QByteArray lowerCaseResource = resourceString.toLower();

    if (lowerCaseResource == "display" || lowerCaseResource == "wl_display" || lowerCaseResource == "nativedisplay")
        return m_integration->display()->wl_display();
    if (lowerCaseResource == "compositor")
        return const_cast<wl_compositor *>(m_integration->display()->wl_compositor());
    if (lowerCaseResource == "server_buffer_integration")
        return m_integration->serverBufferIntegration();

    if (lowerCaseResource == "egldisplay" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResource(QWaylandClientBufferIntegration::EglDisplay);

    return 0;
}

void *QWaylandNativeInterface::nativeResourceForWindow(const QByteArray &resourceString, QWindow *window)
{
    QByteArray lowerCaseResource = resourceString.toLower();

    if (lowerCaseResource == "display")
        return m_integration->display()->wl_display();
    if (lowerCaseResource == "compositor")
        return const_cast<wl_compositor *>(m_integration->display()->wl_compositor());
    if (lowerCaseResource == "surface") {
        return ((QWaylandWindow *) window->handle())->object();
    }
    if (lowerCaseResource == "wl_shell_surface") {
        QWaylandWindow *w = (QWaylandWindow *) window->handle();
        if (!w)
            return NULL;
        QWaylandWlShellSurface *s = qobject_cast<QWaylandWlShellSurface *>(w->shellSurface());
        if (!s)
            return NULL;
        return s->object();
    }
    if (lowerCaseResource == "egldisplay" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResource(QWaylandClientBufferIntegration::EglDisplay);

    return NULL;
}

void *QWaylandNativeInterface::nativeResourceForScreen(const QByteArray &resourceString, QScreen *screen)
{
    QByteArray lowerCaseResource = resourceString.toLower();

    if (lowerCaseResource == "output")
        return ((QWaylandScreen *) screen->handle())->output();

    return NULL;
}

void *QWaylandNativeInterface::nativeResourceForContext(const QByteArray &resource, QOpenGLContext *context)
{
    QByteArray lowerCaseResource = resource.toLower();

    if (lowerCaseResource == "eglconfig" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResourceForContext(QWaylandClientBufferIntegration::EglConfig, context->handle());

    if (lowerCaseResource == "eglcontext" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResourceForContext(QWaylandClientBufferIntegration::EglContext, context->handle());

    if (lowerCaseResource == "egldisplay" && m_integration->clientBufferIntegration())
        return m_integration->clientBufferIntegration()->nativeResourceForContext(QWaylandClientBufferIntegration::EglDisplay, context->handle());

    return 0;
}

QVariantMap QWaylandNativeInterface::windowProperties(QPlatformWindow *window) const
{
    QWaylandWindow *waylandWindow = static_cast<QWaylandWindow *>(window);
    return waylandWindow->properties();
}

QVariant QWaylandNativeInterface::windowProperty(QPlatformWindow *window, const QString &name) const
{
    QWaylandWindow *waylandWindow = static_cast<QWaylandWindow *>(window);
    return waylandWindow->property(name);
}

QVariant QWaylandNativeInterface::windowProperty(QPlatformWindow *window, const QString &name, const QVariant &defaultValue) const
{
    QWaylandWindow *waylandWindow = static_cast<QWaylandWindow *>(window);
    return waylandWindow->property(name, defaultValue);
}

void QWaylandNativeInterface::setWindowProperty(QPlatformWindow *window, const QString &name, const QVariant &value)
{
    QWaylandWindow *wlWindow = static_cast<QWaylandWindow*>(window);
    wlWindow->sendProperty(name, value);
}

void QWaylandNativeInterface::emitWindowPropertyChanged(QPlatformWindow *window, const QString &name)
{
    emit windowPropertyChanged(window,name);
}

}

QT_END_NAMESPACE
