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
#include <QFileInfo>
#include <QDir>

#include <usb.h>

class InterfaceInfo
{
public:
    InterfaceInfo(const QString &mf, const QString &pr, int mfid, int prid);
    QString manufacturer;
    QString product;
    int manufacturerid;
    int productid;
};

InterfaceInfo::InterfaceInfo(const QString &mf, const QString &pr, int mfid, int prid) :
    manufacturer(mf),
    product(pr),
    manufacturerid(mfid),
    productid(prid)
{
    if(mf.isEmpty())
        manufacturer = QString("[%1]").arg(mfid, 4, 16, QChar('0'));
    if(pr.isEmpty())
        product = QString("[%1]").arg(prid, 4, 16, QChar('0'));
}

QList<SerialPortId> enumerateSerialPorts(int loglevel)
{
    QList<QString> eligibleInterfaces;
    QList<InterfaceInfo> eligibleInterfacesInfo;
    QList<SerialPortId> list;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (struct usb_bus *bus = usb_get_busses(); bus; bus = bus->next) {
        for (struct usb_device *device = bus->devices; device; device = device->next) {
            for (int n = 0; n < device->descriptor.bNumConfigurations && device->config; ++n) {
                struct usb_config_descriptor &usbConfig =device->config[n];
                QList<int> usableInterfaces;
                for (int m = 0; m < usbConfig.bNumInterfaces; ++m) {
                    for (int o = 0; o < usbConfig.interface[m].num_altsetting; ++o) {
                        struct usb_interface_descriptor &descriptor = usbConfig.interface[m].altsetting[o];
                        if (descriptor.bInterfaceClass != 2 // "Communication"
                                || descriptor.bInterfaceSubClass != 2 // Abstract (modem)
                                || descriptor.bInterfaceProtocol != 255) // Vendor Specific
                            continue;

                        unsigned char *buf = descriptor.extra;
                        unsigned int size = descriptor.extralen;
                        while (size >= 2 * sizeof(u_int8_t)) {
                            // for Communication devices there is a slave interface for the actual
                            // data transmission.
                            // the extra info stores that as a index for the interface
                            if (buf[0] >= 5 && buf[1] == 36 && buf[2] == 6) { // CDC Union
                                for (int i = 3; i < buf[0]; i++)
                                    usableInterfaces.append((int) buf[i]);
                            }
                            size -= buf[0];
                            buf += buf[0];
                        }
                    }
                }
                
                if (usableInterfaces.isEmpty())
                    continue;
                
                QString manufacturerString;
                QString productString;
                
                usb_dev_handle *devh = usb_open(device);
                if (devh) {
                    QByteArray buf;
                    buf.resize(256);
                    int err = usb_get_string_simple(devh, device->descriptor.iManufacturer, buf.data(), buf.size());
                    if (err < 0) {
                        if (loglevel > 1)
                            qDebug() << "      can't read manufacturer name, error:" << err;
                    } else {
                        manufacturerString = QString::fromAscii(buf);
                        if (loglevel > 1)
                            qDebug() << "      manufacturer:" << manufacturerString;
                    }

                    buf.resize(256);
                    err = usb_get_string_simple(devh, device->descriptor.iProduct, buf.data(), buf.size());
                    if (err < 0) {
                        if (loglevel > 1)
                            qDebug() << "      can't read product name, error:" << err;
                    } else {
                        productString = QString::fromAscii(buf);
                        if (loglevel > 1)
                            qDebug() << "      product:" << productString;
                    }
                    usb_close(devh);
                } else if (loglevel > 0) {
                    qDebug() << "      can't open usb device";
                }

                // second loop to find the actual data interface.
                foreach (int i, usableInterfaces) {
#ifdef Q_OS_MAC
                    eligibleInterfaces << QString("^cu\\.usbmodem.*%1$")
                        .arg(QString("%1").arg(i, 1, 16).toUpper());
                    eligibleInterfacesInfo << InterfaceInfo(manufacturerString, productString, device->descriptor.idVendor, device->descriptor.idProduct);
#else
                    // ### manufacturer and product strings are only readable as root :(
                    if (!manufacturerString.isEmpty() && !productString.isEmpty()) {
                        eligibleInterfaces << QString("usb-%1_%2-if%3")
                             .arg(manufacturerString.replace(QChar(' '), QChar('_')))
                             .arg(productString.replace(QChar(' '), QChar('_')))
                             .arg(i, 2, 16, QChar('0'));
                    } else {
                        eligibleInterfaces << QString("if%1").arg(i, 2, 16, QChar('0')); // fix!
                    }
#endif
                }
#ifndef Q_OS_MAC
                // On mac, we need a 1-1 mapping between eligibleInterfaces and eligibleInterfacesInfo
                // in order to find the friendly name for an interface
                eligibleInterfacesInfo << InterfaceInfo(manufacturerString, productString, device->descriptor.idVendor, device->descriptor.idProduct);
#endif
            }
        }
    }
    
    if (loglevel > 1)
        qDebug() << "      searching for interfaces:" << eligibleInterfaces;

#ifdef Q_OS_MAC
    QDir dir("/dev/");
    bool allowAny = false;
#else
    QDir dir("/dev/serial/by-id/");
    bool allowAny = eligibleInterfaces.isEmpty();
#endif
    foreach (const QFileInfo &info, dir.entryInfoList(QDir::System)) {
        if (!info.isDir()) {
            bool usable = allowAny;
            QString friendlyName = info.fileName();
            foreach (const QString &iface, eligibleInterfaces) {
                if (info.fileName().contains(QRegExp(iface))) {
                    if (loglevel > 1)
                        qDebug() << "      found device file:" << info.fileName() << endl;
#ifdef Q_OS_MAC
                    InterfaceInfo info = eligibleInterfacesInfo[eligibleInterfaces.indexOf(iface)];
                    friendlyName = info.manufacturer + " " + info.product;
#endif
                    usable = true;
                    break;
                }
            }
            if (!usable)
                continue;

            SerialPortId id;
            id.friendlyName = friendlyName;
            id.portName = info.canonicalFilePath();
            list << id;
        }
    }

    if (list.isEmpty() && !eligibleInterfacesInfo.isEmpty() && loglevel > 0) {
        qDebug() << "Possible USB devices found, but without serial drivers:";
        foreach(const InterfaceInfo &iface, eligibleInterfacesInfo) {
            qDebug() << "    Manufacturer:"
                     << iface.manufacturer
                     << "Product:"
                     << iface.product
#ifdef Q_OS_LINUX
                     << endl
                     << "    Load generic driver using:"
                     << QString("sudo modprobe usbserial vendor=0x%1 product=0x%2")
                        .arg(iface.manufacturerid, 4, 16, QChar('0'))
                        .arg(iface.productid, 4, 16, QChar('0'));
#else
                     ;
#endif
        }
    }
    return list;
}

