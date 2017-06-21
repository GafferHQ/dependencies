/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qsceneparserfactory_p.h"
#include "qsceneparserplugin.h"
#include "qabstractsceneparser.h"

#include <QtCore/private/qfactoryloader_p.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader, (QSceneParserFactoryInterface_iid, QLatin1String("/sceneparsers"), Qt::CaseInsensitive))
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, directLoader, (QSceneParserFactoryInterface_iid, QLatin1String(""), Qt::CaseInsensitive))
#endif

QStringList QSceneParserFactory::keys(const QString &pluginPath)
{
#ifndef QT_NO_LIBRARY
    QStringList list;
    if (!pluginPath.isEmpty()) {
        QCoreApplication::addLibraryPath(pluginPath);
        list = directLoader()->keyMap().values();
        if (!list.isEmpty()) {
            const QString postFix = QStringLiteral(" (from ")
                    + QDir::toNativeSeparators(pluginPath)
                    + QLatin1Char(')');
            const QStringList::iterator end = list.end();
            for (QStringList::iterator it = list.begin(); it != end; ++it)
                (*it).append(postFix);
        }
    }
    list.append(loader()->keyMap().values());
    return list;
#else
    return QStringList();
#endif
}

QAbstractSceneParser *QSceneParserFactory::create(const QString &name, const QStringList &args, const QString &pluginPath)
{
#ifndef QT_NO_LIBRARY
    if (!pluginPath.isEmpty()) {
        QCoreApplication::addLibraryPath(pluginPath);
        if (QAbstractSceneParser *ret = qLoadPlugin1<QAbstractSceneParser, QSceneParserPlugin>(directLoader(), name, args))
            return ret;
    }
    if (QAbstractSceneParser *ret = qLoadPlugin1<QAbstractSceneParser, QSceneParserPlugin>(loader(), name, args))
        return ret;
#endif
    return Q_NULLPTR;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
