/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the config.tests of the Qt Toolkit.
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

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xge.h>

#ifndef XInput_2_0
#  error "Missing XInput_2_0 #define"
#endif

int main(int, char **)
{
    // need XGenericEventCookie for XInput2 to work
    Display *dpy = 0;
    XEvent xevent;
    if (XGetEventData(dpy, &xevent.xcookie)) {
        XFreeEventData(dpy, &xevent.xcookie);
    }

    XIEvent *xievent;
    xievent = 0;

    XIDeviceEvent *xideviceevent;
    xideviceevent = 0;

    XIHierarchyEvent *xihierarchyevent;
    xihierarchyevent = 0;

    int deviceid = 0;
    int len = 0;
    Atom *atoms = XIListProperties(dpy, deviceid, &len);
    if (atoms)
        XFree(atoms);

    return 0;
}
