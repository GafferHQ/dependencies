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
#include <qstringlist.h>

#include "qeuckrcodec.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_TEXTCODECPLUGIN

class KRTextCodecs : public QTextCodecPlugin
{
public:
    KRTextCodecs() {}

    QList<QByteArray> names() const;
    QList<QByteArray> aliases() const;
    QList<int> mibEnums() const;

    QTextCodec *createForMib(int);
    QTextCodec *createForName(const QByteArray &);
};

QList<QByteArray> KRTextCodecs::names() const
{
    QList<QByteArray> list;
    list += QEucKrCodec::_name();
#ifdef Q_WS_X11
    list += QFontKsc5601Codec::_name();
#endif
    list += QCP949Codec::_name();
    return list;
}

QList<QByteArray> KRTextCodecs::aliases() const
{
    QList<QByteArray> list;
    list += QEucKrCodec::_aliases();
#ifdef Q_WS_X11
    list += QFontKsc5601Codec::_aliases();
#endif
    list += QCP949Codec::_aliases();
    return list;
}

QList<int> KRTextCodecs::mibEnums() const
{
    QList<int> list;
    list += QEucKrCodec::_mibEnum();
#ifdef Q_WS_X11
    list += QFontKsc5601Codec::_mibEnum();
#endif
    list += QCP949Codec::_mibEnum();
    return list;
}

QTextCodec *KRTextCodecs::createForMib(int mib)
{
    if (mib == QEucKrCodec::_mibEnum())
        return new QEucKrCodec;
#ifdef Q_WS_X11
    if (mib == QFontKsc5601Codec::_mibEnum())
        return new QFontKsc5601Codec;
#endif
    if (mib == QCP949Codec::_mibEnum())
        return new QCP949Codec;
    return 0;
}


QTextCodec *KRTextCodecs::createForName(const QByteArray &name)
{
    if (name == QEucKrCodec::_name() || QEucKrCodec::_aliases().contains(name))
        return new QEucKrCodec;
#ifdef Q_WS_X11
    if (name == QFontKsc5601Codec::_name() || QFontKsc5601Codec::_aliases().contains(name))
        return new QFontKsc5601Codec;
#endif
    if (name == QCP949Codec::_name() || QCP949Codec::_aliases().contains(name))
        return new QCP949Codec;
    return 0;
}


Q_EXPORT_STATIC_PLUGIN(KRTextCodecs);
Q_EXPORT_PLUGIN2(qkrcodecs, KRTextCodecs);

#endif // QT_NO_TEXTCODECPLUGIN

QT_END_NAMESPACE
