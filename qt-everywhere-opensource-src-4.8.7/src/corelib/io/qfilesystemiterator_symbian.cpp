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

#include "qfilesystemiterator_p.h"
#include "qfilesystemengine_p.h"
#include <QtCore/private/qcore_symbian_p.h>

QT_BEGIN_NAMESPACE

QFileSystemIterator::QFileSystemIterator(const QFileSystemEntry &path, QDir::Filters filters,
        const QStringList &nameFilters, QDirIterator::IteratorFlags iteratorFlags)
        : lastError(KErrNone), entryIndex(-1)
{
    RFs& fs = qt_s60GetRFs();

    nativePath = path.nativeFilePath();
    if (!nativePath.endsWith(QLatin1Char('\\')))
        nativePath.append(QLatin1Char('\\'));

    QString absPath = QFileSystemEngine::absoluteName(path).nativeFilePath();

    if (!absPath.endsWith(QLatin1Char('\\')))
        absPath.append(QLatin1Char('\\'));

    int pathLen = absPath.length();
    if (pathLen > KMaxFileName) {
        lastError = KErrBadName;
        return;
    }

    //set up server side filtering to reduce IPCs
    //RDir won't accept all valid name filters e.g. "*. bar"
    if (nameFilters.count() == 1 && !(filters & QDir::AllDirs) && iteratorFlags
        == QDirIterator::NoIteratorFlags && pathLen + nameFilters[0].length()
        <= KMaxFileName) {
        //server side supports one mask - skip this for recursive mode or if only files should be filtered
        absPath.append(nameFilters[0]);
    }

    TUint symbianMask = 0;
    if ((filters & QDir::Dirs) || (filters & QDir::AllDirs) || (iteratorFlags
        & QDirIterator::Subdirectories))
        symbianMask |= KEntryAttDir; //include directories
    if (filters & QDir::Hidden)
        symbianMask |= KEntryAttHidden;
    if (filters & QDir::System)
        symbianMask |= KEntryAttSystem;
    //Do not use KEntryAttMatchExclusive to optimise to return only
    //directories for QDir::Dirs. There may be a file which is actually
    //a "mount point" for a file engine and needs to be returned so it
    //can be overriden to be a directory, see QTBUG-23688
    if (symbianMask == 0
        && ((filters & QDir::PermissionMask) == QDir::Writable)) {
            symbianMask = KEntryAttMatchExclude | KEntryAttReadOnly;
    }

    lastError = dirHandle.Open(fs, qt_QString2TPtrC(absPath), symbianMask);
}

QFileSystemIterator::~QFileSystemIterator()
{
    dirHandle.Close();
}

bool QFileSystemIterator::advance(QFileSystemEntry &fileEntry, QFileSystemMetaData &metaData)
{
    //1st time, lastError is result of dirHandle.Open(), entries.Count() is 0 and entryIndex is -1 so initial read is triggered
    //subsequent times, read is triggered each time we reach the end of the entry list
    //final time, lastError is KErrEof so we don't need to read anymore.
    ++entryIndex;
    if (lastError == KErrNone && entryIndex >= entries.Count()) {
        lastError = dirHandle.Read(entries);
        entryIndex = 0;
    }

    //each call to advance() gets the next entry from the entry list.
    //from the final (or only) read call, KErrEof is returned together with a full buffer so we still need to go through the list
    if ((lastError == KErrNone || lastError == KErrEof) && entryIndex < entries.Count()) {
        Q_ASSERT(entryIndex >= 0);
        const TEntry &entry(entries[entryIndex]);
        fileEntry = QFileSystemEntry(nativePath + qt_TDesC2QString(entry.iName), QFileSystemEntry::FromNativePath());
        metaData.fillFromTEntry(entry);
        return true;
    }

    //TODO: error reporting, to allow user to distinguish empty directory from error condition.

    return false;
}

QT_END_NAMESPACE
