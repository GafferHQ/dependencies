/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qt3to4 porting application of the Qt Toolkit.
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

#include "logger.h"
#include <QFile>
#include <QDateTime>

QT_BEGIN_NAMESPACE

LogEntry::LogEntry(QString type, QString location)
:type(type), location(location)
{}

PlainLogEntry::PlainLogEntry(QString type, QString location, QString text)
:LogEntry(type, location), text(text)
{}

SourcePointLogEntry::SourcePointLogEntry(QString type, QString location, QString file, int line, int column, QString text)
:LogEntry(type, location), file(file), line(line), column(column), text(text)
{}

QString SourcePointLogEntry::description() const
{
    return QLatin1String("In file ")  + file +
        QLatin1String(" at line ") + QString::number(line + 1) + //line count is zero based, adjust here.
        QLatin1String(" column ")  + QString::number(column) +
           QLatin1String(": ") + text ;
}

void SourcePointLogEntry::updateLinePos(int threshold,  int delta)
{
    if (line >= threshold)
        line += delta;
}

/////////////////////////////////////////////////////


Logger::~Logger()
{
   qDeleteAll(logEntries);
}

Logger *Logger::theInstance  = 0;
Logger *Logger::instance()
{
    if(!theInstance)
        theInstance = new Logger();
        return theInstance;
}

void Logger::deleteInstance()
{
    if(theInstance)
        delete theInstance;
}

void Logger::addEntry(LogEntry *entry)
{
   Q_ASSERT(entry);
   pendingLogEntries.append(entry);
}

void Logger::beginSection()
{
    commitSection();
}

void Logger::commitSection()
{
    logEntries += pendingLogEntries;
    pendingLogEntries.clear();
}

void Logger::revertSection()
{
    qDeleteAll(pendingLogEntries);
    pendingLogEntries.clear();
}

int Logger::numEntries()
{
    commitSection();
    return logEntries.size();
}

QStringList Logger::fullReport()
{
    commitSection();
    QStringList report;
    report << QLatin1String("Log for qt3to4 on ") + QDateTime::currentDateTime().toString() +
        QLatin1String(". Number of log entries: ") + QString::number(logEntries.size());
    foreach(LogEntry *logEntry, logEntries) {
        report << logEntry->description();
    }
    return report;
}

/*
    Update the line for all SourcePointLogEntrys in the list of pending log
    entries located on or after insertLine.
*/
void Logger::updateLineNumbers(int insertLine, int numLines)
{
    foreach(LogEntry *logEntry, pendingLogEntries) {
        logEntry->updateLinePos(insertLine, numLines);
    }
}

QT_END_NAMESPACE
