/****************************************************************************
**
** Copyright (C) 2014 Pelagicore AG
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "qsgvivantevideonode.h"
#include "qsgvivantevideomaterialshader.h"
#include "qsgvivantevideomaterial.h"

QMap<QVideoFrame::PixelFormat, GLenum> QSGVivanteVideoNode::static_VideoFormat2GLFormatMap = QMap<QVideoFrame::PixelFormat, GLenum>();

QSGVivanteVideoNode::QSGVivanteVideoNode(const QVideoSurfaceFormat &format) :
    mFormat(format)
{
    setFlag(QSGNode::OwnsMaterial, true);
    mMaterial = new QSGVivanteVideoMaterial();
    setMaterial(mMaterial);
}

QSGVivanteVideoNode::~QSGVivanteVideoNode()
{
}

void QSGVivanteVideoNode::setCurrentFrame(const QVideoFrame &frame, FrameFlags flags)
{
    mMaterial->setCurrentFrame(frame, flags);
    markDirty(DirtyMaterial);
}

const QMap<QVideoFrame::PixelFormat, GLenum>& QSGVivanteVideoNode::getVideoFormat2GLFormatMap()
{
    if (static_VideoFormat2GLFormatMap.isEmpty()) {
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_YUV420P,  GL_VIV_I420);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_YV12,     GL_VIV_YV12);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_NV12,     GL_VIV_NV12);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_NV21,     GL_VIV_NV21);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_UYVY,     GL_VIV_UYVY);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_YUYV,     GL_VIV_YUY2);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_RGB32,    GL_BGRA_EXT);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_ARGB32,   GL_BGRA_EXT);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_BGR32,    GL_RGBA);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_BGRA32,   GL_RGBA);
        static_VideoFormat2GLFormatMap.insert(QVideoFrame::Format_RGB565,   GL_RGB565);
    }

    return static_VideoFormat2GLFormatMap;
}


int QSGVivanteVideoNode::getBytesForPixelFormat(QVideoFrame::PixelFormat pixelformat)
{
    switch (pixelformat) {
    case QVideoFrame::Format_YUV420P: return 1;
    case QVideoFrame::Format_YV12: return 1;
    case QVideoFrame::Format_NV12: return 1;
    case QVideoFrame::Format_NV21: return 1;
    case QVideoFrame::Format_UYVY: return 2;
    case QVideoFrame::Format_YUYV: return 2;
    case QVideoFrame::Format_RGB32: return 4;
    case QVideoFrame::Format_ARGB32: return 4;
    case QVideoFrame::Format_BGR32: return 4;
    case QVideoFrame::Format_BGRA32: return 4;
    case QVideoFrame::Format_RGB565: return 2;
    default: return 1;
    }
}



