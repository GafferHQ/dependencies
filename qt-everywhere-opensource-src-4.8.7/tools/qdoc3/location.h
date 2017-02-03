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

/*
  location.h
*/

#ifndef LOCATION_H
#define LOCATION_H

#include <qstack.h>

#include "tr.h"

#define QDOC_QML

QT_BEGIN_NAMESPACE

class Config;
class QRegExp;

class Location
{
 public:
    Location();
    Location(const QString& filePath);
    Location(const Location& other);
    ~Location() { delete stk; }

    Location& operator=(const Location& other);

    void start();
    void advance(QChar ch);
    void advanceLines(int n) { stkTop->lineNo += n; stkTop->columnNo = 1; }

    void push(const QString& filePath);
    void pop();
    void setEtc(bool etc) { etcetera = etc; }
    void setLineNo(int no) { stkTop->lineNo = no; }
    void setColumnNo(int no) { stkTop->columnNo = no; }

    bool isEmpty() const { return stkDepth == 0; }
    int depth() const { return stkDepth; }
    const QString& filePath() const { return stkTop->filePath; }
    QString fileName() const;
    int lineNo() const { return stkTop->lineNo; }
    int columnNo() const { return stkTop->columnNo; }
    bool etc() const { return etcetera; }
    void warning(const QString& message, 
                 const QString& details = QString()) const;
    void error(const QString& message, 
               const QString& details = QString()) const;
    void fatal(const QString& message, 
               const QString& details = QString()) const;

    QT_STATIC_CONST Location null;

    static void initialize(const Config& config);
    static void terminate();
    static void information(const QString& message);
    static void internalError(const QString& hint);

 private:
    enum MessageType { Warning, Error };

    struct StackEntry
    {
	QString filePath;
	int lineNo;
	int columnNo;
    };

    void emitMessage(MessageType type, 
                     const QString& message,
                     const QString& details) const;
    QString toString() const;
    QString top() const;

 private:
    StackEntry stkBottom;
    QStack<StackEntry> *stk;
    StackEntry *stkTop;
    int stkDepth;
    bool etcetera;

    static int tabSize;
    static QString programName;
    static QRegExp *spuriousRegExp;
};

QT_END_NAMESPACE

#endif
