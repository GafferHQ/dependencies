/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "modules/webdatabase/SQLStatementBackend.h"

#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLStatement.h"
#include "modules/webdatabase/sqlite/SQLiteDatabase.h"
#include "modules/webdatabase/sqlite/SQLiteStatement.h"
#include "platform/Logging.h"
#include "wtf/text/CString.h"


// The Life-Cycle of a SQLStatement i.e. Who's keeping the SQLStatement alive?
// ==========================================================================
// The RefPtr chain goes something like this:
//
//     At birth (in SQLTransactionBackend::executeSQL()):
//     =================================================
//     SQLTransactionBackend           // HeapDeque<Member<SQLStatementBackend>> m_statementQueue points to ...
//     --> SQLStatementBackend         // Member<SQLStatement> m_frontend points to ...
//         --> SQLStatement
//
//     After grabbing the statement for execution (in SQLTransactionBackend::getNextStatement()):
//     =========================================================================================
//     SQLTransactionBackend           // Member<SQLStatementBackend> m_currentStatementBackend points to ...
//     --> SQLStatementBackend         // Member<SQLStatement> m_frontend points to ...
//         --> SQLStatement
//
//     Then we execute the statement in SQLTransactionBackend::runCurrentStatementAndGetNextState().
//     And we callback to the script in SQLTransaction::deliverStatementCallback() if
//     necessary.
//     - Inside SQLTransaction::deliverStatementCallback(), we operate on a raw SQLStatement*.
//       This pointer is valid because it is owned by SQLTransactionBackend's
//       SQLTransactionBackend::m_currentStatementBackend.
//
//     After we're done executing the statement (in SQLTransactionBackend::getNextStatement()):
//     =======================================================================================
//     When we're done executing, we'll grab the next statement. But before we
//     do that, getNextStatement() nullify SQLTransactionBackend::m_currentStatementBackend.
//     This will trigger the deletion of the SQLStatementBackend and SQLStatement.
//
//     Note: unlike with SQLTransaction, there is no JS representation of SQLStatement.
//     Hence, there is no GC dependency at play here.

namespace blink {

SQLStatementBackend* SQLStatementBackend::create(SQLStatement* frontend,
    const String& statement, const Vector<SQLValue>& arguments, int permissions)
{
    return new SQLStatementBackend(frontend, statement, arguments, permissions);
}

SQLStatementBackend::SQLStatementBackend(SQLStatement* frontend,
    const String& statement, const Vector<SQLValue>& arguments, int permissions)
    : m_frontend(frontend)
    , m_statement(statement.isolatedCopy())
    , m_arguments(arguments)
    , m_hasCallback(m_frontend->hasCallback())
    , m_hasErrorCallback(m_frontend->hasErrorCallback())
    , m_resultSet(SQLResultSet::create())
    , m_permissions(permissions)
{
    m_frontend->setBackend(this);
}

DEFINE_TRACE(SQLStatementBackend)
{
    visitor->trace(m_frontend);
    visitor->trace(m_resultSet);
}

SQLStatement* SQLStatementBackend::frontend()
{
    return m_frontend.get();
}

SQLErrorData* SQLStatementBackend::sqlError() const
{
    return m_error.get();
}

SQLResultSet* SQLStatementBackend::sqlResultSet() const
{
    return m_resultSet->isValid() ? m_resultSet.get() : 0;
}

bool SQLStatementBackend::execute(Database* db)
{
    ASSERT(!m_resultSet->isValid());

    // If we're re-running this statement after a quota violation, we need to clear that error now
    clearFailureDueToQuota();

    // This transaction might have been marked bad while it was being set up on the main thread,
    // so if there is still an error, return false.
    if (m_error)
        return false;

    db->setAuthorizerPermissions(m_permissions);

    SQLiteDatabase* database = &db->sqliteDatabase();

    SQLiteStatement statement(*database, m_statement);
    int result = statement.prepare();

    if (result != SQLResultOk) {
        WTF_LOG(StorageAPI, "Unable to verify correctness of statement %s - error %i (%s)", m_statement.ascii().data(), result, database->lastErrorMsg());
        if (result == SQLResultInterrupt)
            m_error = SQLErrorData::create(SQLError::DATABASE_ERR, "could not prepare statement", result, "interrupted");
        else
            m_error = SQLErrorData::create(SQLError::SYNTAX_ERR, "could not prepare statement", result, database->lastErrorMsg());
        db->reportExecuteStatementResult(1, m_error->code(), result);
        return false;
    }

    // FIXME: If the statement uses the ?### syntax supported by sqlite, the bind parameter count is very likely off from the number of question marks.
    // If this is the case, they might be trying to do something fishy or malicious
    if (statement.bindParameterCount() != m_arguments.size()) {
        WTF_LOG(StorageAPI, "Bind parameter count doesn't match number of question marks");
        m_error = SQLErrorData::create(SQLError::SYNTAX_ERR, "number of '?'s in statement string does not match argument count");
        db->reportExecuteStatementResult(2, m_error->code(), 0);
        return false;
    }

    for (unsigned i = 0; i < m_arguments.size(); ++i) {
        result = statement.bindValue(i + 1, m_arguments[i]);
        if (result == SQLResultFull) {
            setFailureDueToQuota(db);
            return false;
        }

        if (result != SQLResultOk) {
            WTF_LOG(StorageAPI, "Failed to bind value index %i to statement for query '%s'", i + 1, m_statement.ascii().data());
            db->reportExecuteStatementResult(3, SQLError::DATABASE_ERR, result);
            m_error = SQLErrorData::create(SQLError::DATABASE_ERR, "could not bind value", result, database->lastErrorMsg());
            return false;
        }
    }

    // Step so we can fetch the column names.
    result = statement.step();
    if (result == SQLResultRow) {
        int columnCount = statement.columnCount();
        SQLResultSetRowList* rows = m_resultSet->rows();

        for (int i = 0; i < columnCount; i++)
            rows->addColumn(statement.getColumnName(i));

        do {
            for (int i = 0; i < columnCount; i++)
                rows->addResult(statement.getColumnValue(i));

            result = statement.step();
        } while (result == SQLResultRow);

        if (result != SQLResultDone) {
            db->reportExecuteStatementResult(4, SQLError::DATABASE_ERR, result);
            m_error = SQLErrorData::create(SQLError::DATABASE_ERR, "could not iterate results", result, database->lastErrorMsg());
            return false;
        }
    } else if (result == SQLResultDone) {
        // Didn't find anything, or was an insert
        if (db->lastActionWasInsert())
            m_resultSet->setInsertId(database->lastInsertRowID());
    } else if (result == SQLResultFull) {
        // Return the Quota error - the delegate will be asked for more space and this statement might be re-run
        setFailureDueToQuota(db);
        return false;
    } else if (result == SQLResultConstraint) {
        db->reportExecuteStatementResult(6, SQLError::CONSTRAINT_ERR, result);
        m_error = SQLErrorData::create(SQLError::CONSTRAINT_ERR, "could not execute statement due to a constaint failure", result, database->lastErrorMsg());
        return false;
    } else {
        db->reportExecuteStatementResult(5, SQLError::DATABASE_ERR, result);
        m_error = SQLErrorData::create(SQLError::DATABASE_ERR, "could not execute statement", result, database->lastErrorMsg());
        return false;
    }

    // FIXME: If the spec allows triggers, and we want to be "accurate" in a different way, we'd use
    // sqlite3_total_changes() here instead of sqlite3_changed, because that includes rows modified from within a trigger
    // For now, this seems sufficient
    m_resultSet->setRowsAffected(database->lastChanges());

    db->reportExecuteStatementResult(0, -1, 0); // OK
    return true;
}

void SQLStatementBackend::setVersionMismatchedError(Database* database)
{
    ASSERT(!m_error && !m_resultSet->isValid());
    database->reportExecuteStatementResult(7, SQLError::VERSION_ERR, 0);
    m_error = SQLErrorData::create(SQLError::VERSION_ERR, "current version of the database and `oldVersion` argument do not match");
}

void SQLStatementBackend::setFailureDueToQuota(Database* database)
{
    ASSERT(!m_error && !m_resultSet->isValid());
    database->reportExecuteStatementResult(8, SQLError::QUOTA_ERR, 0);
    m_error = SQLErrorData::create(SQLError::QUOTA_ERR, "there was not enough remaining storage space, or the storage quota was reached and the user declined to allow more space");
}

void SQLStatementBackend::clearFailureDueToQuota()
{
    if (lastExecutionFailedDueToQuota())
        m_error = nullptr;
}

bool SQLStatementBackend::lastExecutionFailedDueToQuota() const
{
    return m_error && m_error->code() == SQLError::QUOTA_ERR;
}

} // namespace blink
