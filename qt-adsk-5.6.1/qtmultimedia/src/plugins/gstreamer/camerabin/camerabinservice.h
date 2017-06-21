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

#ifndef CAMERABINCAPTURESERVICE_H
#define CAMERABINCAPTURESERVICE_H

#include <qmediaservice.h>

#include <gst/gst.h>

QT_BEGIN_NAMESPACE
class QAudioInputSelectorControl;
class QVideoDeviceSelectorControl;


class CameraBinSession;
class CameraBinControl;
class QGstreamerMessage;
class QGstreamerBusHelper;
class QGstreamerVideoRenderer;
class QGstreamerVideoWindow;
class QGstreamerVideoWidgetControl;
class QGstreamerElementFactory;
class CameraBinMetaData;
class CameraBinImageCapture;
class CameraBinMetaData;
class CameraBinViewfinderSettings;
class CameraBinViewfinderSettings2;

class CameraBinService : public QMediaService
{
    Q_OBJECT

public:
    CameraBinService(GstElementFactory *sourceFactory, QObject *parent = 0);
    virtual ~CameraBinService();

    QMediaControl *requestControl(const char *name);
    void releaseControl(QMediaControl *);

    static bool isCameraBinAvailable();

private:
    void setAudioPreview(GstElement*);

    CameraBinSession *m_captureSession;
    CameraBinMetaData *m_metaDataControl;

    QAudioInputSelectorControl *m_audioInputSelector;
    QVideoDeviceSelectorControl *m_videoInputDevice;

    QMediaControl *m_videoOutput;

    QMediaControl *m_videoRenderer;
    QGstreamerVideoWindow *m_videoWindow;
#if defined(HAVE_WIDGETS)
    QGstreamerVideoWidgetControl *m_videoWidgetControl;
#endif
    CameraBinImageCapture *m_imageCaptureControl;
    QMediaControl *m_cameraInfoControl;

    CameraBinViewfinderSettings *m_viewfinderSettingsControl;
    CameraBinViewfinderSettings2 *m_viewfinderSettingsControl2;
};

QT_END_NAMESPACE

#endif // CAMERABINCAPTURESERVICE_H
