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

#ifndef QSQL_ODBC_H
#define QSQL_ODBC_H

#include <QtSql/qsqldriver.h>
#include <QtSql/qsqlresult.h>

#if defined (Q_OS_WIN32)
#include <QtCore/qt_windows.h>
#endif

#ifdef QT_PLUGIN
#define Q_EXPORT_SQLDRIVER_ODBC
#else
#define Q_EXPORT_SQLDRIVER_ODBC Q_SQL_EXPORT
#endif

#ifdef Q_OS_UNIX
#define HAVE_LONG_LONG 1 // force UnixODBC NOT to fall back to a struct for BIGINTs
#endif

#if defined(Q_CC_BOR)
// workaround for Borland to make sure that SQLBIGINT is defined
#  define _MSC_VER 900
#endif
#include <sql.h>
#if defined(Q_CC_BOR)
#  undef _MSC_VER
#endif

#include <sqlext.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

class QODBCPrivate;
class QODBCDriverPrivate;
class QODBCDriver;
class QSqlRecordInfo;

class QODBCResult : public QSqlResult
{
public:
    QODBCResult(const QODBCDriver * db, QODBCDriverPrivate* p);
    virtual ~QODBCResult();

    bool prepare(const QString& query);
    bool exec();

    QVariant handle() const;
    virtual void setForwardOnly(bool forward);

protected:
    bool fetchNext();
    bool fetchFirst();
    bool fetchLast();
    bool fetchPrevious();
    bool fetch(int i);
    bool reset (const QString& query);
    QVariant data(int field);
    bool isNull(int field);
    int size();
    int numRowsAffected();
    QSqlRecord record() const;
    void virtual_hook(int id, void *data);
    bool nextResult();

private:
    QODBCPrivate *d;
};

class Q_EXPORT_SQLDRIVER_ODBC QODBCDriver : public QSqlDriver
{
    Q_OBJECT
public:
    explicit QODBCDriver(QObject *parent=0);
    QODBCDriver(SQLHANDLE env, SQLHANDLE con, QObject * parent=0);
    virtual ~QODBCDriver();
    bool hasFeature(DriverFeature f) const;
    void close();
    QSqlResult *createResult() const;
    QStringList tables(QSql::TableType) const;
    QSqlRecord record(const QString& tablename) const;
    QSqlIndex primaryIndex(const QString& tablename) const;
    QVariant handle() const;
    QString formatValue(const QSqlField &field,
                        bool trimStrings) const;
    bool open(const QString& db,
              const QString& user,
              const QString& password,
              const QString& host,
              int port,
              const QString& connOpts);

    QString escapeIdentifier(const QString &identifier, IdentifierType type) const;

protected Q_SLOTS:
    bool isIdentifierEscapedImplementation(const QString &identifier, IdentifierType type) const;

protected:
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

private:
    void init();
    bool endTrans();
    void cleanup();
    QODBCDriverPrivate* d;
    friend class QODBCPrivate;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QSQL_ODBC_H
