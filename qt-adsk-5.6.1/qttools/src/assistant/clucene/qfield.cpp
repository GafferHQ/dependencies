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

#include "qfield_p.h"
#include "qreader_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/document/Field.h>

QT_BEGIN_NAMESPACE

QCLuceneFieldPrivate::QCLuceneFieldPrivate()
    : QSharedData()
{
    field = 0;
    deleteCLuceneField = true;
}

QCLuceneFieldPrivate::QCLuceneFieldPrivate(const QCLuceneFieldPrivate &other)
    : QSharedData()
{
    field = _CL_POINTER(other.field);
    deleteCLuceneField = other.deleteCLuceneField;
}

QCLuceneFieldPrivate::~QCLuceneFieldPrivate()
{
    if (deleteCLuceneField)
        _CLDECDELETE(field);
}


QCLuceneField::QCLuceneField()
    : d(new QCLuceneFieldPrivate())
    , reader(0)
{
    // nothing todo
}

QCLuceneField::QCLuceneField(const QString &name, const QString &value, int configs)
    : d(new QCLuceneFieldPrivate())
    , reader(0)
{
    TCHAR* fieldName = QStringToTChar(name);
    TCHAR* fieldValue = QStringToTChar(value);

    d->field = new lucene::document::Field(fieldName, fieldValue, configs);

    delete [] fieldName;
    delete [] fieldValue;
}

QCLuceneField::QCLuceneField(const QString &name, QCLuceneReader *reader,
                             int configs)
    : d(new QCLuceneFieldPrivate())
    , reader(reader)
{
    TCHAR* fieldName = QStringToTChar(name);

    reader->d->deleteCLuceneReader = false; // clucene takes ownership
    d->field = new lucene::document::Field(fieldName, reader->d->reader, configs);

    delete [] fieldName;
}

QCLuceneField::~QCLuceneField()
{
    delete reader;
}

QString QCLuceneField::name() const
{
    return TCharToQString(d->field->name());
}

QString QCLuceneField::stringValue() const
{
    return TCharToQString((const TCHAR*)d->field->stringValue());
}

QCLuceneReader* QCLuceneField::readerValue() const
{
    return reader;
}

bool QCLuceneField::isStored() const
{
    return d->field->isStored();
}

bool QCLuceneField::isIndexed() const
{
    return d->field->isIndexed();
}

bool QCLuceneField::isTokenized() const
{
    return d->field->isTokenized();
}

bool QCLuceneField::isCompressed() const
{
    return d->field->isCompressed();
}

void QCLuceneField::setConfig(int termVector)
{
    d->field->setConfig(termVector);
}

bool QCLuceneField::isTermVectorStored() const
{
    return d->field->isTermVectorStored();
}

bool QCLuceneField::isStoreOffsetWithTermVector() const
{
    return d->field->isStoreOffsetWithTermVector();
}

bool QCLuceneField::isStorePositionWithTermVector() const
{
    return d->field->isStorePositionWithTermVector();
}

qreal QCLuceneField::getBoost() const
{
    return qreal(d->field->getBoost());
}

void QCLuceneField::setBoost(qreal value)
{
    d->field->setBoost(qreal(value));
}

bool QCLuceneField::isBinary() const
{
    return d->field->isBinary();
}

bool QCLuceneField::getOmitNorms() const
{
    return d->field->getOmitNorms();
}

void QCLuceneField::setOmitNorms(bool omitNorms)
{
    d->field->setOmitNorms(omitNorms);
}

QString QCLuceneField::toString() const
{
    return TCharToQString(d->field->toString());
}

QT_END_NAMESPACE
