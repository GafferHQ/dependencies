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

#ifndef QSQL_PSQL_H
#define QSQL_PSQL_H

#include <QtSql/qsqlresult.h>
#include <QtSql/qsqldriver.h>

#ifdef QT_PLUGIN
#define Q_EXPORT_SQLDRIVER_PSQL
#else
#define Q_EXPORT_SQLDRIVER_PSQL Q_SQL_EXPORT
#endif

QT_BEGIN_HEADER

typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;

QT_BEGIN_NAMESPACE

class QPSQLResultPrivate;
class QPSQLDriverPrivate;
class QPSQLDriver;
class QSqlRecordInfo;

class QPSQLResult : public QSqlResult
{
    friend class QPSQLResultPrivate;
public:
    QPSQLResult(const QPSQLDriver* db, const QPSQLDriverPrivate* p);
    ~QPSQLResult();

    QVariant handle() const;
    void virtual_hook(int id, void *data);

protected:
    void cleanup();
    bool fetch(int i);
    bool fetchFirst();
    bool fetchLast();
    QVariant data(int i);
    bool isNull(int field);
    bool reset (const QString& query);
    int size();
    int numRowsAffected();
    QSqlRecord record() const;
    QVariant lastInsertId() const;
    bool prepare(const QString& query);
    bool exec();

private:
    QPSQLResultPrivate *d;
};

class Q_EXPORT_SQLDRIVER_PSQL QPSQLDriver : public QSqlDriver
{
    Q_OBJECT
public:
    enum Protocol {
        VersionUnknown = -1,
        Version6 = 6,
        Version7 = 7,
        Version71 = 8,
        Version73 = 9,
        Version74 = 10,
        Version8 = 11,
        Version81 = 12,
        Version82 = 13,
        Version83 = 14,
        Version84 = 15,
        Version9 = 16,
    };

    explicit QPSQLDriver(QObject *parent=0);
    explicit QPSQLDriver(PGconn *conn, QObject *parent=0);
    ~QPSQLDriver();
    bool hasFeature(DriverFeature f) const;
    bool open(const QString & db,
              const QString & user,
              const QString & password,
              const QString & host,
              int port,
              const QString& connOpts);
    bool isOpen() const;
    void close();
    QSqlResult *createResult() const;
    QStringList tables(QSql::TableType) const;
    QSqlIndex primaryIndex(const QString& tablename) const;
    QSqlRecord record(const QString& tablename) const;

    Protocol protocol() const;
    QVariant handle() const;

    QString escapeIdentifier(const QString &identifier, IdentifierType type) const;
    QString formatValue(const QSqlField &field, bool trimStrings) const;

protected:
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

protected Q_SLOTS:
    bool subscribeToNotificationImplementation(const QString &name);
    bool unsubscribeFromNotificationImplementation(const QString &name);
    QStringList subscribedToNotificationsImplementation() const;

private Q_SLOTS:
    void _q_handleNotification(int);

private:
    void init();
    QPSQLDriverPrivate *d;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QSQL_PSQL_H
