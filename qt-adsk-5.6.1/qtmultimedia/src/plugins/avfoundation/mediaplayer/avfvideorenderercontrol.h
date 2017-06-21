/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef AVFVIDEORENDERERCONTROL_H
#define AVFVIDEORENDERERCONTROL_H

#include <QtMultimedia/QVideoRendererControl>
#include <QtCore/QMutex>
#include <QtCore/QSize>

#include "avfvideooutput.h"

#import <CoreVideo/CVBase.h>

QT_BEGIN_NAMESPACE

class AVFDisplayLink;
class AVFVideoFrameRenderer;

class AVFVideoRendererControl : public QVideoRendererControl, public AVFVideoOutput
{
    Q_OBJECT
    Q_INTERFACES(AVFVideoOutput)
public:
    explicit AVFVideoRendererControl(QObject *parent = 0);
    virtual ~AVFVideoRendererControl();

    QAbstractVideoSurface *surface() const;
    void setSurface(QAbstractVideoSurface *surface);

    void setLayer(void *playerLayer);

private Q_SLOTS:
    void updateVideoFrame(const CVTimeStamp &ts);

Q_SIGNALS:
    void surfaceChanged(QAbstractVideoSurface *surface);

private:
    void setupVideoOutput();

    QMutex m_mutex;
    QAbstractVideoSurface *m_surface;

    void *m_playerLayer;

    AVFVideoFrameRenderer *m_frameRenderer;
    AVFDisplayLink *m_displayLink;
    QSize m_nativeSize;
    bool m_enableOpenGL;
};

QT_END_NAMESPACE

#endif // AVFVIDEORENDERERCONTROL_H
