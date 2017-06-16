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

#ifndef ANDROIDCAMERA_H
#define ANDROIDCAMERA_H

#include <qobject.h>
#include <QtCore/private/qjni_p.h>
#include <qsize.h>
#include <qrect.h>
#include <QtMultimedia/qcamera.h>

QT_BEGIN_NAMESPACE

class QThread;

class AndroidCameraPrivate;
class AndroidSurfaceTexture;
class AndroidSurfaceHolder;

struct AndroidCameraInfo
{
    QByteArray name;
    QString description;
    QCamera::Position position;
    int orientation;
};

class AndroidCamera : public QObject
{
    Q_OBJECT
    Q_ENUMS(CameraFacing)
    Q_ENUMS(ImageFormat)
public:
    enum CameraFacing {
        CameraFacingBack = 0,
        CameraFacingFront = 1
    };

    enum ImageFormat { // same values as in android.graphics.ImageFormat Java class
        UnknownImageFormat = 0,
        RGB565 = 4,
        NV16 = 16,
        NV21 = 17,
        YUY2 = 20,
        JPEG = 256,
        YV12 = 842094169
    };

    ~AndroidCamera();

    static AndroidCamera *open(int cameraId);

    int cameraId() const;

    bool lock();
    bool unlock();
    bool reconnect();
    void release();

    CameraFacing getFacing();
    int getNativeOrientation();

    QSize getPreferredPreviewSizeForVideo();
    QList<QSize> getSupportedPreviewSizes();

    ImageFormat getPreviewFormat();
    void setPreviewFormat(ImageFormat fmt);
    QList<ImageFormat> getSupportedPreviewFormats();

    QSize previewSize() const;
    void setPreviewSize(const QSize &size);
    bool setPreviewTexture(AndroidSurfaceTexture *surfaceTexture);
    bool setPreviewDisplay(AndroidSurfaceHolder *surfaceHolder);

    bool isZoomSupported();
    int getMaxZoom();
    QList<int> getZoomRatios();
    int getZoom();
    void setZoom(int value);

    QStringList getSupportedFlashModes();
    QString getFlashMode();
    void setFlashMode(const QString &value);

    QStringList getSupportedFocusModes();
    QString getFocusMode();
    void setFocusMode(const QString &value);

    int getMaxNumFocusAreas();
    QList<QRect> getFocusAreas();
    void setFocusAreas(const QList<QRect> &areas);

    void autoFocus();
    void cancelAutoFocus();

    bool isAutoExposureLockSupported();
    bool getAutoExposureLock();
    void setAutoExposureLock(bool toggle);

    bool isAutoWhiteBalanceLockSupported();
    bool getAutoWhiteBalanceLock();
    void setAutoWhiteBalanceLock(bool toggle);

    int getExposureCompensation();
    void setExposureCompensation(int value);
    float getExposureCompensationStep();
    int getMinExposureCompensation();
    int getMaxExposureCompensation();

    QStringList getSupportedSceneModes();
    QString getSceneMode();
    void setSceneMode(const QString &value);

    QStringList getSupportedWhiteBalance();
    QString getWhiteBalance();
    void setWhiteBalance(const QString &value);

    void setRotation(int rotation);
    int getRotation() const;

    QList<QSize> getSupportedPictureSizes();
    void setPictureSize(const QSize &size);
    void setJpegQuality(int quality);

    void startPreview();
    void stopPreview();
    void stopPreviewSynchronous();

    void takePicture();

    void setupPreviewFrameCallback();
    void notifyNewFrames(bool notify);
    void fetchLastPreviewFrame();
    QJNIObjectPrivate getCameraObject();

    static int getNumberOfCameras();
    static void getCameraInfo(int id, AndroidCameraInfo *info);

    static bool initJNI(JNIEnv *env);

Q_SIGNALS:
    void previewSizeChanged();
    void previewStarted();
    void previewFailedToStart();
    void previewStopped();

    void autoFocusStarted();
    void autoFocusComplete(bool success);

    void whiteBalanceChanged();

    void takePictureFailed();
    void pictureExposed();
    void pictureCaptured(const QByteArray &data);
    void lastPreviewFrameFetched(const QVideoFrame &frame);
    void newPreviewFrame(const QVideoFrame &frame);

private:
    AndroidCamera(AndroidCameraPrivate *d, QThread *worker);

    Q_DECLARE_PRIVATE(AndroidCamera)
    AndroidCameraPrivate *d_ptr;
    QScopedPointer<QThread> m_worker;
};

Q_DECLARE_METATYPE(AndroidCamera::ImageFormat)

QT_END_NAMESPACE

#endif // ANDROIDCAMERA_H
