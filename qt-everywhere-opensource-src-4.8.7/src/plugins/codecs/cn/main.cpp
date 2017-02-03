/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include <qtextcodecplugin.h>
#include <qtextcodec.h>
#include <qlist.h>

#include "qgb18030codec.h"

#ifndef QT_NO_TEXTCODECPLUGIN

QT_BEGIN_NAMESPACE

class CNTextCodecs : public QTextCodecPlugin
{
public:
    CNTextCodecs() {}

    QList<QByteArray> names() const;
    QList<QByteArray> aliases() const;
    QList<int> mibEnums() const;

    QTextCodec *createForMib(int);
    QTextCodec *createForName(const QByteArray &);
};

QList<QByteArray> CNTextCodecs::names() const
{
    QList<QByteArray> list;
    list += QGb18030Codec::_name();
    list += QGbkCodec::_name();
    list += QGb2312Codec::_name();
#ifdef Q_WS_X11
    list += QFontGb2312Codec::_name();
    list += QFontGbkCodec::_name();
#endif
    return list;
}

QList<QByteArray> CNTextCodecs::aliases() const
{
    QList<QByteArray> list;
    list += QGb18030Codec::_aliases();
    list += QGbkCodec::_aliases();
    list += QGb2312Codec::_aliases();
#ifdef Q_WS_X11
    list += QFontGb2312Codec::_aliases();
    list += QFontGbkCodec::_aliases();
#endif
    return list;
}

QList<int> CNTextCodecs::mibEnums() const
{
    QList<int> list;
    list += QGb18030Codec::_mibEnum();
    list += QGbkCodec::_mibEnum();
    list += QGb2312Codec::_mibEnum();
#ifdef Q_WS_X11
    list += QFontGb2312Codec::_mibEnum();
    list += QFontGbkCodec::_mibEnum();
#endif
    return list;
}

QTextCodec *CNTextCodecs::createForMib(int mib)
{
    if (mib == QGb18030Codec::_mibEnum())
        return new QGb18030Codec;
    if (mib == QGbkCodec::_mibEnum())
        return new QGbkCodec;
    if (mib == QGb2312Codec::_mibEnum())
        return new QGb2312Codec;
#ifdef Q_WS_X11
    if (mib == QFontGbkCodec::_mibEnum())
        return new QFontGbkCodec;
    if (mib == QFontGb2312Codec::_mibEnum())
        return new QFontGb2312Codec;
#endif
    return 0;
}


QTextCodec *CNTextCodecs::createForName(const QByteArray &name)
{
    if (name == QGb18030Codec::_name() || QGb18030Codec::_aliases().contains(name))
        return new QGb18030Codec;
    if (name == QGbkCodec::_name() || QGbkCodec::_aliases().contains(name))
        return new QGbkCodec;
    if (name == QGb2312Codec::_name() || QGb2312Codec::_aliases().contains(name))
        return new QGb2312Codec;
#ifdef Q_WS_X11
    if (name == QFontGbkCodec::_name() || QFontGbkCodec::_aliases().contains(name))
        return new QFontGbkCodec;
    if (name == QFontGb2312Codec::_name() || QFontGb2312Codec::_aliases().contains(name))
        return new QFontGb2312Codec;
#endif
    return 0;
}


Q_EXPORT_STATIC_PLUGIN(CNTextCodecs)
Q_EXPORT_PLUGIN2(qcncodecs, CNTextCodecs)

QT_END_NAMESPACE

#endif // QT_NO_TEXTCODECPLUGIN
