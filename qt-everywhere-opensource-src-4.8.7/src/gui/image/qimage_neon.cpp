/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include <qimage.h>
#include <private/qimage_p.h>
#include <private/qsimd_p.h>

#ifdef QT_HAVE_NEON

QT_BEGIN_NAMESPACE

Q_GUI_EXPORT void QT_FASTCALL qt_convert_rgb888_to_rgb32_neon(quint32 *dst, const uchar *src, int len)
{
    if (!len)
        return;

    const quint32 *const end = dst + len;

    // align dst on 64 bits
    const int offsetToAlignOn8Bytes = (reinterpret_cast<quintptr>(dst) >> 2) & 0x1;
    for (int i = 0; i < offsetToAlignOn8Bytes; ++i) {
        *dst++ = qRgb(src[0], src[1], src[2]);
        src += 3;
    }

    if ((len - offsetToAlignOn8Bytes) >= 8) {
        const quint32 *const simdEnd = end - 7;
        register uint8x8_t fullVector asm ("d3") = vdup_n_u8(0xff);
        do {
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
            asm volatile (
                "vld3.8     { d4, d5, d6 }, [%[SRC]] !\n\t"
                "vst4.8     { d3, d4, d5, d6 }, [%[DST],:64] !\n\t"
                : [DST]"+r" (dst), [SRC]"+r" (src)
                : "w"(fullVector)
                : "memory", "d4", "d5", "d6"
            );
#else
            asm volatile (
                "vld3.8     { d0, d1, d2 }, [%[SRC]] !\n\t"
                "vswp d0, d2\n\t"
                "vst4.8     { d0, d1, d2, d3 }, [%[DST],:64] !\n\t"
                : [DST]"+r" (dst), [SRC]"+r" (src)
                : "w"(fullVector)
                : "memory", "d0", "d1", "d2"
            );
#endif
        } while (dst < simdEnd);
    }

    while (dst != end) {
        *dst++ = qRgb(src[0], src[1], src[2]);
        src += 3;
    }
}

void convert_RGB888_to_RGB32_neon(QImageData *dest, const QImageData *src, Qt::ImageConversionFlags)
{
    Q_ASSERT(src->format == QImage::Format_RGB888);
    Q_ASSERT(dest->format == QImage::Format_RGB32 || dest->format == QImage::Format_ARGB32 || dest->format == QImage::Format_ARGB32_Premultiplied);
    Q_ASSERT(src->width == dest->width);
    Q_ASSERT(src->height == dest->height);

    const uchar *src_data = (uchar *) src->data;
    quint32 *dest_data = (quint32 *) dest->data;

    for (int i = 0; i < src->height; ++i) {
        qt_convert_rgb888_to_rgb32_neon(dest_data, src_data, src->width);
        src_data += src->bytes_per_line;
        dest_data = (quint32 *)((uchar*)dest_data + dest->bytes_per_line);
    }
}

QT_END_NAMESPACE

#endif // QT_HAVE_NEON
