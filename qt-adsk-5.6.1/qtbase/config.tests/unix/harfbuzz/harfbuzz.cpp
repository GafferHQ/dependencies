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

#include <harfbuzz/hb.h>

#if !HB_VERSION_ATLEAST(0, 9, 42)
#  error "This version of harfbuzz is too old."
#endif

int main(int, char **)
{
    hb_buffer_t *buffer = hb_buffer_create();

    const uint16_t string[] = { 'A', 'b', 'c' };
    hb_buffer_add_utf16(buffer, string, 3, 0, 3);
    hb_buffer_guess_segment_properties(buffer);
    hb_buffer_set_flags(buffer, hb_buffer_flags_t(HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES));

    hb_buffer_destroy(buffer);

    return 0;
}
