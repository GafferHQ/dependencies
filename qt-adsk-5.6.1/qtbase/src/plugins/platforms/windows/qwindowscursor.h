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

#ifndef QWINDOWSCURSOR_H
#define QWINDOWSCURSOR_H

#include "qtwindows_additional.h"

#include <qpa/qplatformcursor.h>
#include <QtCore/QSharedPointer>
#include <QtCore/QHash>

QT_BEGIN_NAMESPACE

struct QWindowsPixmapCursorCacheKey
{
    explicit QWindowsPixmapCursorCacheKey(const QCursor &c);

    qint64 bitmapCacheKey;
    qint64 maskCacheKey;
};

inline bool operator==(const QWindowsPixmapCursorCacheKey &k1, const QWindowsPixmapCursorCacheKey &k2)
{
    return k1.bitmapCacheKey == k2.bitmapCacheKey && k1.maskCacheKey == k2.maskCacheKey;
}

inline uint qHash(const QWindowsPixmapCursorCacheKey &k, uint seed) Q_DECL_NOTHROW
{
    return (uint(k.bitmapCacheKey) + uint(k.maskCacheKey)) ^ seed;
}

class CursorHandle
{
    Q_DISABLE_COPY(CursorHandle)
public:
    explicit CursorHandle(HCURSOR hcursor = Q_NULLPTR) : m_hcursor(hcursor) {}
    ~CursorHandle()
    {
        if (m_hcursor)
            DestroyCursor(m_hcursor);
    }

    bool isNull() const { return !m_hcursor; }
    HCURSOR handle() const { return m_hcursor; }

private:
    const HCURSOR m_hcursor;
};

typedef QSharedPointer<CursorHandle> CursorHandlePtr;

class QWindowsCursor : public QPlatformCursor
{
public:
    enum CursorState {
        CursorShowing,
        CursorHidden,
        CursorSuppressed // Cursor suppressed by touch interaction (Windows 8).
    };

    struct PixmapCursor {
        explicit PixmapCursor(const QPixmap &pix = QPixmap(), const QPoint &h = QPoint()) : pixmap(pix), hotSpot(h) {}

        QPixmap pixmap;
        QPoint hotSpot;
    };

    explicit QWindowsCursor(const QPlatformScreen *screen);

    void changeCursor(QCursor * widgetCursor, QWindow * widget) Q_DECL_OVERRIDE;
    QPoint pos() const Q_DECL_OVERRIDE;
    void setPos(const QPoint &pos) Q_DECL_OVERRIDE;

    static HCURSOR createPixmapCursor(QPixmap pixmap, const QPoint &hotSpot, qreal scaleFactor = 1);
    static HCURSOR createPixmapCursor(const PixmapCursor &pc, qreal scaleFactor = 1) { return createPixmapCursor(pc.pixmap, pc.hotSpot, scaleFactor); }
    static PixmapCursor customCursor(Qt::CursorShape cursorShape, const QPlatformScreen *screen = Q_NULLPTR);

    static HCURSOR createCursorFromShape(Qt::CursorShape cursorShape, const QPlatformScreen *screen = Q_NULLPTR);
    static QPoint mousePosition();
    static CursorState cursorState();

    CursorHandlePtr standardWindowCursor(Qt::CursorShape s = Qt::ArrowCursor);
    CursorHandlePtr pixmapWindowCursor(const QCursor &c);

    QPixmap dragDefaultCursor(Qt::DropAction action) const;

private:
    typedef QHash<Qt::CursorShape, CursorHandlePtr> StandardCursorCache;
    typedef QHash<QWindowsPixmapCursorCacheKey, CursorHandlePtr> PixmapCursorCache;

    const QPlatformScreen *const m_screen;
    StandardCursorCache m_standardCursorCache;
    PixmapCursorCache m_pixmapCursorCache;

    mutable QPixmap m_copyDragCursor;
    mutable QPixmap m_moveDragCursor;
    mutable QPixmap m_linkDragCursor;
    mutable QPixmap m_ignoreDragCursor;
};

QT_END_NAMESPACE

#endif // QWINDOWSCURSOR_H
