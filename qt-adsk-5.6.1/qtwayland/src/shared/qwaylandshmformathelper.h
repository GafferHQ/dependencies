/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QWAYLANDSHMFORMATHELPER_H
#define QWAYLANDSHMFORMATHELPER_H

#include <QtGui/QImage>

//the correct protocol header for the wayland server or wayland client has to be
//included before this file is included

QT_BEGIN_NAMESPACE

class QWaylandShmFormatHelper
{
public:
    static inline wl_shm_format fromQImageFormat(QImage::Format format);
    static inline QImage::Format fromWaylandShmFormat(wl_shm_format format);
    static inline QVector<wl_shm_format> supportedWaylandFormats();

private:
//IMPLEMENTATION (which has to be inline in the header because of the include trick)
    struct Array
    {
        Array(const size_t size, const wl_shm_format *data)
            : size(size)
            , data(data)
        { }
        const size_t size;
        const wl_shm_format *data;
    };

    static const Array getData()
    {
        static wl_shm_format formats_array[] = {
            wl_shm_format(INT_MIN),    //Format_Invalid,
            wl_shm_format(INT_MIN),    //Format_Mono,
            wl_shm_format(INT_MIN),    //Format_MonoLSB,
            wl_shm_format(INT_MIN),    //Format_Indexed8,
            WL_SHM_FORMAT_XRGB8888,    //Format_RGB32,
            WL_SHM_FORMAT_ARGB8888,    //Format_ARGB32,
            WL_SHM_FORMAT_ARGB8888,    //Format_ARGB32_Premultiplied,
            WL_SHM_FORMAT_RGB565,      //Format_RGB16,
            wl_shm_format(INT_MIN),    //Format_ARGB8565_Premultiplied,
            wl_shm_format(INT_MIN),    //Format_RGB666,
            wl_shm_format(INT_MIN),    //Format_ARGB6666_Premultiplied,
            WL_SHM_FORMAT_XRGB1555,    //Format_RGB555,
            wl_shm_format(INT_MIN),    //Format_ARGB8555_Premultiplied,
            WL_SHM_FORMAT_RGB888,      //Format_RGB888,
            WL_SHM_FORMAT_XRGB4444,    //Format_RGB444,
            WL_SHM_FORMAT_ARGB4444,    //Format_ARGB4444_Premultiplied,
            WL_SHM_FORMAT_XBGR8888,    //Format_RGBX8888,
            WL_SHM_FORMAT_ABGR8888,    //Format_RGBA8888,
            WL_SHM_FORMAT_ABGR8888,    //Format_RGBA8888_Premultiplied,
            WL_SHM_FORMAT_XBGR2101010, //Format_BGR30,
            WL_SHM_FORMAT_ARGB2101010, //Format_A2BGR30_Premultiplied,
            WL_SHM_FORMAT_XRGB2101010, //Format_RGB30,
            WL_SHM_FORMAT_ARGB2101010, //Format_A2RGB30_Premultiplied,
            WL_SHM_FORMAT_C8,          //Format_Alpha8,
            WL_SHM_FORMAT_C8           //Format_Grayscale8,
        };
        const size_t size = sizeof(formats_array) / sizeof(*formats_array);
        return Array(size, formats_array);
    }
};

wl_shm_format QWaylandShmFormatHelper::fromQImageFormat(QImage::Format format)
{
    Array array = getData();
    if (array.size <= size_t(format))
        return wl_shm_format(INT_MIN);
    return array.data[format];
}

QImage::Format QWaylandShmFormatHelper::fromWaylandShmFormat(wl_shm_format format)
{
    Array array = getData();
    for (size_t i = 0; i < array.size; i++) {
        if (array.data[i] == format)
            return QImage::Format(i);
    }
    return QImage::Format_Invalid;
}

QVector<wl_shm_format> QWaylandShmFormatHelper::supportedWaylandFormats()
{
    QVector<wl_shm_format> retFormats;
    Array array = getData();
    for (size_t i = 0; i < array.size; i++) {
        if (int(array.data[i]) != INT_MIN
                && !retFormats.contains(array.data[i])) {
            retFormats.append(array.data[i]);
        }
    }
    return retFormats;
}

QT_END_NAMESPACE

#endif //QWAYLANDSHMFORMATHELPER_H
