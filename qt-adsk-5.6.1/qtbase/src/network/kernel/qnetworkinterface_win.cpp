/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Intel Corporation.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#define WIN32_LEAN_AND_MEAN 1

#include "qnetworkinterface.h"
#include "qnetworkinterface_p.h"

#ifndef QT_NO_NETWORKINTERFACE

#include <qhostinfo.h>
#include <qhash.h>
#include <qurl.h>

// Since we need to include winsock2.h, we need to define WIN32_LEAN_AND_MEAN
// (above) so windows.h won't include winsock.h.
// In addition, we need to include winsock2.h before iphlpapi.h and we need
// to include ws2ipdef.h to work around an MinGW-w64 bug
// (http://sourceforge.net/p/mingw-w64/mailman/message/32935366/)
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

typedef NETIO_STATUS (WINAPI *PtrConvertInterfaceLuidToName)(const NET_LUID *, PWSTR, SIZE_T);
static PtrConvertInterfaceLuidToName ptrConvertInterfaceLuidToName = 0;

static void resolveLibs()
{
    // try to find the functions we need from Iphlpapi.dll
    static bool done = false;

    if (!done) {
        HINSTANCE iphlpapiHnd = GetModuleHandle(L"iphlpapi");
        Q_ASSERT(iphlpapiHnd);

#if defined(Q_OS_WINCE)
        // since Windows Embedded Compact 7
        ptrConvertInterfaceLuidToName = (PtrConvertInterfaceLuidToName)GetProcAddress(iphlpapiHnd, L"ConvertInterfaceLuidToNameW");
#else
        // since Windows Vista
        ptrConvertInterfaceLuidToName = (PtrConvertInterfaceLuidToName)GetProcAddress(iphlpapiHnd, "ConvertInterfaceLuidToNameW");
#endif
        done = true;
    }
}

static QHostAddress addressFromSockaddr(sockaddr *sa)
{
    QHostAddress address;
    if (!sa)
        return address;

    if (sa->sa_family == AF_INET)
        address.setAddress(htonl(((sockaddr_in *)sa)->sin_addr.s_addr));
    else if (sa->sa_family == AF_INET6) {
        address.setAddress(((sockaddr_in6 *)sa)->sin6_addr.s6_addr);
        int scope = ((sockaddr_in6 *)sa)->sin6_scope_id;
        if (scope)
            address.setScopeId(QString::number(scope));
    } else
        qWarning("Got unknown socket family %d", sa->sa_family);
    return address;

}

static QHash<QHostAddress, QHostAddress> ipv4Netmasks()
{
    //Retrieve all the IPV4 addresses & netmasks
    IP_ADAPTER_INFO staticBuf[2]; // 2 is arbitrary
    PIP_ADAPTER_INFO pAdapter = staticBuf;
    ULONG bufSize = sizeof staticBuf;
    QHash<QHostAddress, QHostAddress> ipv4netmasks;

    DWORD retval = GetAdaptersInfo(pAdapter, &bufSize);
    if (retval == ERROR_BUFFER_OVERFLOW) {
        // need more memory
        pAdapter = (IP_ADAPTER_INFO *)malloc(bufSize);
        if (!pAdapter)
            return ipv4netmasks;
        // try again
        if (GetAdaptersInfo(pAdapter, &bufSize) != ERROR_SUCCESS) {
            free(pAdapter);
            return ipv4netmasks;
        }
    } else if (retval != ERROR_SUCCESS) {
        // error
        return ipv4netmasks;
    }

    // iterate over the list and add the entries to our listing
    for (PIP_ADAPTER_INFO ptr = pAdapter; ptr; ptr = ptr->Next) {
        for (PIP_ADDR_STRING addr = &ptr->IpAddressList; addr; addr = addr->Next) {
            QHostAddress address(QLatin1String(addr->IpAddress.String));
            QHostAddress mask(QLatin1String(addr->IpMask.String));
            ipv4netmasks[address] = mask;
        }
    }
    if (pAdapter != staticBuf)
        free(pAdapter);

    return ipv4netmasks;

}

static QList<QNetworkInterfacePrivate *> interfaceListingWinXP()
{
    QList<QNetworkInterfacePrivate *> interfaces;
    IP_ADAPTER_ADDRESSES staticBuf[2]; // 2 is arbitrary
    PIP_ADAPTER_ADDRESSES pAdapter = staticBuf;
    ULONG bufSize = sizeof staticBuf;

    const QHash<QHostAddress, QHostAddress> &ipv4netmasks = ipv4Netmasks();
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX |
                  GAA_FLAG_SKIP_DNS_SERVER |
                  GAA_FLAG_SKIP_MULTICAST;
    ULONG retval = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAdapter, &bufSize);
    if (retval == ERROR_BUFFER_OVERFLOW) {
        // need more memory
        pAdapter = (IP_ADAPTER_ADDRESSES *)malloc(bufSize);
        if (!pAdapter)
            return interfaces;
        // try again
        if (GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAdapter, &bufSize) != ERROR_SUCCESS) {
            free(pAdapter);
            return interfaces;
        }
    } else if (retval != ERROR_SUCCESS) {
        // error
        return interfaces;
    }

    // iterate over the list and add the entries to our listing
    for (PIP_ADAPTER_ADDRESSES ptr = pAdapter; ptr; ptr = ptr->Next) {
        QNetworkInterfacePrivate *iface = new QNetworkInterfacePrivate;
        interfaces << iface;

        iface->index = 0;
        if (ptr->Length >= offsetof(IP_ADAPTER_ADDRESSES, Ipv6IfIndex) && ptr->Ipv6IfIndex != 0)
            iface->index = ptr->Ipv6IfIndex;
        else if (ptr->IfIndex != 0)
            iface->index = ptr->IfIndex;

        iface->flags = QNetworkInterface::CanBroadcast;
        if (ptr->OperStatus == IfOperStatusUp)
            iface->flags |= QNetworkInterface::IsUp | QNetworkInterface::IsRunning;
        if ((ptr->Flags & IP_ADAPTER_NO_MULTICAST) == 0)
            iface->flags |= QNetworkInterface::CanMulticast;
        if (ptr->IfType == IF_TYPE_PPP)
            iface->flags |= QNetworkInterface::IsPointToPoint;

        if (ptrConvertInterfaceLuidToName && ptr->Length >= offsetof(IP_ADAPTER_ADDRESSES, Luid)) {
            // use ConvertInterfaceLuidToName because that returns a friendlier name, though not
            // as friendly as FriendlyName below
            WCHAR buf[IF_MAX_STRING_SIZE + 1];
            if (ptrConvertInterfaceLuidToName(&ptr->Luid, buf, sizeof(buf)/sizeof(buf[0])) == NO_ERROR)
                iface->name = QString::fromWCharArray(buf);
        }
        if (iface->name.isEmpty())
            iface->name = QString::fromLocal8Bit(ptr->AdapterName);

        iface->friendlyName = QString::fromWCharArray(ptr->FriendlyName);
        if (ptr->PhysicalAddressLength)
            iface->hardwareAddress = iface->makeHwAddress(ptr->PhysicalAddressLength,
                                                          ptr->PhysicalAddress);
        else
            // loopback if it has no address
            iface->flags |= QNetworkInterface::IsLoopBack;

        // The GetAdaptersAddresses call has an interesting semantic:
        // It can return a number N of addresses and a number M of prefixes.
        // But if you have IPv6 addresses, generally N > M.
        // I cannot find a way to relate the Address to the Prefix, aside from stopping
        // the iteration at the last Prefix entry and assume that it applies to all addresses
        // from that point on.
        PIP_ADAPTER_PREFIX pprefix = 0;
        if (ptr->Length >= offsetof(IP_ADAPTER_ADDRESSES, FirstPrefix))
            pprefix = ptr->FirstPrefix;
        for (PIP_ADAPTER_UNICAST_ADDRESS addr = ptr->FirstUnicastAddress; addr; addr = addr->Next) {
            QNetworkAddressEntry entry;
            entry.setIp(addressFromSockaddr(addr->Address.lpSockaddr));
            if (pprefix) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    entry.setNetmask(ipv4netmasks[entry.ip()]);

                    // broadcast address is set on postProcess()
                } else { //IPV6
                    entry.setPrefixLength(pprefix->PrefixLength);
                }
                pprefix = pprefix->Next ? pprefix->Next : pprefix;
            }
            iface->addressEntries << entry;
        }
    }

    if (pAdapter != staticBuf)
        free(pAdapter);

    return interfaces;
}

QList<QNetworkInterfacePrivate *> QNetworkInterfaceManager::scan()
{
    resolveLibs();
    return interfaceListingWinXP();
}

QString QHostInfo::localDomainName()
{
    resolveLibs();

    FIXED_INFO info, *pinfo;
    ULONG bufSize = sizeof info;
    pinfo = &info;
    if (GetNetworkParams(pinfo, &bufSize) == ERROR_BUFFER_OVERFLOW) {
        pinfo = (FIXED_INFO *)malloc(bufSize);
        if (!pinfo)
            return QString();
        // try again
        if (GetNetworkParams(pinfo, &bufSize) != ERROR_SUCCESS) {
            free(pinfo);
            return QString();   // error
        }
    }

    QString domainName = QUrl::fromAce(pinfo->DomainName);

    if (pinfo != &info)
        free(pinfo);

    return domainName;
}

QT_END_NAMESPACE

#endif // QT_NO_NETWORKINTERFACE
