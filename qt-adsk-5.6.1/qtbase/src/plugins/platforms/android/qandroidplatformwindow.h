/****************************************************************************
**
** Copyright (C) 2014 BogDan Vatra <bogdan@kde.org>
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

#ifndef ANDROIDPLATFORMWINDOW_H
#define ANDROIDPLATFORMWINDOW_H
#include <qobject.h>
#include <qrect.h>
#include <qpa/qplatformwindow.h>

QT_BEGIN_NAMESPACE

class QAndroidPlatformScreen;
class QAndroidPlatformBackingStore;

class QAndroidPlatformWindow: public QPlatformWindow
{
public:
    explicit QAndroidPlatformWindow(QWindow *window);

    void lower();
    void raise();

    void setVisible(bool visible);

    void setWindowState(Qt::WindowState state);
    void setWindowFlags(Qt::WindowFlags flags);
    Qt::WindowFlags windowFlags() const;
    void setParent(const QPlatformWindow *window);
    WId winId() const { return m_windowId; }

    QAndroidPlatformScreen *platformScreen() const;

    void propagateSizeHints();
    void requestActivateWindow();
    void updateStatusBarVisibility();
    inline bool isRaster() const {
        if ((window()->flags() & Qt::ForeignWindow) == Qt::ForeignWindow)
            return false;

        return window()->surfaceType() == QSurface::RasterSurface
            || window()->surfaceType() == QSurface::RasterGLSurface;
    }
    bool isExposed() const;

    virtual void applicationStateChanged(Qt::ApplicationState);

    void setBackingStore(QAndroidPlatformBackingStore *store) { m_backingStore = store; }
    QAndroidPlatformBackingStore *backingStore() const { return m_backingStore; }

    virtual void repaint(const QRegion &) { }

protected:
    void setGeometry(const QRect &rect);

protected:
    Qt::WindowFlags m_windowFlags;
    Qt::WindowState m_windowState;

    WId m_windowId;

    QAndroidPlatformBackingStore *m_backingStore = nullptr;
};

QT_END_NAMESPACE
#endif // ANDROIDPLATFORMWINDOW_H
