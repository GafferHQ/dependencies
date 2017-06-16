/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#include "qgtkpainter_p.h"

#if !defined(QT_NO_STYLE_GTK)

#include <private/qhexstring_p.h>

QT_BEGIN_NAMESPACE

QGtkPainter::QGtkPainter()
{
    reset(0);
}

QGtkPainter::~QGtkPainter()
{
}

void QGtkPainter::reset(QPainter *painter)
{
    m_painter = painter;
    m_alpha = true;
    m_hflipped = false;
    m_vflipped = false;
    m_usePixmapCache = true;
    m_cliprect = QRect();
}

QString QGtkPainter::uniqueName(const QString &key, GtkStateType state, GtkShadowType shadow,
                                const QSize &size, GtkWidget *widget)
{
    // Note the widget arg should ideally use the widget path, though would compromise performance
    QString tmp = key
                  % HexString<uint>(state)
                  % HexString<uint>(shadow)
                  % HexString<uint>(size.width())
                  % HexString<uint>(size.height())
                  % HexString<quint64>(quint64(widget));
    return tmp;
}

QT_END_NAMESPACE

#endif //!defined(QT_NO_STYLE_GTK)
