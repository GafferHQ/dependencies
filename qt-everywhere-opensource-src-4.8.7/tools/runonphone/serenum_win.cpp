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

#include "serenum.h"
#include <QByteArray>
#include <QString>
#include <QDebug>
#include <windows.h>
#include <windef.h>
#include <setupapi.h>
#include <devguid.h>
#include <winreg.h>
#include <shlwapi.h>

//{4d36e978-e325-11ce-bfc1-08002be10318}
//DEFINE_GUID(GUID_DEVCLASS_PORTS, 0x4D36E978, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 );

QList<SerialPortId> enumerateSerialPorts(int)
{
    DWORD index=0;
    SP_DEVINFO_DATA info;
    GUID guid = GUID_DEVCLASS_PORTS;
    HDEVINFO infoset = SetupDiGetClassDevs(&guid, 0, 0, DIGCF_PRESENT);
    QString valueName(16384, 0);
    QList<SerialPortId> list;

    for (index=0;;index++) {
        ZeroMemory(&info, sizeof(SP_DEVINFO_DATA));
        info.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiEnumDeviceInfo(infoset, index, &info))
            break;
        QString friendlyName;
        QString portName;
        DWORD size=0;
        SetupDiGetDeviceRegistryProperty(infoset, &info, SPDRP_FRIENDLYNAME, 0, 0, 0, &size);
        QByteArray ba(size, 0);
        if(SetupDiGetDeviceRegistryProperty(infoset, &info, SPDRP_FRIENDLYNAME, 0, (BYTE*)(ba.data()), size, 0)) {
            friendlyName = QString((const QChar*)(ba.constData()), ba.size() / 2 - 1);
        }
        HKEY key = SetupDiOpenDevRegKey(infoset, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if(key != INVALID_HANDLE_VALUE) {
            //RegGetValue not supported on XP, SHRegGetValue not supported by mingw, so use the old method of enumerating all the values
            for (DWORD dwi=0;;dwi++) {
                DWORD vsize = valueName.size();
                if (ERROR_SUCCESS == RegEnumValue(key, dwi, (WCHAR*)(valueName.data()), &vsize, 0, 0, 0, &size)) {
                    if (valueName.startsWith("PortName")) {
                       QByteArray ba(size, 0);
                       vsize = valueName.size();
                       if(ERROR_SUCCESS == RegEnumValue(key, dwi, (WCHAR*)(valueName.data()), &vsize, 0, 0, (BYTE*)(ba.data()), &size)) {
                           portName = QString((const QChar*)(ba.constData()), ba.size() / 2 - 1);
                       }
                   }
                } else {
                    break;
                }
            }
            RegCloseKey(key);
        }
        SerialPortId id;
        id.portName = portName;
        id.friendlyName = friendlyName;
        list.append(id);
    }
    SetupDiDestroyDeviceInfoList(infoset);
    return list;
}

