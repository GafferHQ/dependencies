/****************************************************************************
**
** Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team.
** All rights reserved.
**
** Portion Copyright (C) 2015 The Qt Company Ltd.
** All rights reserved.
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
****************************************************************************/

#include "qhits_p.h"
#include "qsearchable_p.h"

#include <CLucene.h>
#include <CLucene/search/SearchHeader.h>

QT_BEGIN_NAMESPACE

QCLuceneHitsPrivate::QCLuceneHitsPrivate()
    : QSharedData()
{
    hits = 0;
    deleteCLuceneHits = true;
}

QCLuceneHitsPrivate::QCLuceneHitsPrivate(const QCLuceneHitsPrivate &other)
    : QSharedData()
{
    hits = _CL_POINTER(other.hits);
    deleteCLuceneHits = other.deleteCLuceneHits;
}

QCLuceneHitsPrivate::~QCLuceneHitsPrivate()
{
    if (deleteCLuceneHits)
        _CLDECDELETE(hits);
}


QCLuceneHits::QCLuceneHits(const QCLuceneSearcher &searcher,
                           const QCLuceneQuery &query, const QCLuceneFilter &filter)
    : d(new QCLuceneHitsPrivate())
{
    d->hits = new lucene::search::Hits(searcher.d->searchable, query.d->query,
        filter.d->filter);
}

QCLuceneHits::QCLuceneHits(const QCLuceneSearcher &searcher, const QCLuceneQuery &query,
                         const QCLuceneFilter &filter, const QCLuceneSort &sort)
    : d(new QCLuceneHitsPrivate())
{
    d->hits = new lucene::search::Hits(searcher.d->searchable, query.d->query,
        filter.d->filter, sort.d->sort);
}

QCLuceneHits::~QCLuceneHits()
{
    // nothing todo
}

QCLuceneDocument QCLuceneHits::document(const qint32 index)
{
    // TODO: check this
    QCLuceneDocument document;
    document.d->deleteCLuceneDocument = false;
    lucene::document::Document &doc = d->hits->doc(int32_t(index));
    document.d->document = &doc;

    return document;
}

qint32 QCLuceneHits::length() const
{
    return qint32(d->hits->length());
}

qint32 QCLuceneHits::id(const qint32 index)
{
    return qint32(d->hits->id(int32_t(index)));
}

qreal QCLuceneHits::score(const qint32 index)
{
    return qreal(d->hits->score(int32_t(index)));
}

QT_END_NAMESPACE
