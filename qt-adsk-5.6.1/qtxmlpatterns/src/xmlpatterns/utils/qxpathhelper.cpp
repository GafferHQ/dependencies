/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include <QStringList>

#include "private/qxmlutils_p.h"
#include "qbuiltintypes_p.h"
#include "qcommonvalues_p.h"
#include "qnamepool_p.h"

#include "qxpathhelper_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

bool XPathHelper::isReservedNamespace(const QXmlName::NamespaceCode ns)
{
    /* The order is because of that XFN and WXS are the most common. */
    return ns == StandardNamespaces::fn     ||
           ns == StandardNamespaces::xs     ||
           ns == StandardNamespaces::xml    ||
           ns == StandardNamespaces::xsi;
}

bool XPathHelper::isQName(const QString &qName)
{
    const QStringList result(qName.split(QLatin1Char(':')));
    const int c = result.count();

    if(c == 2)
    {
        return QXmlUtils::isNCName(result.first()) &&
               QXmlUtils::isNCName(result.last());
    }
    else if(c == 1)
        return QXmlUtils::isNCName(result.first());
    else
        return false;
}

void XPathHelper::splitQName(const QString &qName, QString &prefix, QString &ncName)
{
    Q_ASSERT_X(isQName(qName), Q_FUNC_INFO,
               "qName must be a valid QName.");

    const QStringList result(qName.split(QLatin1Char(':')));

    if(result.count() == 1)
    {
        Q_ASSERT(QXmlUtils::isNCName(result.first()));
        ncName = result.first();
    }
    else
    {
        Q_ASSERT(result.count() == 2);
        Q_ASSERT(QXmlUtils::isNCName(result.first()));
        Q_ASSERT(QXmlUtils::isNCName(result.last()));

        prefix = result.first();
        ncName = result.last();
    }
}

ItemType::Ptr XPathHelper::typeFromKind(const QXmlNodeModelIndex::NodeKind nodeKind)
{
    switch(nodeKind)
    {
        case QXmlNodeModelIndex::Element:
            return BuiltinTypes::element;
        case QXmlNodeModelIndex::Attribute:
            return BuiltinTypes::attribute;
        case QXmlNodeModelIndex::Text:
            return BuiltinTypes::text;
        case QXmlNodeModelIndex::ProcessingInstruction:
            return BuiltinTypes::pi;
        case QXmlNodeModelIndex::Comment:
            return BuiltinTypes::comment;
        case QXmlNodeModelIndex::Document:
            return BuiltinTypes::document;
        default:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO,
                       "A node type that doesn't exist in the XPath Data Model was encountered.");
            return ItemType::Ptr(); /* Dummy, silence compiler warning. */
        }
    }
}

QUrl XPathHelper::normalizeQueryURI(const QUrl &uri)
{
    Q_ASSERT_X(uri.isEmpty() || uri.isValid(), Q_FUNC_INFO,
               "The URI passed to QXmlQuery::setQuery() must be valid or empty.");
    if(uri.isEmpty())
        return QUrl::fromLocalFile(QCoreApplication::applicationFilePath());
    else if(uri.isRelative())
        return QUrl::fromLocalFile(QCoreApplication::applicationFilePath()).resolved(uri);
    else
        return uri;
}

QT_END_NAMESPACE
