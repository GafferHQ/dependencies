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

#ifndef QXCBCURSOR_H
#define QXCBCURSOR_H

#include <qpa/qplatformcursor.h>
#include "qxcbscreen.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_CURSOR

struct QXcbCursorCacheKey
{
    explicit QXcbCursorCacheKey(const QCursor &c);
    explicit QXcbCursorCacheKey(Qt::CursorShape s) : shape(s), bitmapCacheKey(0), maskCacheKey(0) {}
    QXcbCursorCacheKey() : shape(Qt::CustomCursor), bitmapCacheKey(0), maskCacheKey(0) {}

    Qt::CursorShape shape;
    qint64 bitmapCacheKey;
    qint64 maskCacheKey;
};

inline bool operator==(const QXcbCursorCacheKey &k1, const QXcbCursorCacheKey &k2)
{
    return k1.shape == k2.shape && k1.bitmapCacheKey == k2.bitmapCacheKey && k1.maskCacheKey == k2.maskCacheKey;
}

inline uint qHash(const QXcbCursorCacheKey &k, uint seed) Q_DECL_NOTHROW
{
    return (uint(k.shape) + uint(k.bitmapCacheKey) + uint(k.maskCacheKey)) ^ seed;
}

#endif // !QT_NO_CURSOR

class QXcbCursor : public QXcbObject, public QPlatformCursor
{
public:
    QXcbCursor(QXcbConnection *conn, QXcbScreen *screen);
    ~QXcbCursor();
#ifndef QT_NO_CURSOR
    void changeCursor(QCursor *cursor, QWindow *widget) Q_DECL_OVERRIDE;
#endif
    QPoint pos() const Q_DECL_OVERRIDE;
    void setPos(const QPoint &pos) Q_DECL_OVERRIDE;

    static void queryPointer(QXcbConnection *c, QXcbVirtualDesktop **virtualDesktop, QPoint *pos, int *keybMask = 0);

private:
#ifndef QT_NO_CURSOR
    typedef QHash<QXcbCursorCacheKey, xcb_cursor_t> CursorHash;

    xcb_cursor_t createFontCursor(int cshape);
    xcb_cursor_t createBitmapCursor(QCursor *cursor);
    xcb_cursor_t createNonStandardCursor(int cshape);
#endif

    QXcbScreen *m_screen;
#ifndef QT_NO_CURSOR
    CursorHash m_cursorHash;
#endif
#if defined(XCB_USE_XLIB) && !defined(QT_NO_LIBRARY)
    static void cursorThemePropertyChanged(QXcbVirtualDesktop *screen,
                                           const QByteArray &name,
                                           const QVariant &property,
                                           void *handle);
#endif
    bool m_gtkCursorThemeInitialized;
};

QT_END_NAMESPACE

#endif
