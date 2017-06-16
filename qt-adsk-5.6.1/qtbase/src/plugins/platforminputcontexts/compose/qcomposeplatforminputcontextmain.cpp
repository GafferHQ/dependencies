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

#include <qpa/qplatforminputcontextplugin_p.h>

#include <QtCore/QStringList>

#include "qcomposeplatforminputcontext.h"

QT_BEGIN_NAMESPACE

class QComposePlatformInputContextPlugin : public QPlatformInputContextPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformInputContextFactoryInterface_iid FILE "compose.json")

public:
    QComposeInputContext *create(const QString &, const QStringList &) Q_DECL_OVERRIDE;
};

QComposeInputContext *QComposePlatformInputContextPlugin::create(const QString &system, const QStringList &paramList)
{
    Q_UNUSED(paramList);

    if (system.compare(system, QLatin1String("compose"), Qt::CaseInsensitive) == 0
            || system.compare(system, QLatin1String("xim"), Qt::CaseInsensitive) == 0)
        return new QComposeInputContext;
    return 0;
}

QT_END_NAMESPACE

#include "qcomposeplatforminputcontextmain.moc"
