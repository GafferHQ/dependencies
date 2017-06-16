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

#ifndef QWAYLANDWINDOWMANAGERINTEGRATION_H
#define QWAYLANDWINDOWMANAGERINTEGRATION_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <wayland-client.h>
#include <QtPlatformSupport/private/qgenericunixservices_p.h>

#include <QtWaylandClient/private/qwayland-windowmanager.h>
#include <QtWaylandClient/private/qwaylandclientexport_p.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandWindow;
class QWaylandDisplay;

class QWaylandWindowManagerIntegrationPrivate;

class Q_WAYLAND_CLIENT_EXPORT QWaylandWindowManagerIntegration : public QObject, public QGenericUnixServices, public QtWayland::qt_windowmanager
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandWindowManagerIntegration)
public:
    explicit QWaylandWindowManagerIntegration(QWaylandDisplay *waylandDisplay);
    virtual ~QWaylandWindowManagerIntegration();

    QByteArray desktopEnvironment() const;

    bool openUrl(const QUrl &url);
    bool openDocument(const QUrl &url);

    bool showIsFullScreen() const;

private:
    static void wlHandleListenerGlobal(void *data, wl_registry *registry, uint32_t id,
                                       const QString &interface, uint32_t version);

    QScopedPointer<QWaylandWindowManagerIntegrationPrivate> d_ptr;

    void windowmanager_hints(int32_t showIsFullScreen) Q_DECL_OVERRIDE;
    void windowmanager_quit() Q_DECL_OVERRIDE;

    void openUrl_helper(const QUrl &url);
};

QT_END_NAMESPACE

}

#endif // QWAYLANDWINDOWMANAGERINTEGRATION_H
