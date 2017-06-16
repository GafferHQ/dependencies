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

#ifndef QQUICKRANGEDDATE_H
#define QQUICKRANGEDDATE_H

#include <QDate>

#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQuickRangedDate : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged RESET resetDate)
    Q_PROPERTY(QDate minimumDate READ minimumDate WRITE setMinimumDate NOTIFY minimumDateChanged RESET resetMinimumDate)
    Q_PROPERTY(QDate maximumDate READ maximumDate WRITE setMaximumDate NOTIFY maximumDateChanged RESET resetMaximumDate)
public:
    QQuickRangedDate();
    ~QQuickRangedDate() {}

    QDate date() const { return mDate; }
    void setDate(const QDate &date);
    void resetDate() {}

    QDate minimumDate() const { return mMinimumDate; }
    void setMinimumDate(const QDate &minimumDate);
    void resetMinimumDate() {}

    QDate maximumDate() const { return mMaximumDate; }
    void setMaximumDate(const QDate &maximumDate);
    void resetMaximumDate() {}

Q_SIGNALS:
    void dateChanged();
    void minimumDateChanged();
    void maximumDateChanged();

private:
    QDate mDate;
    QDate mMinimumDate;
    QDate mMaximumDate;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickRangedDate)

#endif // QQUICKRANGEDDATE_H
