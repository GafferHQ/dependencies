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

#include "qopenwfdintegration.h"
#include "qopenwfdscreen.h"
#include "qopenwfdnativeinterface.h"
#include "qopenwfddevice.h"
#include "qopenwfdwindow.h"
#include "qopenwfdglcontext.h"
#include "qopenwfdbackingstore.h"

#include <QtPlatformSupport/private/qgenericunixprintersupport_p.h>

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QScreen>

#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>

#include <stdio.h>

#include <WF/wfd.h>

QOpenWFDIntegration::QOpenWFDIntegration()
    : QPlatformIntegration()
    , mPrinterSupport(new QGenericUnixPrinterSupport)
{
    int numberOfDevices = wfdEnumerateDevices(0,0,0);

    WFDint devices[numberOfDevices];
    int actualNumberOfDevices = wfdEnumerateDevices(devices,numberOfDevices,0);
    Q_ASSERT(actualNumberOfDevices == numberOfDevices);

    for (int i = 0; i < actualNumberOfDevices; i++) {
        mDevices.append(new QOpenWFDDevice(this,devices[i]));
    }

    mFontDatabase = new QGenericUnixFontDatabase();
    mNativeInterface = new QOpenWFDNativeInterface;
}

QOpenWFDIntegration::~QOpenWFDIntegration()
{
    //don't delete screens since they are deleted by the devices
    qDebug() << "deleting platform integration";
    for (int i = 0; i < mDevices.size(); i++) {
        delete mDevices[i];
    }

    delete mFontDatabase;
    delete mNativeInterface;
    delete mPrinterSupport;
}

bool QOpenWFDIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case OpenGL: return true;
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *QOpenWFDIntegration::createPlatformWindow(QWindow *window) const
{
    return new QOpenWFDWindow(window);
}

QPlatformOpenGLContext *QOpenWFDIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    QOpenWFDScreen *screen = static_cast<QOpenWFDScreen *>(context->screen()->handle());

    return new QOpenWFDGLContext(screen->port()->device());
}

QPlatformBackingStore *QOpenWFDIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QOpenWFDBackingStore(window);
}

QAbstractEventDispatcher *QOpenWFDIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QPlatformFontDatabase *QOpenWFDIntegration::fontDatabase() const
{
    return mFontDatabase;
}

QPlatformNativeInterface * QOpenWFDIntegration::nativeInterface() const
{
    return mNativeInterface;
}

QPlatformPrinterSupport * QOpenWFDIntegration::printerSupport() const
{
    return mPrinterSupport;
}

void QOpenWFDIntegration::addScreen(QOpenWFDScreen *screen)
{
    screenAdded(screen);
}

void QOpenWFDIntegration::destroyScreen(QOpenWFDScreen *screen)
{
    QPlatformIntegration::destroyScreen(screen);
}
