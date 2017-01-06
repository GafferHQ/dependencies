/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QDATAWIDGETMAPPER_H
#define QDATAWIDGETMAPPER_H

#include "QtCore/qobject.h"

#ifndef QT_NO_DATAWIDGETMAPPER

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class QAbstractItemDelegate;
class QAbstractItemModel;
class QModelIndex;
class QDataWidgetMapperPrivate;

class Q_GUI_EXPORT QDataWidgetMapper: public QObject
{
    Q_OBJECT

    Q_ENUMS(SubmitPolicy)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(SubmitPolicy submitPolicy READ submitPolicy WRITE setSubmitPolicy)

public:
    QDataWidgetMapper(QObject *parent = 0);
    ~QDataWidgetMapper();

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;

    void setItemDelegate(QAbstractItemDelegate *delegate);
    QAbstractItemDelegate *itemDelegate() const;

    void setRootIndex(const QModelIndex &index);
    QModelIndex rootIndex() const;

    void setOrientation(Qt::Orientation aOrientation);
    Qt::Orientation orientation() const;

    enum SubmitPolicy { AutoSubmit, ManualSubmit };
    void setSubmitPolicy(SubmitPolicy policy);
    SubmitPolicy submitPolicy() const;

    void addMapping(QWidget *widget, int section);
    void addMapping(QWidget *widget, int section, const QByteArray &propertyName);
    void removeMapping(QWidget *widget);
    int mappedSection(QWidget *widget) const;
    QByteArray mappedPropertyName(QWidget *widget) const;
    QWidget *mappedWidgetAt(int section) const;
    void clearMapping();

    int currentIndex() const;

public Q_SLOTS:
    void revert();
    bool submit();

    void toFirst();
    void toLast();
    void toNext();
    void toPrevious();
    virtual void setCurrentIndex(int index);
    void setCurrentModelIndex(const QModelIndex &index);

Q_SIGNALS:
    void currentIndexChanged(int index);

private:
    Q_DECLARE_PRIVATE(QDataWidgetMapper)
    Q_DISABLE_COPY(QDataWidgetMapper)
    Q_PRIVATE_SLOT(d_func(), void _q_dataChanged(const QModelIndex &, const QModelIndex &))
    Q_PRIVATE_SLOT(d_func(), void _q_commitData(QWidget *))
    Q_PRIVATE_SLOT(d_func(), void _q_closeEditor(QWidget *, QAbstractItemDelegate::EndEditHint))
    Q_PRIVATE_SLOT(d_func(), void _q_modelDestroyed())
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QT_NO_DATAWIDGETMAPPER
#endif

