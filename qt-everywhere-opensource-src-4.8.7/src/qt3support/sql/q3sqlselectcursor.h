/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#ifndef Q3SQLSELECTCURSOR_H
#define Q3SQLSELECTCURSOR_H

#include <Qt3Support/q3sqlcursor.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3Support)

#ifndef QT_NO_SQL

class Q3SqlSelectCursorPrivate;

class Q_COMPAT_EXPORT Q3SqlSelectCursor : public Q3SqlCursor
{
public:
    Q3SqlSelectCursor(const QString& query = QString(), QSqlDatabase db = QSqlDatabase());
    Q3SqlSelectCursor(const Q3SqlSelectCursor& other);
    ~Q3SqlSelectCursor();
    bool exec(const QString& query);
    bool select() { return Q3SqlCursor::select(); }

protected:
    QSqlIndex primaryIndex(bool = true) const { return QSqlIndex(); }
    QSqlIndex index(const QStringList&) const { return QSqlIndex(); }
    QSqlIndex index(const QString&) const { return QSqlIndex(); }
    QSqlIndex index(const char*) const { return QSqlIndex(); }
    void setPrimaryIndex(const QSqlIndex&) {}
    void append(const Q3SqlFieldInfo&) {}
    void insert(int, const Q3SqlFieldInfo&) {}
    void remove(int) {}
    void clear() {}
    void setGenerated(const QString&, bool) {}
    void setGenerated(int, bool) {}
    QSqlRecord*        editBuffer(bool = false) { return 0; }
    QSqlRecord*        primeInsert() { return 0; }
    QSqlRecord*        primeUpdate() { return 0; }
    QSqlRecord*        primeDelete() { return 0; }
    int        insert(bool = true) { return 0; }
    int        update(bool = true) { return 0; }
    int        del(bool = true) { return 0; }
    void setMode(int) {}

    void setSort(const QSqlIndex&) {}
    QSqlIndex sort() const { return QSqlIndex(); }
    void setFilter(const QString&) {}
    QString filter() const { return QString(); }
    void setName(const QString&, bool = true) {}
    QString name() const { return QString(); }
    QString toString(const QString& = QString(), const QString& = QLatin1String(",")) const { return QString(); }
    bool select(const QString &, const QSqlIndex& = QSqlIndex());

private:
    void populateCursor();

    Q3SqlSelectCursorPrivate * d;

protected:
#if !defined(Q_NO_USING_KEYWORD)
    using Q3SqlCursor::update;
#else
    virtual int update(const QString & filter, bool invalidate = true) { return Q3SqlCursor::update(filter, invalidate); }
#endif
};

#endif // QT_NO_SQL

QT_END_NAMESPACE

QT_END_HEADER

#endif // Q3SQLSELECTCURSOR_H
