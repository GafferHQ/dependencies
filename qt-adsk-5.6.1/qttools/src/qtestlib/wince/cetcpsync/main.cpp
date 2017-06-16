/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/
#include <iostream>
#include "qtcesterconnection.h"

using namespace std;

static void showUsage()
{
    cout    << "cetcpsync is meant to be used by cetest internally." << endl
            << "For usage instructions remoteconnection.h could be useful." << endl;
}

const int debugLevel = 0;
void debugOutput(const QString& text, int level)
{
    if (level <= debugLevel)
        cout << qPrintable(text) << endl;
}

class Exception
{
public:
    Exception(const QString& msg = QString())
        : m_message(msg)
    {}

    QString message() { return m_message; }

protected:
    QString m_message;
};

class TooFewParametersException : public Exception
{
public:
    TooFewParametersException(const QLatin1String& cmd, int expectedParameterCount)
    {
        m_message = QLatin1String("Command ") + cmd + QLatin1String(" needs at least ");
        m_message.append(QString::number(expectedParameterCount));
        m_message.append(QLatin1String(" parameters."));
    }
};

static void fileTimeFromString(FILETIME& ft, const QString& str)
{
    int idx = str.indexOf("*");
    if (idx <= 0)
        return;
    ft.dwLowDateTime = str.left(idx).toULong();
    ft.dwHighDateTime = str.mid(idx+1).toULong();
}

static QString fileTimeToString(FILETIME& ft)
{
    return QString::number(ft.dwLowDateTime) + "*" + QString::number(ft.dwHighDateTime);
}

static int execCommand(const QLatin1String& cmd, int argc, char* argv[])
{
    int retval = 0;
    bool success = true;
    QtCesterConnection connection;
    if (cmd == "copyFileToDevice") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.copyFileToDevice(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "copyDirectoryToDevice") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.copyDirectoryToDevice(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "copyFileFromDevice") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.copyFileFromDevice(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "copyDirectoryFromDevice") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.copyDirectoryFromDevice(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "timeStampForLocalFileTime") {
        if (argc < 1)
            throw TooFewParametersException(cmd, 1);
        FILETIME ft;
        fileTimeFromString(ft, argv[0]);
        success = connection.timeStampForLocalFileTime(&ft);
        if (success)
            cout << qPrintable(fileTimeToString(ft));
    } else if (cmd == "fileCreationTime") {
        if (argc < 1)
            throw TooFewParametersException(cmd, 1);
        FILETIME ft;
        success = connection.fileCreationTime(argv[0], &ft);
        if (success)
            cout << qPrintable(fileTimeToString(ft));
    } else if (cmd == "copyFile") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.copyFile(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "copyDirectory") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.copyDirectory(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "deleteFile") {
        if (argc < 1)
            throw TooFewParametersException(cmd, 1);
        success = connection.deleteFile(argv[0]);
    } else if (cmd == "deleteDirectory") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.deleteDirectory(argv[0], argv[1] == "true", argv[2] == "true");
    } else if (cmd == "moveFile") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.moveFile(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "moveDirectory") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        success = connection.moveDirectory(argv[0], argv[1], argv[2] == "true");
    } else if (cmd == "createDirectory") {
        if (argc < 2)
            throw TooFewParametersException(cmd, 2);
        success = connection.createDirectory(argv[0], argv[1] == "true");
    } else if (cmd == "execute") {
        if (argc < 3)
            throw TooFewParametersException(cmd, 3);
        int timeout = QString(argv[2]).toInt();
        success = connection.execute(argv[0], argv[1], timeout, &retval);
    } else if (cmd == "noop") {
        // do nothing :)
        success = true;
    } else {
        throw Exception("unknown command");
    }

    return success ? retval : 1;
}

int main(int argc, char *argv[])
{
    if (argc <= 1) {
        showUsage();
        return 0;
    }

    QLatin1String param(argv[1]);
    int result = 1;
    try {
        result = execCommand(param, argc - 2, argv + 2);
    } catch (Exception e) {
        cerr << "Error: " << qPrintable(e.message());
    }
    return result;
}
