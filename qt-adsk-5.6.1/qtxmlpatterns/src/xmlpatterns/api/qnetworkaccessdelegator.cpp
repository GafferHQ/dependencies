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

#include <QUrl>

#include <QNetworkAccessManager>

#include "qnetworkaccessdelegator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

NetworkAccessDelegator::NetworkAccessDelegator(QNetworkAccessManager *const genericManager,
                                               QNetworkAccessManager *const variableURIManager) : m_genericManager(genericManager)
                                                                                                , m_variableURIManager(variableURIManager)
{
}

QNetworkAccessManager *NetworkAccessDelegator::managerFor(const QUrl &uri)
{
    /* Unfortunately we have to do it this way, QUrl::isParentOf() doesn't
     * understand URI schemes like this one. */
    const QString requestedUrl(uri.toString());

    /* On the topic of timeouts:
     *
     * Currently the schemes QNetworkAccessManager handles should/will do
     * timeouts for 4.4, but we need to do timeouts for our own. */
    if(requestedUrl.startsWith(QLatin1String("tag:trolltech.com,2007:QtXmlPatterns:QIODeviceVariable:")))
        return m_variableURIManager;
    else
    {
        if(!m_genericManager)
            m_genericManager = new QNetworkAccessManager(this);

        return m_genericManager;
    }
}

QT_END_NAMESPACE

