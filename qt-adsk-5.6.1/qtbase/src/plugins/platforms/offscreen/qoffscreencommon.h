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

#ifndef QOFFSCREENCOMMON_H
#define QOFFSCREENCOMMON_H

#include <qpa/qplatformbackingstore.h>
#include <qpa/qplatformdrag.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformscreen.h>
#include <qpa/qplatformwindow.h>

#include <qscopedpointer.h>
#include <qimage.h>
#include <qhash.h>

QT_BEGIN_NAMESPACE

class QOffscreenScreen : public QPlatformScreen
{
public:
    QOffscreenScreen();

    QRect geometry() const Q_DECL_OVERRIDE { return m_geometry; }
    int depth() const Q_DECL_OVERRIDE { return 32; }
    QImage::Format format() const Q_DECL_OVERRIDE { return QImage::Format_RGB32; }
    QPlatformCursor *cursor() const Q_DECL_OVERRIDE { return m_cursor.data(); }

    QPixmap grabWindow(WId window, int x, int y, int width, int height) const Q_DECL_OVERRIDE;

    static QPlatformWindow *windowContainingCursor;

public:
    QRect m_geometry;
    QScopedPointer<QPlatformCursor> m_cursor;
};

#ifndef QT_NO_DRAGANDDROP
class QOffscreenDrag : public QPlatformDrag
{
public:
    QMimeData *platformDropData() Q_DECL_OVERRIDE { return 0; }
    Qt::DropAction drag(QDrag *) Q_DECL_OVERRIDE { return Qt::IgnoreAction; }
};
#endif

class QOffscreenBackingStore : public QPlatformBackingStore
{
public:
    QOffscreenBackingStore(QWindow *window);
    ~QOffscreenBackingStore();

    QPaintDevice *paintDevice() Q_DECL_OVERRIDE;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) Q_DECL_OVERRIDE;
    void resize(const QSize &size, const QRegion &staticContents) Q_DECL_OVERRIDE;
    bool scroll(const QRegion &area, int dx, int dy) Q_DECL_OVERRIDE;

    QPixmap grabWindow(WId window, const QRect &rect) const;

    static QOffscreenBackingStore *backingStoreForWinId(WId id);

private:
    void clearHash();

    QImage m_image;
    QHash<WId, QRect> m_windowAreaHash;

    static QHash<WId, QOffscreenBackingStore *> m_backingStoreForWinIdHash;
};

QT_END_NAMESPACE

#endif
