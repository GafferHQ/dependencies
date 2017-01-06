/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSql module of the Qt Toolkit.
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

#ifndef QSQL_OCI_H
#define QSQL_OCI_H

#include <QtSql/qsqlresult.h>
#include <QtSql/qsqldriver.h>
#include <QtSql/private/qsqlcachedresult_p.h>

#ifdef QT_PLUGIN
#define Q_EXPORT_SQLDRIVER_OCI
#else
#define Q_EXPORT_SQLDRIVER_OCI Q_SQL_EXPORT
#endif

QT_BEGIN_HEADER

typedef struct OCIEnv OCIEnv;
typedef struct OCISvcCtx OCISvcCtx;

QT_BEGIN_NAMESPACE

class QOCIDriver;
class QOCICols;
struct QOCIDriverPrivate;
struct QOCIResultPrivate;

class Q_EXPORT_SQLDRIVER_OCI QOCIResult : public QSqlCachedResult
{
    friend class QOCIDriver;
    friend struct QOCIResultPrivate;
    friend class QOCICols;
public:
    QOCIResult(const QOCIDriver * db, const QOCIDriverPrivate* p);
    ~QOCIResult();
    bool prepare(const QString& query);
    bool exec();
    QVariant handle() const;

protected:
    bool gotoNext(ValueCache &values, int index);
    bool reset (const QString& query);
    int size();
    int numRowsAffected();
    QSqlRecord record() const;
    QVariant lastInsertId() const;
    void virtual_hook(int id, void *data);

private:
    QOCIResultPrivate *d;
};

class Q_EXPORT_SQLDRIVER_OCI QOCIDriver : public QSqlDriver
{
    Q_OBJECT
    friend struct QOCIResultPrivate;
    friend class QOCIPrivate;
public:
    explicit QOCIDriver(QObject* parent = 0);
    QOCIDriver(OCIEnv* env, OCISvcCtx* ctx, QObject* parent = 0);
    ~QOCIDriver();
    bool hasFeature(DriverFeature f) const;
    bool open(const QString & db,
              const QString & user,
              const QString & password,
              const QString & host,
              int port,
              const QString& connOpts);
    void close();
    QSqlResult *createResult() const;
    QStringList tables(QSql::TableType) const;
    QSqlRecord record(const QString& tablename) const;
    QSqlIndex primaryIndex(const QString& tablename) const;
    QString formatValue(const QSqlField &field,
                        bool trimStrings) const;
    QVariant handle() const;
    QString escapeIdentifier(const QString &identifier, IdentifierType) const;

protected:
    bool                beginTransaction();
    bool                commitTransaction();
    bool                rollbackTransaction();
private:
    QOCIDriverPrivate *d;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QSQL_OCI_H
