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

#include "qopenwfdnativeinterface.h"

#include "qopenwfdscreen.h"
#include "qopenwfdwindow.h"
#include "qopenwfdglcontext.h"

#include <private/qguiapplication_p.h>
#include <QtCore/QMap>

#include <QtCore/QDebug>

#include <QtGui/qguiglcontext_qpa.h>

class QOpenWFDResourceMap : public QMap<QByteArray, QOpenWFDNativeInterface::ResourceType>
{
public:
    QOpenWFDResourceMap()
        :QMap<QByteArray, QOpenWFDNativeInterface::ResourceType>()
    {
        insert("wfddevice",QOpenWFDNativeInterface::WFDDevice);
        insert("egldisplay",QOpenWFDNativeInterface::EglDisplay);
        insert("eglcontext",QOpenWFDNativeInterface::EglContext);
        insert("wfdport",QOpenWFDNativeInterface::WFDPort);
        insert("wfdpipeline",QOpenWFDNativeInterface::WFDPipeline);
    }
};

Q_GLOBAL_STATIC(QOpenWFDResourceMap, qOpenWFDResourceMap)

void *QOpenWFDNativeInterface::nativeResourceForContext(const QByteArray &resourceString, QOpenGLContext *context)
{
    QByteArray lowerCaseResource = resourceString.toLower();
    ResourceType resource = qOpenWFDResourceMap()->value(lowerCaseResource);
    void *result = 0;
    switch (resource) {
    case EglContext:
        result = eglContextForContext(context);
        break;
    default:
        result = 0;
    }
    return result;
}

void *QOpenWFDNativeInterface::nativeResourceForWindow(const QByteArray &resourceString, QWindow *window)
{
    QByteArray lowerCaseResource = resourceString.toLower();
    ResourceType resource = qOpenWFDResourceMap()->value(lowerCaseResource);
    void *result = 0;
    switch (resource) {
    //What should we do for int wfd handles? This is clearly not the solution
    case WFDDevice:
        result = (void *)wfdDeviceForWindow(window);
        break;
    case WFDPort:
        result = (void *)wfdPortForWindow(window);
        break;
    case WFDPipeline:
        result = (void *)wfdPipelineForWindow(window);
        break;
    case EglDisplay:
        result = eglDisplayForWindow(window);
        break;
    default:
        result = 0;
    }
    return result;
}

WFDHandle QOpenWFDNativeInterface::wfdDeviceForWindow(QWindow *window)
{
    QOpenWFDWindow *openWFDwindow = static_cast<QOpenWFDWindow *>(window->handle());
    return openWFDwindow->port()->device()->handle();
}

WFDHandle QOpenWFDNativeInterface::wfdPortForWindow(QWindow *window)
{
    QOpenWFDWindow *openWFDwindow = static_cast<QOpenWFDWindow *>(window->handle());
    return openWFDwindow->port()->handle();
}

WFDHandle QOpenWFDNativeInterface::wfdPipelineForWindow(QWindow *window)

{
    QOpenWFDWindow *openWFDwindow = static_cast<QOpenWFDWindow *>(window->handle());
    return openWFDwindow->port()->pipeline();

}

void *QOpenWFDNativeInterface::eglDisplayForWindow(QWindow *window)
{
    QOpenWFDWindow *openWFDwindow = static_cast<QOpenWFDWindow *>(window->handle());
    return openWFDwindow->port()->device()->eglDisplay();
}

void * QOpenWFDNativeInterface::eglContextForContext(QOpenGLContext *context)
{
    QOpenWFDGLContext *openWFDContext = static_cast<QOpenWFDGLContext *>(context->handle());
    return openWFDContext->eglContext();
}
