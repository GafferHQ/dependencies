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

#ifndef QCURSOR_P_H
#define QCURSOR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "QtCore/qatomic.h"
#include "QtCore/qglobal.h"
#include "QtCore/qnamespace.h"
#include "QtGui/qpixmap.h"

# if defined (Q_WS_MAC)
#  include "private/qt_mac_p.h"
# elif defined(Q_WS_X11)
#  include "private/qt_x11_p.h"
# elif defined(Q_WS_WIN)
#  include "QtCore/qt_windows.h"
# elif defined(Q_OS_SYMBIAN)
#  include "private/qt_s60_p.h"
#endif

QT_BEGIN_NAMESPACE

#if defined (Q_WS_MAC)
void *qt_mac_nsCursorForQCursor(const QCursor &c);
class QMacAnimateCursor;
#endif

class QBitmap;
class QCursorData {
public:
    QCursorData(Qt::CursorShape s = Qt::ArrowCursor);
    ~QCursorData();

    static void initialize();
    static void cleanup();

    QAtomicInt ref;
    Qt::CursorShape cshape;
    QBitmap  *bm, *bmm;
    QPixmap pixmap;
    short     hx, hy;
#if defined (Q_WS_MAC)
    int mId;
#elif defined(Q_WS_QWS) || defined(Q_WS_QPA)
    int id;
#endif
#if defined (Q_WS_WIN)
    HCURSOR hcurs;
#elif defined (Q_WS_X11)
    XColor fg, bg;
    Cursor hcurs;
    Pixmap pm, pmm;
#elif defined (Q_WS_MAC)
    enum { TYPE_None, TYPE_ImageCursor, TYPE_ThemeCursor } type;
    union {
        struct {
            uint my_cursor:1;
            void *nscursor;
        } cp;
        struct {
            QMacAnimateCursor *anim;
            ThemeCursor curs;
        } tc;
    } curs;
    void initCursorFromBitmap();
    void initCursorFromPixmap();
#elif defined Q_OS_SYMBIAN
    void loadShapeFromResource(RWsSpriteBase& target, QString resource, int hx, int hy, int interval=0);
    void constructShapeSprite(RWsSpriteBase& target);
    void constructCursorSprite(RWsSpriteBase& target);
    RWsPointerCursor pcurs;
    RWsSprite scurs;
    RPointerArray<TSpriteMember> nativeSpriteMembers;
#endif
    static bool initialized;
    void update();
    static QCursorData *setBitmap(const QBitmap &bitmap, const QBitmap &mask, int hotX, int hotY);
};

extern QCursorData *qt_cursorTable[Qt::LastCursor + 1]; // qcursor.cpp

QT_END_NAMESPACE

#endif // QCURSOR_P_H
