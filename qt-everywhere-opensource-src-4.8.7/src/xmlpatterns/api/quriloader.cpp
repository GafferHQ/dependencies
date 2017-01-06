/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include "qiodevicedelegate_p.h"
#include "quriloader_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

URILoader::URILoader(QObject *const parent,
                     const NamePool::Ptr &np,
                     const VariableLoader::Ptr &l) : QNetworkAccessManager(parent)
                                                   , m_variableNS(QLatin1String("tag:trolltech.com,2007:QtXmlPatterns:QIODeviceVariable:"))
                                                   , m_namePool(np)
                                                   , m_variableLoader(l)
{
    Q_ASSERT(m_variableLoader);
}

QNetworkReply *URILoader::createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData)
{
    const QString requestedUrl(req.url().toString());

    /* We got a QIODevice variable. */
    const QString name(requestedUrl.right(requestedUrl.length() - m_variableNS.length()));

    const QVariant variant(m_variableLoader->valueFor(m_namePool->allocateQName(QString(), name, QString())));

    if(!variant.isNull() && variant.userType() == qMetaTypeId<QIODevice *>())
        return new QIODeviceDelegate(qvariant_cast<QIODevice *>(variant));
    else
    {
        /* If we're entering this code path, the variable URI identified a variable
         * which we don't have, which means we either have a bug, or the user had
         * crafted an invalid URI manually. */

        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }
}

QT_END_NAMESPACE

