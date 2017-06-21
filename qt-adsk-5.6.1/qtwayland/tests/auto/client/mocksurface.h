/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qglobal.h>

#include "qwayland-server-wayland.h"

#include "mockcompositor.h"

namespace Impl {

class Surface : public QtWaylandServer::wl_surface
{
public:
    Surface(wl_client *client, uint32_t id, int v, Compositor *compositor);
    ~Surface();

    Compositor *compositor() const { return m_compositor; }
    static Surface *fromResource(struct ::wl_resource *resource);
    void map();
    bool isMapped() const;

    QSharedPointer<MockSurface> mockSurface() const { return m_mockSurface; }

protected:

    void surface_destroy_resource(Resource *resource) Q_DECL_OVERRIDE;

    void surface_destroy(Resource *resource) Q_DECL_OVERRIDE;
    void surface_attach(Resource *resource,
                        struct wl_resource *buffer, int x, int y) Q_DECL_OVERRIDE;
    void surface_damage(Resource *resource,
                        int32_t x, int32_t y, int32_t width, int32_t height) Q_DECL_OVERRIDE;
    void surface_frame(Resource *resource,
                       uint32_t callback) Q_DECL_OVERRIDE;
    void surface_commit(Resource *resource) Q_DECL_OVERRIDE;
private:
    wl_resource *m_buffer;

    Compositor *m_compositor;
    QSharedPointer<MockSurface> m_mockSurface;
    QList<wl_resource *> m_frameCallbackList;
    bool m_mapped;
};

}
