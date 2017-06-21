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

#ifndef QOPENWFDDEVICE_H
#define QOPENWFDDEVICE_H

#include "qopenwfdintegration.h"

#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QSocketNotifier>

#include <WF/wfd.h>

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

class QOpenWFDPort;

class QOpenWFDDevice : public QObject
{
    Q_OBJECT
public:
    QOpenWFDDevice(QOpenWFDIntegration *integration, WFDint handle);
    ~QOpenWFDDevice();
    WFDDevice handle() const;
    QOpenWFDIntegration *integration() const;


    bool isPipelineUsed(WFDint pipelineId);
    void addToUsedPipelineSet(WFDint pipelineId, QOpenWFDPort *port);
    void removeFromUsedPipelineSet(WFDint pipelineId);

    gbm_device *gbmDevice() const;
    EGLDisplay eglDisplay() const;
    EGLContext eglContext() const;

    PFNEGLCREATEIMAGEKHRPROC eglCreateImage;
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImage;
    PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEglImageTargetRenderBufferStorage;

    void commit(WFDCommitType type, WFDHandle handle);
    bool isDeviceInitializedAndCommited() const { return mCommitedDevice; }

    void waitForPipelineBindSourceCompleteEvent();

public slots:
    void readEvents(WFDtime wait = 0);
private:
    void initializeGbmAndEgl();
    void handlePortAttachDetach();
    void handlePipelineBindSourceComplete();
    QOpenWFDIntegration *mIntegration;
    WFDint mDeviceEnum;
    WFDDevice mDevice;

    WFDEvent mEvent;
    QSocketNotifier *mEventSocketNotifier;

    QList<QOpenWFDPort *> mPorts;

    QMap<WFDint, QOpenWFDPort *> mUsedPipelines;

    struct gbm_device *mGbmDevice;
    EGLDisplay mEglDisplay;
    EGLContext mEglContext;

    bool mCommitedDevice;
    bool mWaitingForBindSourceEvent;
};

#endif // QOPENWFDDEVICE_H
