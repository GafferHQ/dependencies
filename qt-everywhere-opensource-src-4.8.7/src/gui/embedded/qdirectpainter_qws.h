/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QDIRECTPAINTER_QWS_H
#define QDIRECTPAINTER_QWS_H

#include <QtCore/qobject.h>
#include <QtGui/qregion.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

#ifndef QT_NO_DIRECTPAINTER
class QDirectPainterPrivate;
struct QWSEmbedEvent;

class Q_GUI_EXPORT QDirectPainter : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(QDirectPainter)
public:

    enum SurfaceFlag { NonReserved = 0,
                       Reserved = 1,
                       ReservedSynchronous = 3 };

    explicit QDirectPainter(QObject *parentObject = 0, SurfaceFlag flag = NonReserved);
    ~QDirectPainter();

    void setRegion(const QRegion&);
    QRegion requestedRegion() const;
    QRegion allocatedRegion() const;

    void setGeometry(const QRect&);
    QRect geometry() const;

    WId winId() const;
    virtual void regionChanged(const QRegion &exposedRegion);

    void startPainting(bool lockDisplay = true);
    void endPainting();
    void endPainting(const QRegion &region);
    void flush(const QRegion &region);

    void raise();
    void lower();


    static QRegion reserveRegion(const QRegion&);
    static QRegion reservedRegion();
    static QRegion region() { return reservedRegion(); }

    static uchar* frameBuffer();
    static int screenDepth();
    static int screenWidth();
    static int screenHeight();
    static int linestep();

    static void lock();
    static void unlock();
private:
    friend  void qt_directpainter_region(QDirectPainter *dp, const QRegion &alloc, int type);
    friend void qt_directpainter_embedevent(QDirectPainter*, const QWSEmbedEvent*);
};

#endif

QT_END_NAMESPACE

QT_END_HEADER

#endif // QDIRECTPAINTER_QWS_H
