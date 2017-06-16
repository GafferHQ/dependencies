/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QWINRTSERVICEPLUGIN_H
#define QWINRTSERVICEPLUGIN_H

#include <QtMultimedia/QMediaServiceProviderPlugin>

QT_BEGIN_NAMESPACE

class QWinRTServicePlugin : public QMediaServiceProviderPlugin
        , public QMediaServiceFeaturesInterface
        , public QMediaServiceCameraInfoInterface
        , public QMediaServiceSupportedDevicesInterface
        , public QMediaServiceDefaultDeviceInterface
{
    Q_OBJECT
    Q_INTERFACES(QMediaServiceFeaturesInterface)
    Q_INTERFACES(QMediaServiceCameraInfoInterface)
    Q_INTERFACES(QMediaServiceSupportedDevicesInterface)
    Q_INTERFACES(QMediaServiceDefaultDeviceInterface)
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.mediaserviceproviderfactory/5.0" FILE "winrt.json")
public:
    QMediaService *create(QString const &key);
    void release(QMediaService *service);

    QMediaServiceProviderHint::Features supportedFeatures(const QByteArray &service) const;

    QCamera::Position cameraPosition(const QByteArray &device) const Q_DECL_OVERRIDE;
    int cameraOrientation(const QByteArray &device) const Q_DECL_OVERRIDE;

    QList<QByteArray> devices(const QByteArray &service) const Q_DECL_OVERRIDE;
    QString deviceDescription(const QByteArray &service, const QByteArray &device) Q_DECL_OVERRIDE;

    QByteArray defaultDevice(const QByteArray &service) const Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // QWINRTSERVICEPLUGIN_H
