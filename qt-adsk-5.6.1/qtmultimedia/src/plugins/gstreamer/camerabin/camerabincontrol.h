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


#ifndef CAMERABINCONTROL_H
#define CAMERABINCONTROL_H

#include <QHash>
#include <qcameracontrol.h>
#include "camerabinsession.h"

QT_BEGIN_NAMESPACE

class CamerabinResourcePolicy;

class CameraBinControl : public QCameraControl
{
    Q_OBJECT
    Q_PROPERTY(bool viewfinderColorSpaceConversion READ viewfinderColorSpaceConversion WRITE setViewfinderColorSpaceConversion)
public:
    CameraBinControl( CameraBinSession *session );
    virtual ~CameraBinControl();

    bool isValid() const { return true; }

    QCamera::State state() const;
    void setState(QCamera::State state);

    QCamera::Status status() const;

    QCamera::CaptureModes captureMode() const;
    void setCaptureMode(QCamera::CaptureModes mode);

    bool isCaptureModeSupported(QCamera::CaptureModes mode) const;
    bool canChangeProperty(PropertyChangeType changeType, QCamera::Status status) const;
    bool viewfinderColorSpaceConversion() const;

    CamerabinResourcePolicy *resourcePolicy() { return m_resourcePolicy; }

public slots:
    void reloadLater();
    void setViewfinderColorSpaceConversion(bool enabled);

private slots:
    void delayedReload();

    void handleResourcesGranted();
    void handleResourcesLost();

    void handleBusyChanged(bool);
    void handleCameraError(int error, const QString &errorString);

private:
    void updateSupportedResolutions(const QString &device);

    CameraBinSession *m_session;
    QCamera::State m_state;
    CamerabinResourcePolicy *m_resourcePolicy;

    bool m_reloadPending;
};

QT_END_NAMESPACE

#endif // CAMERABINCONTROL_H
