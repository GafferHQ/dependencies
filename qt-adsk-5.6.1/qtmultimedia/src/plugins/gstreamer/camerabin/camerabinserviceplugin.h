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


#ifndef CAMERABINSERVICEPLUGIN_H
#define CAMERABINSERVICEPLUGIN_H

#include <qmediaserviceproviderplugin.h>
#include <private/qgstreamervideoinputdevicecontrol_p.h>

#include <gst/gst.h>

QT_BEGIN_NAMESPACE

class CameraBinServicePlugin
    : public QMediaServiceProviderPlugin
    , public QMediaServiceSupportedDevicesInterface
    , public QMediaServiceDefaultDeviceInterface
    , public QMediaServiceFeaturesInterface
    , public QMediaServiceCameraInfoInterface
{
    Q_OBJECT
    Q_INTERFACES(QMediaServiceSupportedDevicesInterface)
    Q_INTERFACES(QMediaServiceDefaultDeviceInterface)
    Q_INTERFACES(QMediaServiceFeaturesInterface)
    Q_INTERFACES(QMediaServiceCameraInfoInterface)
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.mediaserviceproviderfactory/5.0" FILE "camerabin.json")
public:
    CameraBinServicePlugin();
    ~CameraBinServicePlugin();

    QMediaService* create(QString const& key);
    void release(QMediaService *service);

    QMediaServiceProviderHint::Features supportedFeatures(const QByteArray &service) const;

    QByteArray defaultDevice(const QByteArray &service) const;
    QList<QByteArray> devices(const QByteArray &service) const;
    QString deviceDescription(const QByteArray &service, const QByteArray &device);
    QVariant deviceProperty(const QByteArray &service, const QByteArray &device, const QByteArray &property);

    QCamera::Position cameraPosition(const QByteArray &device) const;
    int cameraOrientation(const QByteArray &device) const;

private:
    GstElementFactory *sourceFactory() const;

    mutable GstElementFactory *m_sourceFactory;
};

QT_END_NAMESPACE

#endif // QGSTREAMERCAPTURESERVICEPLUGIN_H
