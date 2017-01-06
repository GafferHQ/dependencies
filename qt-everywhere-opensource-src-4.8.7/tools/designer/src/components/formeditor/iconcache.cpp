/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "iconcache.h"
#include <QtGui/QPixmap>
#include <QtGui/QIcon>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

IconCache::IconCache(QObject *parent)
    : QDesignerIconCacheInterface(parent)
{
}

QIcon IconCache::nameToIcon(const QString &path, const QString &resourcePath)
{
    Q_UNUSED(path)
    Q_UNUSED(resourcePath)
    qWarning() << "IconCache::nameToIcon(): IconCache is obsoleted";
    return QIcon();
}

QString IconCache::iconToFilePath(const QIcon &pm) const
{
    Q_UNUSED(pm)
    qWarning() << "IconCache::iconToFilePath(): IconCache is obsoleted";
    return QString();
}

QString IconCache::iconToQrcPath(const QIcon &pm) const
{
    Q_UNUSED(pm)
    qWarning() << "IconCache::iconToQrcPath(): IconCache is obsoleted";
    return QString();
}

QPixmap IconCache::nameToPixmap(const QString &path, const QString &resourcePath)
{
    Q_UNUSED(path)
    Q_UNUSED(resourcePath)
    qWarning() << "IconCache::nameToPixmap(): IconCache is obsoleted";
    return QPixmap();
}

QString IconCache::pixmapToFilePath(const QPixmap &pm) const
{
    Q_UNUSED(pm)
    qWarning() << "IconCache::pixmapToFilePath(): IconCache is obsoleted";
    return QString();
}

QString IconCache::pixmapToQrcPath(const QPixmap &pm) const
{
    Q_UNUSED(pm)
    qWarning() << "IconCache::pixmapToQrcPath(): IconCache is obsoleted";
    return QString();
}

QList<QPixmap> IconCache::pixmapList() const
{
    qWarning() << "IconCache::pixmapList(): IconCache is obsoleted";
    return QList<QPixmap>();
}

QList<QIcon> IconCache::iconList() const
{
    qWarning() << "IconCache::iconList(): IconCache is obsoleted";
    return QList<QIcon>();
}

QString IconCache::resolveQrcPath(const QString &filePath, const QString &qrcPath, const QString &wd) const
{
    Q_UNUSED(filePath)
    Q_UNUSED(qrcPath)
    Q_UNUSED(wd)
    qWarning() << "IconCache::resolveQrcPath(): IconCache is obsoleted";
    return QString();
}

QT_END_NAMESPACE
