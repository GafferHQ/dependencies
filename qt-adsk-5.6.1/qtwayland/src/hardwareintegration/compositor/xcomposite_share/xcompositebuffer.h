/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef XCOMPOSITEBUFFER_H
#define XCOMPOSITEBUFFER_H

#include <private/qwlcompositor_p.h>
#include <qwayland-server-wayland.h>

#include <QtCore/QSize>

#include <QtCore/QTextStream>
#include <QtCore/QDataStream>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>

#include <X11/X.h>

QT_BEGIN_NAMESPACE

class XCompositeBuffer : public QtWaylandServer::wl_buffer
{
public:
    XCompositeBuffer(Window window, const QSize &size,
                     struct ::wl_client *client, uint32_t id);

    Window window();

    bool isYInverted() const { return mInvertedY; }
    void setInvertedY(bool inverted) { mInvertedY = inverted; }

    QSize size() const { return mSize; }

    static XCompositeBuffer *fromResource(struct ::wl_resource *resource) { return static_cast<XCompositeBuffer*>(Resource::fromResource(resource)->buffer_object); }

protected:
    void buffer_destroy_resource(Resource *) Q_DECL_OVERRIDE;
    void buffer_destroy(Resource *) Q_DECL_OVERRIDE;

private:
    Window mWindow;
    bool mInvertedY;
    QSize mSize;
};

QT_END_NAMESPACE

#endif // XCOMPOSITORBUFFER_H
