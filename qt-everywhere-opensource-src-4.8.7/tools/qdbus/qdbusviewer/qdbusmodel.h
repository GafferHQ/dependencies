/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef QDBUSMODEL_H
#define QDBUSMODEL_H

#include <QtCore/qabstractitemmodel.h>
#include <QtDBus/QtDBus>

struct QDBusItem;

QT_FORWARD_DECLARE_CLASS(QDomDocument);
QT_FORWARD_DECLARE_CLASS(QDomElement);


class QDBusModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Type { InterfaceItem, PathItem, MethodItem, SignalItem, PropertyItem };

    QDBusModel(const QString &service, const QDBusConnection &connection);
    ~QDBusModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    Type itemType(const QModelIndex &index) const;
    QString dBusPath(const QModelIndex &index) const;
    QString dBusInterface(const QModelIndex &index) const;
    QString dBusMethodName(const QModelIndex &index) const;
    QString dBusTypeSignature(const QModelIndex &index) const;

    void refresh(const QModelIndex &index = QModelIndex());

    QModelIndex findObject(const QDBusObjectPath &objectPath);

Q_SIGNALS:
    void busError(const QString &text);

private:
    QDomDocument introspect(const QString &path);
    void addMethods(QDBusItem *parent, const QDomElement &iface);
    void addPath(QDBusItem *parent);

    QString service;
    QDBusConnection c;
    QDBusItem *root;
};

#endif

