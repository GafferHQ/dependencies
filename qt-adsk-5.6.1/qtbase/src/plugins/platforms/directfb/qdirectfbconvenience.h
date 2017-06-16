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

#ifndef QDIRECTFBCONVENIENCE_H
#define QDIRECTFBCONVENIENCE_H

#include <QtGui/qimage.h>
#include <QtCore/QHash>
#include <QtCore/QEvent>
#include <QtGui/QPixmap>

#include <directfb.h>

QT_BEGIN_NAMESPACE

class QDirectFbScreen;
class QPlatformScreen;

class QDirectFbKeyMap: public QHash<DFBInputDeviceKeySymbol, Qt::Key>
{
public:
    QDirectFbKeyMap();
};


class QDirectFbConvenience
{
public:
    static QImage::Format imageFormatFromSurfaceFormat(const DFBSurfacePixelFormat format, const DFBSurfaceCapabilities caps);
    static bool pixelFomatHasAlpha(const DFBSurfacePixelFormat format) { return (1 << 16) & format; }
    static int colorDepthForSurface(const DFBSurfacePixelFormat format);

    //This is set by the graphicssystem constructor
    static IDirectFB *dfbInterface();
    static IDirectFBDisplayLayer *dfbDisplayLayer(int display = DLID_PRIMARY);

    static IDirectFBSurface *dfbSurfaceForPlatformPixmap(QPlatformPixmap *);

    static Qt::MouseButton mouseButton(DFBInputDeviceButtonIdentifier identifier);
    static Qt::MouseButtons mouseButtons(DFBInputDeviceButtonMask mask);
    static Qt::KeyboardModifiers keyboardModifiers(DFBInputDeviceModifierMask mask);
    static QEvent::Type eventType(DFBWindowEventType type);

    static QDirectFbKeyMap *keyMap();

private:
    static QDirectFbKeyMap *dfbKeymap;
    friend class QDirectFbIntegration;
};

template <typename T> struct QDirectFBInterfaceCleanupHandler
{
    static void cleanup(T *t)
    {
        if (!t)
            return;
        t->Release(t);
    }
};

template <typename T>
class QDirectFBPointer : public QScopedPointer<T, QDirectFBInterfaceCleanupHandler<T> >
{
public:
    QDirectFBPointer(T *t = 0)
        : QScopedPointer<T, QDirectFBInterfaceCleanupHandler<T> >(t)
    {}

    T** outPtr()
    {
        this->reset(0);
        return &this->d;
    }
};

// Helper conversions from internal to DFB types
QDirectFbScreen *toDfbScreen(QWindow *window);
IDirectFBDisplayLayer *toDfbLayer(QPlatformScreen *screen);

#define QDFB_STRINGIFY(x) #x
#define QDFB_TOSTRING(x) QDFB_STRINGIFY(x)
#define QDFB_PRETTY \
    (__FILE__ ":" QDFB_TOSTRING(__LINE__))

QT_END_NAMESPACE


#endif // QDIRECTFBCONVENIENCE_H
