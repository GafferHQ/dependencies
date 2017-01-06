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
#ifndef REMOTECONNECTION_H
#define REMOTECONNECTION_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <windows.h>
class AbstractRemoteConnection
{
public:
    AbstractRemoteConnection();
    virtual ~AbstractRemoteConnection();

    virtual bool connect(QVariantList&) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // These functions are designed for transfer between desktop and device
    // Caution: deviceDest path has to be device specific (eg. no drive letters for CE)
    virtual bool copyFileToDevice(const QString &localSource, const QString &deviceDest, bool failIfExists = false) = 0;
    virtual bool copyDirectoryToDevice(const QString &localSource, const QString &deviceDest, bool recursive = true) = 0;
    virtual bool copyFileFromDevice(const QString &deviceSource, const QString &localDest, bool failIfExists = false) = 0;
    virtual bool copyDirectoryFromDevice(const QString &deviceSource, const QString &localDest, bool recursive = true) = 0;

    // For "intelligent deployment" we need to investigate on filetimes on the device
    virtual bool timeStampForLocalFileTime(FILETIME*) const = 0;
    virtual bool fileCreationTime(const QString &fileName, FILETIME*) const = 0;
    
    // These functions only work on files existing on the device
    virtual bool copyFile(const QString&, const QString&, bool failIfExists = false) = 0;
    virtual bool copyDirectory(const QString&, const QString&, bool recursive = true) = 0;
    virtual bool deleteFile(const QString&) = 0;
    virtual bool deleteDirectory(const QString&, bool recursive = true, bool failIfContentExists = false) = 0;
    bool moveFile(const QString&, const QString&, bool FailIfExists = false);
    bool moveDirectory(const QString&, const QString&, bool recursive = true);

    virtual bool createDirectory(const QString&, bool deleteBefore=false) = 0;

    virtual bool execute(QString program, QString arguments = QString(), int timeout = -1, int *returnValue = NULL) = 0;
};

#endif
