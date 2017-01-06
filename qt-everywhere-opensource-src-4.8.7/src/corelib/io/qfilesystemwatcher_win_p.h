/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QFILESYSTEMWATCHER_WIN_P_H
#define QFILESYSTEMWATCHER_WIN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QLibrary class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "qfilesystemwatcher_p.h"

#ifndef QT_NO_FILESYSTEMWATCHER

#include <qt_windows.h>

#include <QtCore/qdatetime.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qhash.h>
#include <QtCore/qmutex.h>
#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

class QWindowsFileSystemWatcherEngineThread;

// Even though QWindowsFileSystemWatcherEngine is derived of QThread
// via QFileSystemWatcher, it does not start a thread.
// Instead QWindowsFileSystemWatcher creates QWindowsFileSystemWatcherEngineThreads
// to do the actually watching.
class QWindowsFileSystemWatcherEngine : public QFileSystemWatcherEngine
{
    Q_OBJECT
public:
    QWindowsFileSystemWatcherEngine();
    ~QWindowsFileSystemWatcherEngine();

    QStringList addPaths(const QStringList &paths, QStringList *files, QStringList *directories);
    QStringList removePaths(const QStringList &paths, QStringList *files, QStringList *directories);

    void stop();


    class Handle
    {
    public:
        HANDLE handle;
        uint flags;

        Handle()
                : handle(INVALID_HANDLE_VALUE), flags(0u)
        { }
        Handle(const Handle &other)
                : handle(other.handle), flags(other.flags)
        { }
    };

    class PathInfo {
    public:
        QString absolutePath;
        QString path;
        bool isDir;

        // fileinfo bits
        uint ownerId;
        uint groupId;
        QFile::Permissions permissions;
        QDateTime lastModified;

        PathInfo &operator=(const QFileInfo &fileInfo)
                           {
            ownerId = fileInfo.ownerId();
            groupId = fileInfo.groupId();
            permissions = fileInfo.permissions();
            lastModified = fileInfo.lastModified();
            return *this;
        }

        bool operator!=(const QFileInfo &fileInfo) const
        {
            return (ownerId != fileInfo.ownerId()
                    || groupId != fileInfo.groupId()
                    || permissions != fileInfo.permissions()
                    || lastModified != fileInfo.lastModified());
        }
    };
private:
    QList<QWindowsFileSystemWatcherEngineThread *> threads;

};

class QWindowsFileSystemWatcherEngineThread : public QThread
{
    Q_OBJECT

public:
    QWindowsFileSystemWatcherEngineThread();
    ~QWindowsFileSystemWatcherEngineThread();
    void run();
    void stop();
    void wakeup();

    QMutex mutex;
    QVector<HANDLE> handles;
    int msg;

    QHash<QString, QWindowsFileSystemWatcherEngine::Handle> handleForDir;

    QHash<HANDLE, QHash<QString, QWindowsFileSystemWatcherEngine::PathInfo> > pathInfoForHandle;

Q_SIGNALS:
    void fileChanged(const QString &path, bool removed);
    void directoryChanged(const QString &path, bool removed);
};

#endif // QT_NO_FILESYSTEMWATCHER

QT_END_NAMESPACE

#endif // QFILESYSTEMWATCHER_WIN_P_H
