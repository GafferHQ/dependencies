/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKCALENDARMODEL_H
#define QQUICKCALENDARMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QLocale>
#include <QVariant>
#include <QDate>

QT_BEGIN_NAMESPACE

class QQuickCalendarModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QDate visibleDate READ visibleDate WRITE setVisibleDate NOTIFY visibleDateChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit QQuickCalendarModel(QObject *parent = 0);

    enum {
        // If this class is made public, this will have to be changed.
        DateRole = Qt::UserRole + 1
    };

    QDate visibleDate() const;
    void setVisibleDate(const QDate &visibleDate);

    QLocale locale() const;
    void setLocale(const QLocale &locale);

    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;

    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;

    QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

    Q_INVOKABLE QDate dateAt(int index) const;
    Q_INVOKABLE int indexAt(const QDate &visibleDate);
    Q_INVOKABLE int weekNumberAt(int row) const;

Q_SIGNALS:
    void visibleDateChanged(const QDate &visibleDate);
    void localeChanged(const QLocale &locale);
    void countChanged(int count);

protected:
    void populateFromVisibleDate(const QDate &previousDate, bool force = false);

    QDate mVisibleDate;
    QDate mFirstVisibleDate;
    QDate mLastVisibleDate;
    QVector<QDate> mVisibleDates;
    QLocale mLocale;
};

QT_END_NAMESPACE

#endif // QQUICKCALENDARMODEL_H
