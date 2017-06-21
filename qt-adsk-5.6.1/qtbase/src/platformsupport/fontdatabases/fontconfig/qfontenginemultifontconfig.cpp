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

#include "qfontenginemultifontconfig_p.h"

#include <QtGui/private/qfontengine_ft_p.h>

QT_BEGIN_NAMESPACE

QFontEngineMultiFontConfig::QFontEngineMultiFontConfig(QFontEngine *fe, int script)
    : QFontEngineMulti(fe, script)
{
}

QFontEngineMultiFontConfig::~QFontEngineMultiFontConfig()
{
    Q_FOREACH (FcPattern *pattern, cachedMatchPatterns) {
        if (pattern)
            FcPatternDestroy(pattern);
    }
}

bool QFontEngineMultiFontConfig::shouldLoadFontEngineForCharacter(int at, uint ucs4) const
{
    bool charSetHasChar = true;
    FcPattern *matchPattern = getMatchPatternForFallback(at - 1);
    if (matchPattern != 0) {
        FcCharSet *charSet;
        FcPatternGetCharSet(matchPattern, FC_CHARSET, 0, &charSet);
        charSetHasChar = FcCharSetHasChar(charSet, ucs4);
    }

    return charSetHasChar;
}


FcPattern * QFontEngineMultiFontConfig::getMatchPatternForFallback(int fallBackIndex) const
{
    Q_ASSERT(fallBackIndex < fallbackFamilyCount());
    if (fallbackFamilyCount() > cachedMatchPatterns.size())
        cachedMatchPatterns.resize(fallbackFamilyCount());
    FcPattern *ret = cachedMatchPatterns.at(fallBackIndex);
    if (ret)
        return ret;
    FcPattern *requestPattern = FcPatternCreate();
    FcValue value;
    value.type = FcTypeString;
    QByteArray cs = fallbackFamilyAt(fallBackIndex).toUtf8();
    value.u.s = reinterpret_cast<const FcChar8 *>(cs.data());
    FcPatternAdd(requestPattern, FC_FAMILY, value, true);
    FcResult result;
    ret = FcFontMatch(0, requestPattern, &result);
    cachedMatchPatterns.insert(fallBackIndex, ret);
    FcPatternDestroy(requestPattern);
    return ret;
}

QT_END_NAMESPACE
