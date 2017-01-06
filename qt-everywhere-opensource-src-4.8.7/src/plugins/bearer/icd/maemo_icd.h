/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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


#ifndef MAEMO_ICD_H
#define MAEMO_ICD_H

#include <QObject>
#include <QStringList>
#include <QByteArray>
#include <QMetaType>
#include <QtDBus>
#include <QDBusArgument>

#include <glib.h>
#include <icd/dbus_api.h>
#include <icd/osso-ic.h>
#include <icd/osso-ic-dbus.h>
#include <icd/network_api_defines.h>

#define ICD_LONG_SCAN_TIMEOUT (30*1000)  /* 30sec */
#define ICD_SHORT_SCAN_TIMEOUT (10*1000)  /* 10sec */
#define ICD_SHORT_CONNECT_TIMEOUT (10*1000) /* 10sec */
#define ICD_LONG_CONNECT_TIMEOUT (150*1000) /* 2.5min */

namespace Maemo {

struct CommonParams {
	QString service_type;
	uint service_attrs;
	QString service_id;
	QString network_type;
	uint network_attrs;
	QByteArray network_id;
};

struct IcdScanResult {
	uint status; // see #icd_scan_status
	uint timestamp; // when last seen
	QString service_name;
	uint service_priority; // within a service type
	QString network_name;
	uint network_priority;
	struct CommonParams scan;
	uint signal_strength; // quality, 0 (none) - 10 (good)
	QString station_id; // e.g. MAC address or similar id
	uint signal_dB; // use signal strength above unless you know what you are doing

	IcdScanResult() {
		status = timestamp = scan.service_attrs = service_priority =
			scan.network_attrs = network_priority = signal_strength =
			signal_dB = 0;
	}
};

struct IcdStateResult {
	struct CommonParams params;
	QString error;
	uint state;
};

struct IcdStatisticsResult {
	struct CommonParams params;
	uint time_active;  // in seconds
	enum icd_nw_levels signal_strength; // see network_api_defines.h in icd2-dev package
	uint bytes_sent;
	uint bytes_received;
};

struct IcdIPInformation {
	QString address;
	QString netmask;
	QString default_gateway;
	QString dns1;
	QString dns2;
	QString dns3;
};

struct IcdAddressInfoResult {
	struct CommonParams params;
	QList<IcdIPInformation> ip_info;
};

enum IcdDbusInterfaceVer {
	IcdOldDbusInterface = 0,  // use the old OSSO-IC interface
	IcdNewDbusInterface       // use the new Icd2 interface (default)
};


class IcdPrivate;
class Icd : public QObject
{
    Q_OBJECT

public:
    Icd(QObject *parent = 0);
    Icd(unsigned int timeout, QObject *parent = 0);
    Icd(unsigned int timeout, IcdDbusInterfaceVer ver, QObject *parent = 0);
    ~Icd();

    /* Icd2 dbus API functions */
    QStringList scan(icd_scan_request_flags flags,
		     QStringList &network_types,
		     QList<IcdScanResult>& scan_results,
		     QString& error);

    uint state(QString& service_type, uint service_attrs,
	       QString& service_id, QString& network_type,
	       uint network_attrs, QByteArray& network_id,
	       IcdStateResult &state_result);

    uint addrinfo(QString& service_type, uint service_attrs,
		  QString& service_id, QString& network_type,
		  uint network_attrs, QByteArray& network_id,
		  IcdAddressInfoResult& addr_result);

    uint state(QList<IcdStateResult>& state_results);
    uint statistics(QList<IcdStatisticsResult>& stats_results);
    uint addrinfo(QList<IcdAddressInfoResult>& addr_results);

private Q_SLOTS:
    void icdSignalReceived(const QString& interface, 
                        const QString& signal,
                        const QList<QVariant>& args);
    void icdCallReply(const QString& method, 
                   const QList<QVariant>& args,
                   const QString& error);

private:
    IcdPrivate *d;
    friend class IcdPrivate;
};

}  // Maemo namespace

#endif
