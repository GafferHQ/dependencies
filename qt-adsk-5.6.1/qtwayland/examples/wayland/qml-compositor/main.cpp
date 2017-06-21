/****************************************************************************
**
** Copyright (C) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#include "qwaylandquickcompositor.h"
#include "qwaylandquicksurface.h"

#include <QtCompositor/qwaylandsurfaceitem.h>
#include <QtCompositor/qwaylandoutput.h>

#include <QGuiApplication>
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>

#include <QQmlContext>

#include <QQuickItem>
#include <QQuickView>

class QmlCompositor : public QQuickView, public QWaylandQuickCompositor
{
    Q_OBJECT
    Q_PROPERTY(QWaylandQuickSurface* fullscreenSurface READ fullscreenSurface WRITE setFullscreenSurface NOTIFY fullscreenSurfaceChanged)

public:
    QmlCompositor()
        : QWaylandQuickCompositor(0, DefaultExtensions | SubSurfaceExtension)
        , m_fullscreenSurface(0)
    {
        setSource(QUrl("main.qml"));
        setResizeMode(QQuickView::SizeRootObjectToView);
        setColor(Qt::black);
        winId();
        addDefaultShell();
        createOutput(this, "", "");

        connect(this, SIGNAL(afterRendering()), this, SLOT(sendCallbacks()));
    }

    QWaylandQuickSurface *fullscreenSurface() const
    {
        return m_fullscreenSurface;
    }

    Q_INVOKABLE QWaylandSurfaceItem *item(QWaylandSurface *surf)
    {
        return static_cast<QWaylandSurfaceItem *>(surf->views().first());
    }

signals:
    void windowAdded(QVariant window);
    void windowResized(QVariant window);
    void fullscreenSurfaceChanged();

public slots:
    void destroyWindow(QVariant window) {
        qvariant_cast<QObject *>(window)->deleteLater();
    }

    void setFullscreenSurface(QWaylandQuickSurface *surface) {
        if (surface == m_fullscreenSurface)
            return;
        m_fullscreenSurface = surface;
        emit fullscreenSurfaceChanged();
    }

private slots:
    void surfaceMapped() {
        QWaylandQuickSurface *surface = qobject_cast<QWaylandQuickSurface *>(sender());
        emit windowAdded(QVariant::fromValue(surface));
    }
    void surfaceUnmapped() {
        QWaylandQuickSurface *surface = qobject_cast<QWaylandQuickSurface *>(sender());
        if (surface == m_fullscreenSurface)
            m_fullscreenSurface = 0;
    }

    void surfaceDestroyed() {
        QWaylandQuickSurface *surface = static_cast<QWaylandQuickSurface *>(sender());
        if (surface == m_fullscreenSurface)
            m_fullscreenSurface = 0;
    }

    void sendCallbacks() {
        if (m_fullscreenSurface)
            sendFrameCallbacks(QList<QWaylandSurface *>() << m_fullscreenSurface);
        else
            sendFrameCallbacks(surfaces());
    }

protected:
    void resizeEvent(QResizeEvent *event)
    {
        QQuickView::resizeEvent(event);
        QWaylandCompositor::setOutputGeometry(QRect(0, 0, width(), height()));
    }

    void surfaceCreated(QWaylandSurface *surface) {
        connect(surface, SIGNAL(surfaceDestroyed()), this, SLOT(surfaceDestroyed()));
        connect(surface, SIGNAL(mapped()), this, SLOT(surfaceMapped()));
        connect(surface,SIGNAL(unmapped()), this,SLOT(surfaceUnmapped()));
    }

private:
    QWaylandQuickSurface *m_fullscreenSurface;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QmlCompositor compositor;
    compositor.setTitle(QLatin1String("QML Compositor"));
    compositor.setGeometry(0, 0, 1024, 768);
    compositor.show();

    compositor.rootContext()->setContextProperty("compositor", &compositor);

    QObject::connect(&compositor, SIGNAL(windowAdded(QVariant)), compositor.rootObject(), SLOT(windowAdded(QVariant)));
    QObject::connect(&compositor, SIGNAL(windowResized(QVariant)), compositor.rootObject(), SLOT(windowResized(QVariant)));

    return app.exec();
}

#include "main.moc"
