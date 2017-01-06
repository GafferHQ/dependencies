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

#include "qtbrushmanager.h"
#include <QtGui/QPixmap>
#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

class QtBrushManagerPrivate
{
    QtBrushManager *q_ptr;
    Q_DECLARE_PUBLIC(QtBrushManager)
public:
    QMap<QString, QBrush> theBrushMap;
    QString theCurrentBrush;
};

QtBrushManager::QtBrushManager(QObject *parent)
    : QDesignerBrushManagerInterface(parent), d_ptr(new QtBrushManagerPrivate)
{
    d_ptr->q_ptr = this;
}

QtBrushManager::~QtBrushManager()
{
}

QBrush QtBrushManager::brush(const QString &name) const
{
    if (d_ptr->theBrushMap.contains(name))
        return d_ptr->theBrushMap[name];
    return QBrush();
}

QMap<QString, QBrush> QtBrushManager::brushes() const
{
    return d_ptr->theBrushMap;
}

QString QtBrushManager::currentBrush() const
{
    return d_ptr->theCurrentBrush;
}

QString QtBrushManager::addBrush(const QString &name, const QBrush &brush)
{
    if (name.isNull())
        return QString();

    QString newName = name;
    QString nameBase = newName;
    int i = 0;
    while (d_ptr->theBrushMap.contains(newName)) {
        newName = nameBase + QString::number(++i);
    }
    d_ptr->theBrushMap[newName] = brush;
    emit brushAdded(newName, brush);

    return newName;
}

void QtBrushManager::removeBrush(const QString &name)
{
    if (!d_ptr->theBrushMap.contains(name))
        return;
    if (currentBrush() == name)
        setCurrentBrush(QString());
    emit brushRemoved(name);
    d_ptr->theBrushMap.remove(name);
}

void QtBrushManager::setCurrentBrush(const QString &name)
{
    QBrush newBrush;
    if (!name.isNull()) {
        if (d_ptr->theBrushMap.contains(name))
            newBrush = d_ptr->theBrushMap[name];
        else
            return;
    }
    d_ptr->theCurrentBrush = name;
    emit currentBrushChanged(name, newBrush);
}

QPixmap QtBrushManager::brushPixmap(const QBrush &brush) const
{
    int w = 64;
    int h = 64;

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(0, 0, w, h), brush);
    return QPixmap::fromImage(img);
}

}  // namespace qdesigner_internal

QT_END_NAMESPACE
