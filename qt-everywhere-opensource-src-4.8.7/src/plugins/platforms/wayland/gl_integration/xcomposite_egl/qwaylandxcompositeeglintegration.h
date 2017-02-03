/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWAYLANDXCOMPOSITEEGLINTEGRATION_H
#define QWAYLANDXCOMPOSITEEGLINTEGRATION_H

#include "gl_integration/qwaylandglintegration.h"
#include "wayland-client.h"

#include <QtCore/QTextStream>
#include <QtCore/QDataStream>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>
#include <QtGui/QWidget>

#include <QWaitCondition>

#include <X11/Xlib.h>
#include <EGL/egl.h>

struct wl_xcomposite;

class QWaylandXCompositeEGLIntegration : public QWaylandGLIntegration
{
public:
    QWaylandXCompositeEGLIntegration(QWaylandDisplay * waylandDispaly);
    ~QWaylandXCompositeEGLIntegration();

    void initialize();

    QWaylandWindow *createEglWindow(QWidget *widget);

    QWaylandDisplay *waylandDisplay() const;
    struct wl_xcomposite *waylandXComposite() const;

    Display *xDisplay() const;
    EGLDisplay eglDisplay() const;
    int screen() const;
    Window rootWindow() const;

private:
    QWaylandDisplay *mWaylandDisplay;
    struct wl_xcomposite *mWaylandComposite;

    Display *mDisplay;
    EGLDisplay mEglDisplay;
    int mScreen;
    Window mRootWindow;

    static void wlDisplayHandleGlobal(struct wl_display *display, uint32_t id,
                             const char *interface, uint32_t version, void *data);

    static const struct wl_xcomposite_listener xcomposite_listener;
    static void rootInformation(void *data,
                 struct wl_xcomposite *xcomposite,
                 const char *display_name,
                 uint32_t root_window);
};

#endif // QWAYLANDXCOMPOSITEEGLINTEGRATION_H
