/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#include "qplatformdefs.h"
#include "qurl.h"
#include "private/qurltlds_p.h"
#include "private/qtldurl_p.h"
#include "QtCore/qstring.h"
#include "QtCore/qvector.h"

QT_BEGIN_NAMESPACE

static bool containsTLDEntry(const QString &entry)
{
    int index = qt_hash(entry) % tldCount;

    // select the right chunk from the big table
    short chunk = 0;
    uint chunkIndex = tldIndices[index], offset = 0;
    while (chunk < tldChunkCount && tldIndices[index] >= tldChunks[chunk]) {
        chunkIndex -= tldChunks[chunk];
        offset += tldChunks[chunk];
        chunk++;
    }

    // check all the entries from the given index
    while (chunkIndex < tldIndices[index+1] - offset) {
        QString currentEntry = QString::fromUtf8(tldData[chunk] + chunkIndex);
        if (currentEntry == entry)
            return true;
        chunkIndex += qstrlen(tldData[chunk] + chunkIndex) + 1; // +1 for the ending \0
    }
    return false;
}

/*!
    \internal

    Return the top-level-domain per Qt's copy of the Mozilla public suffix list of
    \a domain.
*/

Q_CORE_EXPORT QString qTopLevelDomain(const QString &domain)
{
    const QString domainLower = domain.toLower();
    QVector<QStringRef> sections = domainLower.splitRef(QLatin1Char('.'), QString::SkipEmptyParts);
    if (sections.isEmpty())
        return QString();

    QString level, tld;
    for (int j = sections.count() - 1; j >= 0; --j) {
        level.prepend(QLatin1Char('.') + sections.at(j));
        if (qIsEffectiveTLD(level.right(level.size() - 1)))
            tld = level;
    }
    return tld;
}

/*!
    \internal

    Return true if \a domain is a top-level-domain per Qt's copy of the Mozilla public suffix list.
*/

Q_CORE_EXPORT bool qIsEffectiveTLD(const QString &domain)
{
    // for domain 'foo.bar.com':
    // 1. return if TLD table contains 'foo.bar.com'
    if (containsTLDEntry(domain))
        return true;

    const int dot = domain.indexOf(QLatin1Char('.'));
    if (dot >= 0) {
        int count = domain.size() - dot;
        QString wildCardDomain = QLatin1Char('*') + domain.rightRef(count);
        // 2. if table contains '*.bar.com',
        // test if table contains '!foo.bar.com'
        if (containsTLDEntry(wildCardDomain)) {
            QString exceptionDomain = QLatin1Char('!') + domain;
            return (! containsTLDEntry(exceptionDomain));
        }
    }
    return false;
}

QT_END_NAMESPACE
