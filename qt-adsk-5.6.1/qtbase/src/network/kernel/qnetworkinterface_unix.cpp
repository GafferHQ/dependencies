/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include "qset.h"
#include "qnetworkinterface.h"
#include "qnetworkinterface_p.h"
#include "qalgorithms.h"
#include "private/qnet_unix_p.h"

#ifndef QT_NO_NETWORKINTERFACE

#define IP_MULTICAST    // make AIX happy and define IFF_MULTICAST

#include <sys/types.h>
#include <sys/socket.h>

#ifdef Q_OS_SOLARIS
# include <sys/sockio.h>
#endif
#include <net/if.h>

#if defined(QT_LINUXBASE)
#  define QT_NO_GETIFADDRS
#endif

#ifdef Q_OS_ANDROID
// android lacks if_nameindex
# define QT_NO_IPV6IFNAME
#endif

#ifdef Q_OS_HAIKU
# include <sys/sockio.h>
# define IFF_RUNNING 0x0001
#endif

#ifndef QT_NO_GETIFADDRS
# include <ifaddrs.h>
#endif

#ifdef QT_LINUXBASE
#  include <arpa/inet.h>
#  ifndef SIOCGIFBRDADDR
#    define SIOCGIFBRDADDR 0x8919
#  endif
#endif // QT_LINUXBASE

#include <qplatformdefs.h>

QT_BEGIN_NAMESPACE

static QHostAddress addressFromSockaddr(sockaddr *sa, int ifindex = 0, const QString &ifname = QString())
{
    QHostAddress address;
    if (!sa)
        return address;

    if (sa->sa_family == AF_INET)
        address.setAddress(htonl(((sockaddr_in *)sa)->sin_addr.s_addr));
    else if (sa->sa_family == AF_INET6) {
        address.setAddress(((sockaddr_in6 *)sa)->sin6_addr.s6_addr);
        int scope = ((sockaddr_in6 *)sa)->sin6_scope_id;
        if (scope && scope == ifindex) {
            // this is the most likely scenario:
            // a scope ID in a socket is that of the interface this address came from
            address.setScopeId(ifname);
        } else  if (scope) {
#ifndef QT_NO_IPV6IFNAME
            char scopeid[IFNAMSIZ];
            if (::if_indextoname(scope, scopeid)) {
                address.setScopeId(QLatin1String(scopeid));
            } else
#endif
                address.setScopeId(QString::number(uint(scope)));
        }
    }
    return address;

}

static QNetworkInterface::InterfaceFlags convertFlags(uint rawFlags)
{
    QNetworkInterface::InterfaceFlags flags = 0;
    flags |= (rawFlags & IFF_UP) ? QNetworkInterface::IsUp : QNetworkInterface::InterfaceFlag(0);
    flags |= (rawFlags & IFF_RUNNING) ? QNetworkInterface::IsRunning : QNetworkInterface::InterfaceFlag(0);
    flags |= (rawFlags & IFF_BROADCAST) ? QNetworkInterface::CanBroadcast : QNetworkInterface::InterfaceFlag(0);
    flags |= (rawFlags & IFF_LOOPBACK) ? QNetworkInterface::IsLoopBack : QNetworkInterface::InterfaceFlag(0);
#ifdef IFF_POINTOPOINT //cygwin doesn't define IFF_POINTOPOINT
    flags |= (rawFlags & IFF_POINTOPOINT) ? QNetworkInterface::IsPointToPoint : QNetworkInterface::InterfaceFlag(0);
#endif

#ifdef IFF_MULTICAST
    flags |= (rawFlags & IFF_MULTICAST) ? QNetworkInterface::CanMulticast : QNetworkInterface::InterfaceFlag(0);
#endif
    return flags;
}

#ifdef QT_NO_GETIFADDRS
// getifaddrs not available

static QSet<QByteArray> interfaceNames(int socket)
{
    QSet<QByteArray> result;
#ifdef QT_NO_IPV6IFNAME
    QByteArray storageBuffer;
    struct ifconf interfaceList;
    static const int STORAGEBUFFER_GROWTH = 256;

    forever {
        // grow the storage buffer
        storageBuffer.resize(storageBuffer.size() + STORAGEBUFFER_GROWTH);
        interfaceList.ifc_buf = storageBuffer.data();
        interfaceList.ifc_len = storageBuffer.size();

        // get the interface list
        if (qt_safe_ioctl(socket, SIOCGIFCONF, &interfaceList) >= 0) {
            if (int(interfaceList.ifc_len + sizeof(ifreq) + 64) < storageBuffer.size()) {
                // if the buffer was big enough, break
                storageBuffer.resize(interfaceList.ifc_len);
                break;
            }
        } else {
            // internal error
            return result;
        }
        if (storageBuffer.size() > 100000) {
            // out of space
            return result;
        }
    }

    int interfaceCount = interfaceList.ifc_len / sizeof(ifreq);
    for (int i = 0; i < interfaceCount; ++i) {
        QByteArray name = QByteArray(interfaceList.ifc_req[i].ifr_name);
        if (!name.isEmpty())
            result << name;
    }

    return result;
#else
    Q_UNUSED(socket);

    // use if_nameindex
    struct if_nameindex *interfaceList = ::if_nameindex();
    for (struct if_nameindex *ptr = interfaceList; ptr && ptr->if_name; ++ptr)
        result << ptr->if_name;

    if_freenameindex(interfaceList);
    return result;
#endif
}

static QNetworkInterfacePrivate *findInterface(int socket, QList<QNetworkInterfacePrivate *> &interfaces,
                                               struct ifreq &req)
{
    QNetworkInterfacePrivate *iface = 0;
    int ifindex = 0;

#if !defined(QT_NO_IPV6IFNAME) || defined(SIOCGIFINDEX)
    // Get the interface index
#  ifdef SIOCGIFINDEX
    if (qt_safe_ioctl(socket, SIOCGIFINDEX, &req) >= 0)
        ifindex = req.ifr_ifindex;
#  else
    ifindex = if_nametoindex(req.ifr_name);
#  endif

    // find the interface data
    QList<QNetworkInterfacePrivate *>::Iterator if_it = interfaces.begin();
    for ( ; if_it != interfaces.end(); ++if_it)
        if ((*if_it)->index == ifindex) {
            // existing interface
            iface = *if_it;
            break;
        }
#else
    // Search by name
    QList<QNetworkInterfacePrivate *>::Iterator if_it = interfaces.begin();
    for ( ; if_it != interfaces.end(); ++if_it)
        if ((*if_it)->name == QLatin1String(req.ifr_name)) {
            // existing interface
            iface = *if_it;
            break;
        }
#endif

    if (!iface) {
        // new interface, create data:
        iface = new QNetworkInterfacePrivate;
        iface->index = ifindex;
        interfaces << iface;
    }

    return iface;
}

static QList<QNetworkInterfacePrivate *> interfaceListing()
{
    QList<QNetworkInterfacePrivate *> interfaces;

    int socket;
    if ((socket = qt_safe_socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == -1)
        return interfaces;      // error

    QSet<QByteArray> names = interfaceNames(socket);
    QSet<QByteArray>::ConstIterator it = names.constBegin();
    for ( ; it != names.constEnd(); ++it) {
        ifreq req;
        memset(&req, 0, sizeof(ifreq));
        memcpy(req.ifr_name, *it, qMin<int>(it->length() + 1, sizeof(req.ifr_name) - 1));

        QNetworkInterfacePrivate *iface = findInterface(socket, interfaces, req);

#ifdef SIOCGIFNAME
        // Get the canonical name
        QByteArray oldName = req.ifr_name;
        if (qt_safe_ioctl(socket, SIOCGIFNAME, &req) >= 0) {
            iface->name = QString::fromLatin1(req.ifr_name);

            // reset the name:
            memcpy(req.ifr_name, oldName, qMin<int>(oldName.length() + 1, sizeof(req.ifr_name) - 1));
        } else
#endif
        {
            // use this name anyways
            iface->name = QString::fromLatin1(req.ifr_name);
        }

        // Get interface flags
        if (qt_safe_ioctl(socket, SIOCGIFFLAGS, &req) >= 0) {
            iface->flags = convertFlags(req.ifr_flags);
        }

#ifdef SIOCGIFHWADDR
        // Get the HW address
        if (qt_safe_ioctl(socket, SIOCGIFHWADDR, &req) >= 0) {
            uchar *addr = (uchar *)req.ifr_addr.sa_data;
            iface->hardwareAddress = iface->makeHwAddress(6, addr);
        }
#endif

        // Get the address of the interface
        QNetworkAddressEntry entry;
        if (qt_safe_ioctl(socket, SIOCGIFADDR, &req) >= 0) {
            sockaddr *sa = &req.ifr_addr;
            entry.setIp(addressFromSockaddr(sa));

            // Get the interface broadcast address
            if (iface->flags & QNetworkInterface::CanBroadcast) {
                if (qt_safe_ioctl(socket, SIOCGIFBRDADDR, &req) >= 0) {
                    sockaddr *sa = &req.ifr_addr;
                    if (sa->sa_family == AF_INET)
                        entry.setBroadcast(addressFromSockaddr(sa));
                }
            }

            // Get the interface netmask
            if (qt_safe_ioctl(socket, SIOCGIFNETMASK, &req) >= 0) {
                sockaddr *sa = &req.ifr_addr;
                entry.setNetmask(addressFromSockaddr(sa));
            }

            iface->addressEntries << entry;
        }
    }

    ::close(socket);
    return interfaces;
}

#else
// use getifaddrs

// platform-specific defs:
# ifdef Q_OS_LINUX
QT_BEGIN_INCLUDE_NAMESPACE
#  include <features.h>
QT_END_INCLUDE_NAMESPACE
# endif

# if defined(Q_OS_LINUX) &&  __GLIBC__ - 0 >= 2 && __GLIBC_MINOR__ - 0 >= 1 && !defined(QT_LINUXBASE)
#  include <netpacket/packet.h>

static QList<QNetworkInterfacePrivate *> createInterfaces(ifaddrs *rawList)
{
    QList<QNetworkInterfacePrivate *> interfaces;
    QSet<QString> seenInterfaces;
    QVarLengthArray<int, 16> seenIndexes;   // faster than QSet<int>

    // On Linux, glibc, uClibc and MUSL obtain the address listing via two
    // netlink calls: first an RTM_GETLINK to obtain the interface listing,
    // then one RTM_GETADDR to get all the addresses (uClibc implementation is
    // copied from glibc; Bionic currently doesn't support getifaddrs). They
    // synthesize AF_PACKET addresses from the RTM_GETLINK responses, which
    // means by construction they currently show up first in the interface
    // listing.
    for (ifaddrs *ptr = rawList; ptr; ptr = ptr->ifa_next) {
        if (ptr->ifa_addr && ptr->ifa_addr->sa_family == AF_PACKET) {
            sockaddr_ll *sll = (sockaddr_ll *)ptr->ifa_addr;
            QNetworkInterfacePrivate *iface = new QNetworkInterfacePrivate;
            interfaces << iface;
            iface->index = sll->sll_ifindex;
            iface->name = QString::fromLatin1(ptr->ifa_name);
            iface->flags = convertFlags(ptr->ifa_flags);
            iface->hardwareAddress = iface->makeHwAddress(sll->sll_halen, (uchar*)sll->sll_addr);

            Q_ASSERT(!seenIndexes.contains(iface->index));
            seenIndexes.append(iface->index);
            seenInterfaces.insert(iface->name);
        }
    }

    // see if we missed anything:
    // - virtual interfaces with no HW address have no AF_PACKET
    // - interface labels have no AF_PACKET, but shouldn't be shown as a new interface
    for (ifaddrs *ptr = rawList; ptr; ptr = ptr->ifa_next) {
        if (!ptr->ifa_addr || ptr->ifa_addr->sa_family != AF_PACKET) {
            QString name = QString::fromLatin1(ptr->ifa_name);
            if (seenInterfaces.contains(name))
                continue;

            int ifindex = if_nametoindex(ptr->ifa_name);
            if (seenIndexes.contains(ifindex))
                continue;

            QNetworkInterfacePrivate *iface = new QNetworkInterfacePrivate;
            interfaces << iface;
            iface->name = name;
            iface->flags = convertFlags(ptr->ifa_flags);
            iface->index = ifindex;
        }
    }

    return interfaces;
}

# elif defined(Q_OS_BSD4)
QT_BEGIN_INCLUDE_NAMESPACE
#  include <net/if_dl.h>
QT_END_INCLUDE_NAMESPACE

static QList<QNetworkInterfacePrivate *> createInterfaces(ifaddrs *rawList)
{
    QList<QNetworkInterfacePrivate *> interfaces;

    // on NetBSD we use AF_LINK and sockaddr_dl
    // scan the list for that family
    for (ifaddrs *ptr = rawList; ptr; ptr = ptr->ifa_next)
        if (ptr->ifa_addr && ptr->ifa_addr->sa_family == AF_LINK) {
            QNetworkInterfacePrivate *iface = new QNetworkInterfacePrivate;
            interfaces << iface;

            sockaddr_dl *sdl = (sockaddr_dl *)ptr->ifa_addr;
            iface->index = sdl->sdl_index;
            iface->name = QString::fromLatin1(ptr->ifa_name);
            iface->flags = convertFlags(ptr->ifa_flags);
            iface->hardwareAddress = iface->makeHwAddress(sdl->sdl_alen, (uchar*)LLADDR(sdl));
        }

    return interfaces;
}

# else  // Generic version

static QList<QNetworkInterfacePrivate *> createInterfaces(ifaddrs *rawList)
{
    QList<QNetworkInterfacePrivate *> interfaces;

    // make sure there's one entry for each interface
    for (ifaddrs *ptr = rawList; ptr; ptr = ptr->ifa_next) {
        // Get the interface index
        int ifindex = if_nametoindex(ptr->ifa_name);

        QList<QNetworkInterfacePrivate *>::Iterator if_it = interfaces.begin();
        for ( ; if_it != interfaces.end(); ++if_it)
            if ((*if_it)->index == ifindex)
                // this one has been added already
                break;

        if (if_it == interfaces.end()) {
            // none found, create
            QNetworkInterfacePrivate *iface = new QNetworkInterfacePrivate;
            interfaces << iface;

            iface->index = ifindex;
            iface->name = QString::fromLatin1(ptr->ifa_name);
            iface->flags = convertFlags(ptr->ifa_flags);
        }
    }

    return interfaces;
}

# endif


static QList<QNetworkInterfacePrivate *> interfaceListing()
{
    QList<QNetworkInterfacePrivate *> interfaces;

    ifaddrs *interfaceListing;
    if (getifaddrs(&interfaceListing) == -1) {
        // error
        return interfaces;
    }

    interfaces = createInterfaces(interfaceListing);
    for (ifaddrs *ptr = interfaceListing; ptr; ptr = ptr->ifa_next) {
        // Find the interface
        QString name = QString::fromLatin1(ptr->ifa_name);
        QNetworkInterfacePrivate *iface = 0;
        QList<QNetworkInterfacePrivate *>::Iterator if_it = interfaces.begin();
        for ( ; if_it != interfaces.end(); ++if_it)
            if ((*if_it)->name == name) {
                // found this interface already
                iface = *if_it;
                break;
            }

        if (!iface) {
            // it may be an interface label, search by interface index
            int ifindex = if_nametoindex(ptr->ifa_name);
            for (if_it = interfaces.begin(); if_it != interfaces.end(); ++if_it)
                if ((*if_it)->index == ifindex) {
                    // found this interface already
                    iface = *if_it;
                    break;
                }
        }

        if (!iface) {
            // skip all non-IP interfaces
            continue;
        }

        QNetworkAddressEntry entry;
        entry.setIp(addressFromSockaddr(ptr->ifa_addr, iface->index, iface->name));
        if (entry.ip().isNull())
            // could not parse the address
            continue;

        entry.setNetmask(addressFromSockaddr(ptr->ifa_netmask, iface->index, iface->name));
        if (iface->flags & QNetworkInterface::CanBroadcast)
            entry.setBroadcast(addressFromSockaddr(ptr->ifa_broadaddr, iface->index, iface->name));

        iface->addressEntries << entry;
    }

    freeifaddrs(interfaceListing);
    return interfaces;
}
#endif

QList<QNetworkInterfacePrivate *> QNetworkInterfaceManager::scan()
{
    return interfaceListing();
}

QT_END_NAMESPACE

#endif // QT_NO_NETWORKINTERFACE
