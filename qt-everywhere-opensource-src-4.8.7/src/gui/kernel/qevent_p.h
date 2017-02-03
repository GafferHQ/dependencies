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

#ifndef QEVENT_P_H
#define QEVENT_P_H

#include <QtCore/qglobal.h>
#include <QtCore/qurl.h>
#include <QtGui/qevent.h>

#ifdef Q_OS_SYMBIAN
#include <f32file.h>
#endif

QT_BEGIN_NAMESPACE

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

// ### Qt 5: remove
class QKeyEventEx : public QKeyEvent
{
public:
    QKeyEventEx(Type type, int key, Qt::KeyboardModifiers modifiers,
                const QString &text, bool autorep, ushort count,
                quint32 nativeScanCode, quint32 nativeVirtualKey, quint32 nativeModifiers);
    QKeyEventEx(const QKeyEventEx &other);

    ~QKeyEventEx();

protected:
    quint32 nScanCode;
    quint32 nVirtualKey;
    quint32 nModifiers;
    friend class QKeyEvent;
};

// ### Qt 5: remove
class QMouseEventEx : public QMouseEvent
{
public:
    QMouseEventEx(Type type, const QPointF &pos, const QPoint &globalPos,
                  Qt::MouseButton button, Qt::MouseButtons buttons,
                  Qt::KeyboardModifiers modifiers);
    ~QMouseEventEx();

protected:
    QPointF posF;
    friend class QMouseEvent;
};

class QTouchEventTouchPointPrivate
{
public:
    inline QTouchEventTouchPointPrivate(int id)
        : ref(1),
          id(id),
          state(Qt::TouchPointReleased),
          pressure(qreal(-1.))
    { }

    inline QTouchEventTouchPointPrivate *detach()
    {
        QTouchEventTouchPointPrivate *d = new QTouchEventTouchPointPrivate(*this);
        d->ref = 1;
        if (!this->ref.deref())
            delete this;
        return d;
    }

    QAtomicInt ref;
    int id;
    Qt::TouchPointStates state;
    QRectF rect, sceneRect, screenRect;
    QPointF normalizedPos,
            startPos, startScenePos, startScreenPos, startNormalizedPos,
            lastPos, lastScenePos, lastScreenPos, lastNormalizedPos;
    qreal pressure;
};

#ifndef QT_NO_GESTURES
class QNativeGestureEvent : public QEvent
{
public:
    enum Type {
        None,
        GestureBegin,
        GestureEnd,
        Pan,
        Zoom,
        Rotate,
        Swipe
    };

    QNativeGestureEvent()
        : QEvent(QEvent::NativeGesture), gestureType(None), percentage(0)
#ifdef Q_WS_WIN
        , sequenceId(0), argument(0)
#endif
    {
    }

    Type gestureType;
    float percentage;
    QPoint position;
    float angle;
#ifdef Q_WS_WIN
    ulong sequenceId;
    quint64 argument;
#endif
};

class QGestureEventPrivate
{
public:
    inline QGestureEventPrivate(const QList<QGesture *> &list)
        : gestures(list), widget(0)
    {
    }

    QList<QGesture *> gestures;
    QWidget *widget;
    QMap<Qt::GestureType, bool> accepted;
    QMap<Qt::GestureType, QWidget *> targetWidgets;
};
#endif // QT_NO_GESTURES

class QFileOpenEventPrivate
{
public:
    inline QFileOpenEventPrivate(const QUrl &url)
        : url(url)
    {
    }
    ~QFileOpenEventPrivate();

    QUrl url;
#ifdef Q_OS_SYMBIAN
    RFile file;
#endif
};

QT_END_NAMESPACE

#endif // QEVENT_P_H
