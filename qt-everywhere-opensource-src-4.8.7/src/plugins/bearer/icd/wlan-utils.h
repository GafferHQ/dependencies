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


#ifndef WLAN_UTILS_H
#define WLAN_UTILS_H

/** Originally taken from: libicd-network-wlan-dev.h*/

#include <glib.h>
#include <dbus/dbus.h>
#include <wlancond.h>
#include <icd/network_api_defines.h>

/* capability bits inside network attributes var */
#define NWATTR_WPS_MASK       0x0000F000
#define NWATTR_ALGORITHM_MASK 0x00000F00
#define NWATTR_WPA2_MASK      0x00000080
#define NWATTR_METHOD_MASK    0x00000078
#define NWATTR_MODE_MASK      0x00000007

#define CAP_LOCALMASK         0x0FFFE008

/* how much to shift between capability and network attributes var */
#define CAP_SHIFT_WPS        3
#define CAP_SHIFT_ALGORITHM 20
#define CAP_SHIFT_WPA2       1
#define CAP_SHIFT_METHOD     1
#define CAP_SHIFT_MODE       0
#define CAP_SHIFT_ALWAYS_ONLINE 26

/* ------------------------------------------------------------------------- */
/* From combined to capability */
static inline dbus_uint32_t nwattr2cap(guint nwattrs, dbus_uint32_t *cap)
{
	guint oldval = *cap;

	*cap &= CAP_LOCALMASK; /* clear old capabilities */
	*cap |=
		((nwattrs & ICD_NW_ATTR_ALWAYS_ONLINE) >> CAP_SHIFT_ALWAYS_ONLINE) |
		((nwattrs & NWATTR_WPS_MASK) >> CAP_SHIFT_WPS) |
		((nwattrs & NWATTR_ALGORITHM_MASK) << CAP_SHIFT_ALGORITHM) |
		((nwattrs & NWATTR_WPA2_MASK) << CAP_SHIFT_WPA2) |
		((nwattrs & NWATTR_METHOD_MASK) << CAP_SHIFT_METHOD) |
		(nwattrs & NWATTR_MODE_MASK);

	return oldval;
}


/* ------------------------------------------------------------------------- */
/* From capability to combined */
static inline guint cap2nwattr(dbus_uint32_t cap, guint *nwattrs)
{
	guint oldval = *nwattrs;

	*nwattrs &= ~ICD_NW_ATTR_LOCALMASK; /* clear old capabilities */
        *nwattrs |=
#ifdef WLANCOND_WPS_MASK
		((cap & WLANCOND_WPS_MASK) << CAP_SHIFT_WPS) |
#endif
		((cap & (WLANCOND_ENCRYPT_ALG_MASK |
			 WLANCOND_ENCRYPT_GROUP_ALG_MASK)) >> CAP_SHIFT_ALGORITHM)|
		((cap & WLANCOND_ENCRYPT_WPA2_MASK) >> CAP_SHIFT_WPA2) |
		((cap & WLANCOND_ENCRYPT_METHOD_MASK) >> CAP_SHIFT_METHOD) |
		(cap & WLANCOND_MODE_MASK);

	return oldval;
}


#endif
