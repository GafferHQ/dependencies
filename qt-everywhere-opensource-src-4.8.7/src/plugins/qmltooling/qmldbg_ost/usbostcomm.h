/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#ifndef USBHOSTCOMM_H
#define USBHOSTCOMM_H

// Based on the official usbostrouter header, modified to remove dependancy on
// the client DLL

#include <e32base.h>

typedef int TOstProtIds;

class RUsbOstComm : public RSessionBase
{
public:
    RUsbOstComm();
    TInt Connect();
    TInt Disconnect();
    TInt Open();
    TInt Close();
    TInt RegisterProtocolID(TOstProtIds aId, TBool aNeedHeader);
    void ReadMessage(TRequestStatus& aStatus, TDes8& aDes);
    TInt ReadCancel();
    void WriteMessage(TRequestStatus& aStatus, const TDesC8& aDes, TBool aHasHeader=EFalse);
    TVersion Version() const;

private:
    enum TUsbOstCmdCode
    {
        EUsbOstCmdCodeFirst,
        EUsbOstCmdConnect,
        EUsbOstCmdDisconnect,
        EUsbOstCmdCodeGetAcmConfig,
        EUsbOstCmdCodeSetAcmConfig,
        EUsbOstCmdCodeOpen,
        EUsbOstCmdCodeClose,
        EUsbOstCmdCodeRegisterId,
        EUsbOstCmdCodeRegisterIds,
        EUsbOstCmdCodeUnRegisterId,
        EUsbOstCmdCodeUnRegisterIds,
        EUsbOstCmdCodeReadMsg,
        EUsbOstCmdCodeReadCancel,
        EUsbOstCmdCodeWriteMsg,
        EUsbOstCmdCodeWriteCancel,
        EUsbOstCmdCodeLast
    };
};

RUsbOstComm::RUsbOstComm()
{
}

TInt RUsbOstComm::Connect()
{
    _LIT(KUsbOstServerName, "!UsbOstRouter");
    _LIT(KUsbOstServerImageName, "usbostrouter");
    const TUid KUsbOstServerUid = { 0x200170BE };
    TInt startupAttempts = 2;
    for(;;) {
        TInt ret = CreateSession(KUsbOstServerName, TVersion(1,0,0));
        if (ret != KErrNotFound && ret != KErrServerTerminated) {
            return ret;
        }

        if (startupAttempts-- == 0) {
            return ret;
        }

        RProcess server;
        ret = server.Create(KUsbOstServerImageName, KNullDesC, KUsbOstServerUid);
        if (ret != KErrNone)
            return ret;

        TRequestStatus serverDiedRequestStatus;
        server.Rendezvous(serverDiedRequestStatus);

        if (serverDiedRequestStatus != KRequestPending) {
            // Abort startup
            server.Kill(KErrNone);
        } else {
            // Logon OK - start the server
            server.Resume();
        }
        User::WaitForRequest(serverDiedRequestStatus);
        ret = (server.ExitType() == EExitPanic) ? KErrGeneral : serverDiedRequestStatus.Int();
        server.Close();

        if (ret != KErrNone && ret != KErrAlreadyExists) {
            return ret;
        }
    }
}

TInt RUsbOstComm::Disconnect()
{
    return SendReceive(EUsbOstCmdDisconnect);
}

TInt RUsbOstComm::Open()
{
    return SendReceive(EUsbOstCmdCodeOpen);
}

TInt RUsbOstComm::Close()
{
    TInt err = SendReceive(EUsbOstCmdCodeClose);
    RHandleBase::Close();
    return err;
}

TInt RUsbOstComm::RegisterProtocolID(const TOstProtIds aId, TBool aNeedHeader)
{
    TIpcArgs args(aId, aNeedHeader);
    return SendReceive(EUsbOstCmdCodeRegisterId, args);
}

void RUsbOstComm::ReadMessage(TRequestStatus& aStatus, TDes8& aDes)
{
    TIpcArgs args(aDes.MaxLength(), &aDes);
    SendReceive(EUsbOstCmdCodeReadMsg, args, aStatus);
}

TInt RUsbOstComm::ReadCancel()
{
    return SendReceive(EUsbOstCmdCodeReadCancel);
}

void RUsbOstComm::WriteMessage(TRequestStatus& aStatus, const TDesC8& aDes, TBool aHasHeader)
{
    TIpcArgs args(aHasHeader, aDes.Length(), &aDes);
    SendReceive(EUsbOstCmdCodeWriteMsg, args, aStatus);
}

typedef TVersion (*TVersionFunction)(const RUsbOstComm*);
const TInt KVersionOrdinal = 17;

TVersion RUsbOstComm::Version() const
{
    // This function has to go to the DLL, unfortunately
    TVersion result; // Return 0.0.0 on any error
    RLibrary lib;
    TInt err = lib.Load(_L("usbostcomm"));
    if (err) return result;

    TLibraryFunction fn = lib.Lookup(KVersionOrdinal);
    if (fn)
        result = ((TVersionFunction)fn)(this);
    lib.Close();
    return result;
}

#endif //USBHOSTCOMM_H
